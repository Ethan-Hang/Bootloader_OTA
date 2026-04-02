#include <stddef.h>
#include <string.h>

#include "stm32f4xx.h"
#include "iwdg.h"
#include "main.h"
#include "bootmanager.h"

#include "at24cxx_driver.h"
#include "w25qxx_Handler.h"

#include "elog.h"
#include "ymodem.h"

#include "Debug.h"

extern uint8_t       tab_1024[1024];
extern uint8_t       key_scan(void);
extern uint32_t g_jumpinit __attribute__((section("NO_INIT"), zero_init));
extern volatile bool elog_init_flag;

static const uint8_t s_iv_default[16] = {0x31, 0x32, 0x31, 0x32, 0x31, 0x32,
                                         0x31, 0x32, 0x31, 0x32, 0x31, 0x32,
                                         0x31, 0x32, 0x31, 0x32};

static const uint8_t s_key_256[32]    = {
    0x31, 0x32, 0x31, 0x32, 0x31, 0x32, 0x31, 0x32, 0x31, 0x32, 0x31,
    0x32, 0x31, 0x32, 0x31, 0x32, 0x31, 0x32, 0x31, 0x32, 0x31, 0x32,
    0x31, 0x32, 0x31, 0x32, 0x31, 0x32, 0x31, 0x32, 0x31, 0x32};

static uint8_t s_mem_read_buffer[4096];

#define OTA_IWDG_PRESCALER IWDG_Prescaler_64
#define OTA_IWDG_RELOAD    3000U

static bool s_iwdg_started = false;

static void ota_watchdog_feed(void)
{
    IWDG_ReloadCounter();
}

static void ota_watchdog_start(void)
{
    if (!s_iwdg_started)
    {
        IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
        IWDG_SetPrescaler(OTA_IWDG_PRESCALER);
        IWDG_SetReload(OTA_IWDG_RELOAD);
        IWDG_ReloadCounter();
        IWDG_Enable();
        s_iwdg_started = true;
        DEBUG_OUT(i, OTA_LOG_TAG, "OTA watchdog started");
    }

    ota_watchdog_feed();
}

static void soft_reset(void)
{
    __NVIC_SystemReset();
}

static int8_t ex_block_to_app(uint8_t block_index, const char *tag)
{
    uint32_t  flash_des        = ApplicationAddress;
    uint32_t  flash_size       = Read_BlockSize(block_index);
    uint16_t  read_memory_size = 0;
    uint8_t   read_state       = 0;
    uint32_t *word_ptr         = NULL;

    if (flash_size == 0U)
    {
        DEBUG_OUT(e, OTA_LOG_TAG, "%s: invalid block size 0", tag);
        return -1;
    }

    if (Flash_erase(flash_des, flash_size) != 0U)
    {
        DEBUG_OUT(e, OTA_LOG_TAG, "%s: internal flash erase failed", tag);
        return -1;
    }

    for (;;)
    {
        ota_watchdog_feed();

        read_state =
            W25Q64_ReadData(block_index, s_mem_read_buffer, &read_memory_size);

        if (read_state == 1U)
        {
            DEBUG_OUT(i, OTA_LOG_TAG, "%s: copy complete, size=%lu bytes", tag,
                      (unsigned long)flash_size);
            return 0;
        }

        if (read_state == 2U)
        {
            DEBUG_OUT(e, OTA_LOG_TAG, "%s: read external flash failed", tag);
            return -1;
        }

        if ((read_memory_size % 4U) != 0U)
        {
            DEBUG_OUT(e, OTA_LOG_TAG, "%s: read size %u is not 4-byte aligned",
                      tag, read_memory_size);
            return -1;
        }

        word_ptr = (uint32_t *)s_mem_read_buffer;
        for (uint16_t i = 0U; i < (read_memory_size / 4U); i++)
        {
            Flash_Write(flash_des, word_ptr[i]);
            flash_des += 4U;

            if ((i & 0x3FU) == 0U)
            {
                ota_watchdog_feed();
            }
        }
    }
}

void ota_apply_update(int32_t file_size, bool first_boot)
{
    uint32_t current_app_size = 0;

    ota_watchdog_feed();

    if (exA_to_exB_AES(file_size) != 0)
    {
        DEBUG_OUT(e, OTA_LOG_TAG, "ota_apply_update: exA_to_exB_AES failed");
        if (!first_boot)
        {
            uint8_t ee_status = EE_OTA_NO_APP_UPDATE;
            ee_WriteBytes((uint8_t *)&ee_status, 0x00, 1);
            jump_to_app();
        }
        return;
    }

    if (!first_boot)
    {
        ota_watchdog_feed();

        if (ee_ReadBytes((uint8_t *)&current_app_size, 0x05, 4) == 0)
        {
            DEBUG_OUT(
                w, OTA_LOG_TAG,
                "ota_apply_update: read current app size failed, use fallback");
            current_app_size = 0U;
        }

        if ((current_app_size == 0U) || (current_app_size == 0xFFFFFFFFU) ||
            (current_app_size > (uint32_t)(0x18010 - 1)))
        {
            if ((file_size > 0) && (file_size <= (int32_t)(0x18010 - 1)))
            {
                current_app_size = (uint32_t)file_size;
                DEBUG_OUT(
                    w, OTA_LOG_TAG,
                    "ota_apply_update: fallback current app size=%lu bytes",
                    (unsigned long)current_app_size);
            }
        }

        if ((current_app_size > 0U) &&
            (current_app_size <= (uint32_t)(0x18010 - 1)))
        {
            DEBUG_OUT(d, OTA_LOG_TAG,
                      "Current app size read from EEPROM: %lu bytes",
                      (unsigned long)current_app_size);

            if (app_to_exA(current_app_size) != 0)
            {
                DEBUG_OUT(w, OTA_LOG_TAG,
                          "ota_apply_update: backup current app failed");
            }
        }
        else
        {
            DEBUG_OUT(w, OTA_LOG_TAG,
                      "ota_apply_update: skip backup due to invalid app size");
        }
    }

    ota_watchdog_feed();

    if (exB_to_app() != 0)
    {
        DEBUG_OUT(e, OTA_LOG_TAG, "ota_apply_update: apply new app failed");
        if (!first_boot)
        {
            uint8_t ee_status = EE_OTA_NO_APP_UPDATE;
            ee_WriteBytes((uint8_t *)&ee_status, 0x00, 1);
        }
        jump_to_app();
        return;
    }

    {
        uint8_t ee_status = EE_OTA_APP_CHECK_START;
        if (ee_WriteBytes((uint8_t *)&ee_status, 0x00, 1) == 0)
        {
            DEBUG_OUT(e, OTA_LOG_TAG,
                      "ota_apply_update: write OTA check start failed");
            jump_to_app();
            return;
        }
    }

    if ((file_size > 0) && (file_size <= (int32_t)(0x18010 - 1)))
    {
        uint32_t new_app_size = (uint32_t)file_size;
        if (ee_WriteBytes((uint8_t *)&new_app_size, 0x05, 4) == 0)
        {
            DEBUG_OUT(w, OTA_LOG_TAG,
                      "ota_apply_update: write current app size failed");
        }
    }

    jump_to_app();

    // no valid app to run, use backup if available
    if (!first_boot)
    {
        ota_watchdog_feed();

        if (exA_to_app() != 0)
        {
            DEBUG_OUT(e, OTA_LOG_TAG, "ota_apply_update: rollback copy failed");
            return;
        }
    }

    jump_to_app();
}

static void disable_all_peripherals(void)
{
    __disable_irq();

    elog_stop();
    elog_deinit();
    // TIM_DeInit(TIM3);

    USART_DeInit(USART1);
    GPIO_DeInit(GPIOA);

    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL  = 0U;

    for (uint32_t i = 0U; i < 8U; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }

    RCC_DeInit();
}

void jump_to_app(void)
{
    uint32_t  sp            = *(__IO uint32_t *)(ApplicationAddress);
    uint32_t  jump_addr     = *(__IO uint32_t *)(ApplicationAddress + 4U);
    pFunction jump_app_func = NULL;

    if ((sp & 0x2FFE0000U) == 0x20000000U)
    {
        disable_all_peripherals();

        NVIC_SetVectorTable(NVIC_VectTab_FLASH,
                            (ApplicationAddress - NVIC_VectTab_FLASH));

        __set_MSP(sp);

        jump_app_func = (pFunction)jump_addr;
        jump_app_func();
    }
    else
    {
        if (elog_init_flag)
        {
            DEBUG_OUT(
                e, OTA_LOG_TAG,
                "jump_to_app: invalid stack pointer 0x%08lX at app 0x%08lX",
                (unsigned long)sp, (unsigned long)ApplicationAddress);
        }
    }
}

int8_t exA_to_exB_AES(int32_t fl_size)
{
    uint8_t  temp[16];
    uint8_t  iv_work[16];
    uint16_t read_time        = 0;
    uint16_t read_data_count  = 0;
    uint32_t app_size         = 0;
    uint16_t read_memory_size = 0;
    uint32_t read_memory_idx  = 0;
    uint8_t  read_state       = 0;
    uint32_t decoded_bytes    = 0;
    uint32_t progress_step    = 0;
    uint32_t progress_last    = 0;

    if ((fl_size <= 0) || (fl_size > (int32_t)(0x18010 - 1)))
    {
        DEBUG_OUT(e, OTA_LOG_TAG, "exA_to_exB_AES: invalid input size=%ld",
                  (long)fl_size);
        return -1;
    }

    /* Rebuild BLOCK_1 read/write context in case RAM state was reset. */
    SetBlockParmeter(BLOCK_1, (uint32_t)fl_size);

    ota_watchdog_feed();

    memcpy(iv_work, s_iv_default, sizeof(iv_work));

    read_state = W25Q64_ReadData(BLOCK_1, s_mem_read_buffer, &read_memory_size);
    if ((read_state != 0U) || (read_memory_size < 16U))
    {
        DEBUG_OUT(e, OTA_LOG_TAG,
                  "exA_to_exB_AES: first read failed, state=%u size=%u",
                  read_state, read_memory_size);
        return -1;
    }

    memcpy(temp, s_mem_read_buffer, 16);
    Aes_IV_key256bit_Decode(iv_work, temp, (uint8_t *)s_key_256);

    app_size = ((uint32_t)temp[15] << 24) | ((uint32_t)temp[14] << 16) |
               ((uint32_t)temp[13] << 8) | (uint32_t)temp[12];

    DEBUG_OUT(d, OTA_LOG_TAG, "exA_to_exB_AES: decoded AppSize=%lu, input=%ld",
              (unsigned long)app_size, (long)fl_size);

    if ((app_size == 0U) || (app_size > (uint32_t)fl_size))
    {
        DEBUG_OUT(e, OTA_LOG_TAG, "exA_to_exB_AES: decoded AppSize invalid");
        return -1;
    }

    read_data_count = (uint16_t)(app_size / 16U);
    if ((app_size % 16U) != 0U)
    {
        read_data_count++;
    }

    DEBUG_OUT(i, OTA_LOG_TAG,
              "exA_to_exB_AES: decrypt progress 0%% (0/%lu bytes)",
              (unsigned long)app_size);

    read_memory_idx = 16U;

    ota_watchdog_feed();
    Erase_Flash_Block(BLOCK_2);

    for (read_time = 0; read_time < read_data_count; read_time++)
    {
        if ((read_time & 0x3FU) == 0U)
        {
            ota_watchdog_feed();
        }

        if (read_memory_idx == read_memory_size)
        {
            read_state =
                W25Q64_ReadData(BLOCK_1, s_mem_read_buffer, &read_memory_size);
            if (read_state != 0U)
            {
                DEBUG_OUT(e, OTA_LOG_TAG,
                          "exA_to_exB_AES: block read failed at %u", read_time);
                return -1;
            }
            read_memory_idx = 0U;
        }

        if ((read_memory_idx + 16U) > read_memory_size)
        {
            DEBUG_OUT(e, OTA_LOG_TAG,
                      "exA_to_exB_AES: frame overflow idx=%lu size=%u",
                      (unsigned long)read_memory_idx, read_memory_size);
            return -1;
        }

        memcpy(temp, s_mem_read_buffer + read_memory_idx, 16);
        read_memory_idx += 16U;
        Aes_IV_key256bit_Decode(iv_work, temp, (uint8_t *)s_key_256);

        W25Q64_WriteData(BLOCK_2, temp, 16);

        if ((app_size - decoded_bytes) >= 16U)
        {
            decoded_bytes += 16U;
        }
        else
        {
            decoded_bytes = app_size;
        }

        progress_step = (decoded_bytes * 10U) / app_size;
        if (progress_step > progress_last)
        {
            progress_last = progress_step;
            DEBUG_OUT(i, OTA_LOG_TAG,
                      "exA_to_exB_AES: decrypt progress %lu%% (%lu/%lu bytes)",
                      (unsigned long)(progress_last * 10U),
                      (unsigned long)decoded_bytes, (unsigned long)app_size);
        }
    }

    W25Q64_WriteData_End(BLOCK_2);
    DEBUG_OUT(i, OTA_LOG_TAG,
              "exA_to_exB_AES: decode and copy done, AppSize=%lu",
              (unsigned long)app_size);

    return 0;
}

int8_t app_to_exA(uint32_t fl_size)
{
    DEBUG_OUT(d, OTA_LOG_TAG,
              "Start to backup current app to external flash A, size=%lu bytes",
              (unsigned long)fl_size);
    if ((fl_size == 0U) || (fl_size > (uint32_t)(0x18010 - 1)))
    {
        DEBUG_OUT(e, OTA_LOG_TAG, "app_to_exA: invalid app size=%lu",
                  (unsigned long)fl_size);
        return -1;
    }

    ota_watchdog_feed();
    Erase_Flash_Block(BLOCK_1);
    W25Q64_WriteData(BLOCK_1, (uint8_t *)ApplicationAddress, fl_size);
    W25Q64_WriteData_End(BLOCK_1);
    return 0;
}

int8_t exB_to_app(void)
{
    DEBUG_OUT(d, OTA_LOG_TAG, "Start to copy external flash B to app");

    return ex_block_to_app(BLOCK_2, "exB_to_app");
}

int8_t exA_to_app(void)
{
    DEBUG_OUT(d, OTA_LOG_TAG, "Start to copy external flash A to app");

    return ex_block_to_app(BLOCK_1, "exA_to_app");
}

void OTA_StateManager(void)
{
    uint8_t         ota_state      = EE_OTA_NO_APP_UPDATE;
    uint32_t        app_size       = 0;
    int32_t         file_size      = 0;
    static uint16_t send_wait_time = 0;

    if (ee_ReadBytes(&ota_state, 0x00, 1) == 0)
    {
        DEBUG_OUT(e, OTA_LOG_TAG, "OTA_StateManager: read OTA state failed");
        return;
    }

    if (0 == send_wait_time)
    {
        DEBUG_OUT(i, OTA_LOG_TAG, "OTA state=0x%02X", ota_state);
    }

    ota_watchdog_feed();

    switch (ota_state)
    {
    case EE_INIT_NO_APP:
        DEBUG_OUT(i, OTA_LOG_TAG,
                  "No app in flash, wait for key press to download");
        if (key_scan())
        {
            file_size = Ymodem_Receive(tab_1024);

            if (file_size <= 0)
            {
                DEBUG_OUT(e, OTA_LOG_TAG, "OTA download failed, file_size=%ld",
                          (long)file_size);
                break;
            }

            ota_apply_update(file_size, true);
        }
        else
        {
            send_wait_time++;
            if (send_wait_time >= 20000U)
            {
                DEBUG_OUT(
                    w, OTA_LOG_TAG,
                    "No app and no update, wait for key press to download");
                send_wait_time = 0U;
            }
        }
        break;
    case EE_OTA_NO_APP_UPDATE:
        if (key_scan())
        {
            file_size = Ymodem_Receive(tab_1024);

            if (file_size <= 0)
            {
                DEBUG_OUT(e, OTA_LOG_TAG, "OTA download failed, file_size=%ld",
                          (long)file_size);
                jump_to_app();
                break;
            }

            ota_apply_update(file_size, false);
        }
        else
        {
            // Set flag for main to jump to app
            g_jumpinit = 0x55AA55AA;
            soft_reset();
        }
        break;

    case EE_OTA_DOWNLOADING:
        DEBUG_OUT(w, OTA_LOG_TAG, "OTA previous downloading interrupted");
        jump_to_app();

        file_size = Ymodem_Receive(tab_1024);
        ota_apply_update(file_size, false);
        break;

    case EE_OTA_DOWNLOAD_FINISHED:
        if (ee_ReadBytes((uint8_t *)&app_size, 0x01, 4) == 0)
        {
            DEBUG_OUT(e, OTA_LOG_TAG,
                      "OTA_StateManager: read downloaded app size failed");
            jump_to_app();
            break;
        }

        if (app_size == 0U)
        {
            DEBUG_OUT(e, OTA_LOG_TAG,
                      "OTA_StateManager: downloaded app size is 0");
            jump_to_app();
            break;
        }

        SetBlockParmeter(BLOCK_1, app_size);
        ota_apply_update((int32_t)app_size, false);
        break;
    case EE_OTA_APP_CHECK_START:
        ota_state = EE_OTA_APP_CHECKING;
        ee_WriteBytes(&ota_state, 0x00, 1);

        ota_watchdog_start();
        jump_to_app();
        break;
    case EE_OTA_APP_CHECKING:
        ota_state = EE_OTA_NO_APP_UPDATE;
        ee_WriteBytes(&ota_state, 0x00, 1);

        ota_watchdog_start();
        soft_reset();
        break;
    default:
        DEBUG_OUT(e, OTA_LOG_TAG, "OTA_StateManager: unknown state 0x%02X",
                  ota_state);
        break;
    }
}
