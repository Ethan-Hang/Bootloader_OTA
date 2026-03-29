#include <stddef.h>
#include <stdlib.h>
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

unsigned char  IV[16]  = {0x31, 0X32, 0x31, 0X32, 0x31, 0X32, 0x31, 0X32,
                          0x31, 0X32, 0x31, 0X32, 0x31, 0X32, 0x31, 0X32};
unsigned char  Key[32] = {0x31, 0X32, 0x31, 0X32, 0x31, 0X32, 0x31, 0X32,
                          0x31, 0X32, 0x31, 0X32, 0x31, 0X32, 0x31, 0X32,
                          0x31, 0X32, 0x31, 0X32, 0x31, 0X32, 0x31, 0X32,
                          0x31, 0X32, 0x31, 0X32, 0x31, 0X32, 0x31, 0X32};

uint8_t        Mem_Read_buffer[4096];

static int8_t  ex_block_to_app(uint8_t block_index, const char *tag)
{
    uint32_t flash_des              = ApplicationAddress;
    uint32_t flash_size             = 0;
    uint16_t read_memory_size       = 0;
    uint32_t w25q_read_status       = 0;
    uint32_t inter_flash_ram_source = 0;

    flash_size                      = Read_BlockSize(block_index);
    if (flash_size == 0)
    {
        DEBUG_OUT(e, "", "%s: Invalid block size 0", tag);
        return -1;
    }

    if (1 == Flash_erase(flash_des, flash_size))
    {
        DEBUG_OUT(e, "", "%s: Internal flash erase failed", tag);
        return -1;
    }

    for (;;)
    {
        w25q_read_status =
            W25Q64_ReadData(block_index, Mem_Read_buffer, &read_memory_size);

        if (1 == w25q_read_status)
        {
            DEBUG_OUT(i, "", "%s: Read complete, total size: 0x%08X (%u bytes)",
                      tag, flash_size, flash_size);
            return (int8_t)flash_size;
        }
        else if (2 == w25q_read_status)
        {
            DEBUG_OUT(e, "", "%s: Read error from external flash", tag);
            return -1;
        }
        else
        {
            inter_flash_ram_source = (uint32_t)Mem_Read_buffer;
            for (uint16_t write_time = 0; write_time < read_memory_size / 4;
                 write_time++)
            {
                Flash_Write(flash_des, inter_flash_ram_source);
                flash_des += 4;
                inter_flash_ram_source += 4;
            }
        }
    }
}

void ota_apply_update(int32_t file_size)
{
    uint32_t current_app_size = 0;

    if (0 == exA_to_exB_AES(file_size))
    {
        if (0 == ee_ReadBytes((uint8_t *)&current_app_size, 0x05, 4))
        {
            DEBUG_OUT(e, "", "ota_apply_update: read current app size failed");
            jump_to_app();
            return;
        }

        app_to_exA(current_app_size);
        exB_to_app();
        jump_to_app();

        exA_to_app();
        jump_to_app();
    }
    else
    {
        DEBUG_OUT(a, "", "Boot download failed");
        jump_to_app();
    }
}

void disable_all_peripherals(void)
{
    __disable_irq();

    elog_stop();
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
    __IO uint32_t sp            = *(__IO uint32_t *)(ApplicationAddress);
    uint32_t      jump_addr     = 0x00;
    pFunction     jump_app_func = NULL;

    delay_ms(100);

    if (0x20000000 < sp && sp < 0x20020000)
    {
        disable_all_peripherals();

        jump_addr = *(__IO uint32_t *)(ApplicationAddress + 4);

        __set_MSP(sp);

        jump_app_func = (pFunction)jump_addr;
        jump_app_func();
    }
    else
    {
        DEBUG_OUT(
            w, "",
            "jump_to_app: Invalid stack pointer 0x%08X (not in SRAM range)",
            sp);
        DEBUG_OUT(
            w, "",
            "No valid application found at address 0x%08X,\\r\\n Please check "
            "the application or press key to receive new application",
            ApplicationAddress);
    }
}

int8_t exA_to_exB_AES(int32_t fl_size)
{
    u8       Temp[16];
    u16      readTime          = 0;
    u16      readDataCount     = 0;
    u32      AppSize           = 0;
    u16      Read_Memory_Size  = 0;
    u32      Read_Memory_index = 0;
    uint8_t *pu8_IV_IN_OUT     = IV;
    uint8_t *pu8_key256bit     = Key;
    uint32_t AppRunDestination = ApplicationAddress;

    DEBUG_OUT(d, "", "back_to_app: Starting OTA decryption, fl_size=%d",
              fl_size);

    if ((fl_size > (0x18010 - 1)) || (fl_size <= 0))
    {
        DEBUG_OUT(
            e, "",
            "back_to_app: fl_size exceeds limit or invalid: %d (max=0x%x)",
            fl_size, 0x18010 - 1);
        return -1;
    }

    DEBUG_OUT(d, "", "back_to_app: Reading first frame from external flash");
    W25Q64_ReadData(BLOCK_1, Mem_Read_buffer, &Read_Memory_Size);
    if (Read_Memory_Size >= 16)
    {
        memcpy(Temp, Mem_Read_buffer, 16);
        Aes_IV_key256bit_Decode(pu8_IV_IN_OUT, Temp, pu8_key256bit);
        AppSize =
            (Temp[15] << 24) + (Temp[14] << 16) + (Temp[13] << 8) + Temp[12];
        DEBUG_OUT(d, "",
                  "back_to_app: Decrypted header, AppSize=0x%08X (%u bytes)",
                  AppSize, AppSize);

        readDataCount = AppSize / 16;
        if (AppSize % 16 != 0)
        {
            readDataCount += 1;
        }
        DEBUG_OUT(d, "", "back_to_app: Will decrypt %u blocks (16 bytes each)",
                  readDataCount);
        Read_Memory_index += 16;
    }
    else
    {
        DEBUG_OUT(e, "",
                  "back_to_app: Read_Memory_Size too small: %u (need >= 16)",
                  Read_Memory_Size);
        return -1;
    }

    DEBUG_OUT(d, "", "back_to_app: Erasing flash at 0x%08X, size=0x%x",
              AppRunDestination, AppSize);
    if (0 == Flash_erase(AppRunDestination, AppSize))
    {
        DEBUG_OUT(d, "", "back_to_app: Flash erase completed");
        for (readTime = 0; readTime < readDataCount; readTime++)
        {
            if (Read_Memory_index == Read_Memory_Size)
            {
                DEBUG_OUT(d, "",
                          "back_to_app: Reading next frame from external flash "
                          "(block %u/%u)",
                          readTime, readDataCount);
                if (2 == W25Q64_ReadData(BLOCK_1, Mem_Read_buffer,
                                         &Read_Memory_Size))
                {
                    DEBUG_OUT(
                        e, "",
                        "back_to_app: Read extern buffer error at block %u",
                        readTime);
                    return -1;
                }
                Read_Memory_index = 0;
            }

            memcpy(Temp, Mem_Read_buffer + Read_Memory_index, 16);
            Read_Memory_index += 16;
            Aes_IV_key256bit_Decode(pu8_IV_IN_OUT, Temp, pu8_key256bit);

            W25Q64_WriteData(BLOCK_2, Temp, 16);

            if (readTime % 64 == 0)
            {
                DEBUG_OUT(d, "",
                          "back_to_app: Decryption progress %u/%u blocks",
                          readTime, readDataCount);
            }
        }
        W25Q64_WriteData_End(BLOCK_2);

        DEBUG_OUT(
            i, "",
            "back_to_app: OTA decryption completed, total size: 0x%08X (%u "
            "bytes)",
            AppSize, AppSize);
        return 0;
    }
    else
    {
        DEBUG_OUT(e, "", "back_to_app: Flash erase failed at address 0x%08X",
                  AppRunDestination);
        return -1;
    }
}

int8_t app_to_exA(uint32_t fl_size)
{
    uint32_t flash_des = ApplicationAddress;

    // if (fl_size > (0x18000 - 1))
    // {
    //     log_e("app_to_exA: Invalid fl_size: %d (max=0x%x)", fl_size,
    //           0x18000 - 1);
    //     return -1;
    // }

    Erase_Flash_Block(BLOCK_1);
    W25Q64_WriteData(BLOCK_1, (uint8_t *)flash_des, fl_size);
    W25Q64_WriteData_End(BLOCK_1);
    return 0;
}

int8_t exB_to_app(void)
{
    return ex_block_to_app(BLOCK_2, "exB_to_app");
}

int8_t exA_to_app(void)
{
    return ex_block_to_app(BLOCK_1, "exA_to_app");
}

void OTA_StateManager(void)
{
    uint8_t  ota_state = EE_OTA_EMPTY;
    uint32_t app_size  = 0;
    int32_t  file_size = 0;

    if (0 == ee_ReadBytes(&ota_state, 0x00, 1))
    {
        DEBUG_OUT(e, "", "OTA_StateManager: read OTA state from EEPROM failed");
        return;
    }

    switch (ota_state)
    {
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
        DEBUG_OUT(a, "", "App download failed");
        jump_to_app();
        DEBUG_OUT(a, "",
                  "No valid application found, waiting for new application...");

        file_size = Ymodem_Receive(tab_1024);
        ota_apply_update(file_size);
        break;

    case EE_OTA_DOWNLOAD_FINISHED:
        if (0 == ee_ReadBytes((uint8_t *)&app_size, 0x01, 4))
        {
            DEBUG_OUT(e, "",
                      "OTA_StateManager: read downloaded app size failed");
            jump_to_app();
            break;
        }

        SetBlockParmeter(BLOCK_1, app_size);
        ota_apply_update((int32_t)app_size);
        break;

    default:
        break;
    }
}
