#include <stddef.h>
#include <string.h>

#include "stm32f4xx.h"

#include "bootmanager.h"
#include "elog.h"
#include "../../../Debug/inc/Debug.h"
#include "aes.h"
#include "w25qxx_Handler.h"
#include "at24cxx_driver.h"
#include "ymodem.h"

extern uint8_t tab_1024[1024];
extern uint8_t key_scan(void);

static const uint8_t s_iv_default[16] = {
    0x31, 0x32, 0x31, 0x32, 0x31, 0x32, 0x31, 0x32,
    0x31, 0x32, 0x31, 0x32, 0x31, 0x32, 0x31, 0x32};

static const uint8_t s_key_256[32] = {
    0x31, 0x32, 0x31, 0x32, 0x31, 0x32, 0x31, 0x32,
    0x31, 0x32, 0x31, 0x32, 0x31, 0x32, 0x31, 0x32,
    0x31, 0x32, 0x31, 0x32, 0x31, 0x32, 0x31, 0x32,
    0x31, 0x32, 0x31, 0x32, 0x31, 0x32, 0x31, 0x32};

static uint8_t s_mem_read_buffer[4096];

static int8_t ex_block_to_app(uint8_t block_index, const char *tag)
{
    uint32_t  flash_des        = ApplicationAddress;
    uint32_t  flash_size       = Read_BlockSize(block_index);
    uint16_t  read_memory_size = 0;
    uint8_t   read_state       = 0;
    uint32_t *word_ptr         = NULL;

    if (flash_size == 0U)
    {
        DEBUG_OUT(e, "", "%s: invalid block size 0", tag);
        return -1;
    }

    if (Flash_erase(flash_des, flash_size) != 0U)
    {
        DEBUG_OUT(e, "", "%s: internal flash erase failed", tag);
        return -1;
    }

    for (;;)
    {
        read_state =
            W25Q64_ReadData(block_index, s_mem_read_buffer, &read_memory_size);

        if (read_state == 1U)
        {
            DEBUG_OUT(i, "", "%s: copy complete, size=%lu bytes", tag,
                      (unsigned long)flash_size);
            return 0;
        }

        if (read_state == 2U)
        {
            DEBUG_OUT(e, "", "%s: read external flash failed", tag);
            return -1;
        }

        if ((read_memory_size % 4U) != 0U)
        {
            DEBUG_OUT(e, "", "%s: read size %u is not 4-byte aligned", tag,
                      read_memory_size);
            return -1;
        }

        word_ptr = (uint32_t *)s_mem_read_buffer;
        for (uint16_t i = 0U; i < (read_memory_size / 4U); i++)
        {
            Flash_Write(flash_des, word_ptr[i]);
            flash_des += 4U;
        }
    }
}

void ota_apply_update(int32_t file_size)
{
    uint32_t current_app_size = 0;

    if (exA_to_exB_AES(file_size) != 0)
    {
        DEBUG_OUT(e, "", "ota_apply_update: exA_to_exB_AES failed");
        jump_to_app();
        return;
    }

    if (ee_ReadBytes((uint8_t *)&current_app_size, 0x01, 4) == 0)
    {
        DEBUG_OUT(e, "", "ota_apply_update: read current app size failed");
        jump_to_app();
        return;
    }
    DEBUG_OUT(d, "", "Current app size read from EEPROM: %lu bytes",
              (unsigned long)current_app_size);

    if (app_to_exA(current_app_size) != 0)
    {
        DEBUG_OUT(w, "", "ota_apply_update: backup current app failed");
    }

    if (exB_to_app() != 0)
    {
        DEBUG_OUT(e, "", "ota_apply_update: apply new app failed");
        jump_to_app();
        return;
    }

    jump_to_app();

    if (exA_to_app() != 0)
    {
        DEBUG_OUT(e, "", "ota_apply_update: rollback copy failed");
        return;
    }

    jump_to_app();
}

static void disable_all_peripherals(void)
{
    __disable_irq();

    //elog_stop();
    elog_deinit();

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

    delay_ms(100);

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
        DEBUG_OUT(
            e, "",
            "jump_to_app: invalid stack pointer 0x%08lX at app 0x%08lX",
            (unsigned long)sp, (unsigned long)ApplicationAddress);
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
        DEBUG_OUT(e, "", "exA_to_exB_AES: invalid input size=%ld",
                  (long)fl_size);
        return -1;
    }

    memcpy(iv_work, s_iv_default, sizeof(iv_work));

    read_state = W25Q64_ReadData(BLOCK_1, s_mem_read_buffer, &read_memory_size);
    if ((read_state != 0U) || (read_memory_size < 16U))
    {
        DEBUG_OUT(e, "", "exA_to_exB_AES: first read failed, state=%u size=%u",
                  read_state, read_memory_size);
        return -1;
    }

    memcpy(temp, s_mem_read_buffer, 16);
    Aes_IV_key256bit_Decode(iv_work, temp, (uint8_t *)s_key_256);

    app_size = ((uint32_t)temp[15] << 24) | ((uint32_t)temp[14] << 16) |
               ((uint32_t)temp[13] << 8) | (uint32_t)temp[12];

    DEBUG_OUT(d, "", "exA_to_exB_AES: decoded AppSize=%lu, input=%ld",
              (unsigned long)app_size, (long)fl_size);

    if ((app_size == 0U) || (app_size > (uint32_t)fl_size))
    {
        DEBUG_OUT(e, "", "exA_to_exB_AES: decoded AppSize invalid");
        return -1;
    }

    read_data_count = (uint16_t)(app_size / 16U);
    if ((app_size % 16U) != 0U)
    {
        read_data_count++;
    }

    DEBUG_OUT(i, "", "exA_to_exB_AES: decrypt progress 0%% (0/%lu bytes)",
              (unsigned long)app_size);

    read_memory_idx = 16U;

    Erase_Flash_Block(BLOCK_2);

    for (read_time = 0; read_time < read_data_count; read_time++)
    {
        if (read_memory_idx == read_memory_size)
        {
            read_state =
                W25Q64_ReadData(BLOCK_1, s_mem_read_buffer, &read_memory_size);
            if (read_state != 0U)
            {
                DEBUG_OUT(e, "", "exA_to_exB_AES: block read failed at %u",
                          read_time);
                return -1;
            }
            read_memory_idx = 0U;
        }

        if ((read_memory_idx + 16U) > read_memory_size)
        {
            DEBUG_OUT(e, "", "exA_to_exB_AES: frame overflow idx=%lu size=%u",
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
            DEBUG_OUT(i, "",
                      "exA_to_exB_AES: decrypt progress %lu%% (%lu/%lu bytes)",
                      (unsigned long)(progress_last * 10U),
                      (unsigned long)decoded_bytes, (unsigned long)app_size);
        }
    }

    W25Q64_WriteData_End(BLOCK_2);
    DEBUG_OUT(i, "", "exA_to_exB_AES: decode and copy done, AppSize=%lu",
              (unsigned long)app_size);

    return 0;
}

int8_t app_to_exA(uint32_t fl_size)
{
    DEBUG_OUT(d, "", "Start to backup current app to external flash A, size=%lu bytes",
              (unsigned long)fl_size);
    if ((fl_size == 0U) || (fl_size > (uint32_t)(0x18010 - 1)))
    {
        DEBUG_OUT(e, "", "app_to_exA: invalid app size=%lu",
                  (unsigned long)fl_size);
        return -1;
    }

    Erase_Flash_Block(BLOCK_1);
    W25Q64_WriteData(BLOCK_1, (uint8_t *)ApplicationAddress, fl_size);
    W25Q64_WriteData_End(BLOCK_1);
    return 0;
}

int8_t exB_to_app(void)
{
    DEBUG_OUT(d, "", "Start to copy external flash B to app");

    return ex_block_to_app(BLOCK_2, "exB_to_app");
}

int8_t exA_to_app(void)
{
    DEBUG_OUT(d, "", "Start to copy external flash A to app");

    return ex_block_to_app(BLOCK_1, "exA_to_app");
}

void OTA_StateManager(void)
{
    uint8_t  ota_state = EE_OTA_EMPTY;
    uint32_t app_size  = 0;
    int32_t  file_size = 0;
    static uint16_t send_wait_time = 0;

    if (ee_ReadBytes(&ota_state, 0x00, 1) == 0)
    {
        DEBUG_OUT(e, "", "OTA_StateManager: read OTA state failed");
        return;
    }

    if (0 == send_wait_time)
    {
        DEBUG_OUT(i, "", "OTA state=0x%02X", ota_state);
    }

    switch (ota_state)
    {
    case EE_INIT_NO_APP:
        if (key_scan())
        {
            file_size = Ymodem_Receive(tab_1024);
            ota_apply_update(file_size);
        }
        else
        {
            send_wait_time++;
            if (send_wait_time >= 20000U)
            {
                DEBUG_OUT(w, "", "No app and no update, wait for key press to download");
                send_wait_time = 0U;
            }
        }
        break;        
    case EE_OTA_EMPTY:
        if (key_scan())
        {
            file_size = Ymodem_Receive(tab_1024);
            ota_apply_update(file_size);
        }
        else
        {
            jump_to_app();
        }
        break;

    case EE_OTA_DOWNLOADING:
        DEBUG_OUT(w, "", "OTA previous downloading interrupted");
        jump_to_app();

        file_size = Ymodem_Receive(tab_1024);
        ota_apply_update(file_size);
        break;

    case EE_OTA_DOWNLOAD_FINISHED:
        if (ee_ReadBytes((uint8_t *)&app_size, 0x01, 4) == 0)
        {
            DEBUG_OUT(e, "", "OTA_StateManager: read downloaded app size failed");
            jump_to_app();
            break;
        }

        if (app_size == 0U)
        {
            DEBUG_OUT(e, "", "OTA_StateManager: downloaded app size is 0");
            jump_to_app();
            break;
        }

        SetBlockParmeter(BLOCK_1, app_size);
        ota_apply_update((int32_t)app_size);
        break;        

    default:
        DEBUG_OUT(e, "", "OTA_StateManager: unknown state 0x%02X", ota_state);
        break;
    }
}
