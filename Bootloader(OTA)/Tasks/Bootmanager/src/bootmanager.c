#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "stm32f4xx.h"

#include "bootmanager.h"
#include "elog.h"
#include "aes.h"
#include "w25qxx_Handler.h"

void disable_all_peripherals(void)
{
    __disable_irq();

    elog_stop();
    elog_deinit();
    USART_DeInit(USART1);
    GPIO_DeInit(GPIOA);

    /* Stop SysTick to avoid bootloader tick interrupt after jump. */
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL  = 0U;

    /* Disable and clear all NVIC interrupt lines. */
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

    // wait for log output to complete before jumping to application
    delay_ms(100);

    if (0x20000000 < sp && sp < 0x20020000)
    {
        disable_all_peripherals();

        // Reset Handler
        jump_addr = *(__IO uint32_t *)(ApplicationAddress + 4);

        __set_MSP(sp);

        jump_app_func = (pFunction)jump_addr;
        jump_app_func();
    }
    else
    {
        log_w("jump_to_app: Invalid stack pointer 0x%08X (not in SRAM range)",
              sp);
        log_w("No valid application found at address 0x%08X,\r\n Please check "
              "the "
              "application or press key to receive new application",
              ApplicationAddress);
    }
}

unsigned char IV[16]  = {0x31, 0X32, 0x31, 0X32, 0x31, 0X32, 0x31, 0X32,
                         0x31, 0X32, 0x31, 0X32, 0x31, 0X32, 0x31, 0X32};
unsigned char Key[32] = {0x31, 0X32, 0x31, 0X32, 0x31, 0X32, 0x31, 0X32,
                         0x31, 0X32, 0x31, 0X32, 0x31, 0X32, 0x31, 0X32,
                         0x31, 0X32, 0x31, 0X32, 0x31, 0X32, 0x31, 0X32,
                         0x31, 0X32, 0x31, 0X32, 0x31, 0X32, 0x31, 0X32};

uint8_t Mem_Read_buffer[4096]; // 4KB数据缓冲区

int8_t back_to_app(int32_t fl_size)
{
    u8  Temp[16];                   // 原密文数据缓存
    u8  wirteTime              = 0; // 一个解析包写入次数
    u16 readTime               = 0,
        readDataCount          = 0; // 读取数据再解密的次数（每次解密16个字节）
    u32      AppSize           = 0; // 升级包的大小
    // u32 FlashDestination=ApplicationAddress;
    u16      Read_Memory_Size  = 0;
    u32      Read_Memory_index = 0;
    uint8_t *pu8_IV_IN_OUT     = IV;
    uint8_t *pu8_key256bit     = Key;
    uint32_t RamSource         = 0;
    uint32_t AppRunFlashDestination = ApplicationAddress;

    log_d("back_to_app: Starting OTA decryption, fl_size=%d", fl_size);

    if ((fl_size > (0x18010 - 1)) || (fl_size <= 0))
    {
        log_e("back_to_app: fl_size exceeds limit or invalid: %d (max=0x%x)",
              fl_size, 0x18010 - 1);
        return -1;
    }
    // 先读一帧，用来解析头文件格式
    log_d("back_to_app: Reading first frame from external flash");
    W25Q64_ReadData(Mem_Read_buffer, &Read_Memory_Size);
    if (Read_Memory_Size >= 16)
    {
        memcpy(Temp, Mem_Read_buffer, 16);
        Aes_IV_key256bit_Decode(pu8_IV_IN_OUT, Temp,
                                pu8_key256bit); // 解析得到自定义内容+文件大小
        AppSize =
            (Temp[15] << 24) + (Temp[14] << 16) + (Temp[13] << 8) + Temp[12];
        log_d("back_to_app: Decrypted header, AppSize=0x%08X (%u bytes)",
              AppSize, AppSize);

        // 计算升级包读取次数
        readDataCount = AppSize / 16;
        if (AppSize % 16 != 0)
        {
            readDataCount += 1;
        }
        log_d("back_to_app: Will decrypt %u blocks (16 bytes each)",
              readDataCount);
        Read_Memory_index += 16;
    }
    else
    {
        log_e("back_to_app: Read_Memory_Size too small: %u (need >= 16)",
              Read_Memory_Size);
        return -1;
    }

    // 数据帧
    // 将待写入区的内容擦除
    log_d("back_to_app: Erasing flash at 0x%08X, size=0x%x",
          AppRunFlashDestination, AppSize);
    uint8_t flash_erase_state = Flash_erase(AppRunFlashDestination, AppSize);
    if (flash_erase_state == 0)
    {
        log_d("back_to_app: Flash erase completed");
        for (readTime = 0; readTime < readDataCount; readTime++)
        {
            // 判断下当前buffer下的数据是否读取完毕
            if (Read_Memory_index == Read_Memory_Size)
            {
                log_d("back_to_app: Reading next frame from external flash "
                      "(block %u/%u)", readTime, readDataCount);
                if (2 == W25Q64_ReadData(Mem_Read_buffer, &Read_Memory_Size))
                {
                    log_e("back_to_app: Read extern buffer error at block %u",
                          readTime);
                    return -1;
                }
                Read_Memory_index = 0;
            }
            // 拷贝16个数据
            memcpy(Temp, Mem_Read_buffer + Read_Memory_index, 16);
            Read_Memory_index += 16;
            // 解析16个数据
            Aes_IV_key256bit_Decode(pu8_IV_IN_OUT, Temp, pu8_key256bit); // 解析

            RamSource = (uint32_t)Temp;
            for (wirteTime = 0; wirteTime < 4; wirteTime++)
            {
                Flash_Write(AppRunFlashDestination, *(uint32_t *)RamSource);
                AppRunFlashDestination += 4;
                RamSource += 4;
            }

            // 每64块打印一次进度
            if (readTime % 64 == 0)
            {
                log_d("back_to_app: Decryption progress %u/%u blocks", readTime,
                      readDataCount);
            }
        }
        log_i("back_to_app: OTA decryption completed successfully, %u bytes "
              "written to 0x%08X",
              AppSize, ApplicationAddress);
        return 0;
    }
    else
    {
        log_e("back_to_app: Flash erase failed at address 0x%08X",
              AppRunFlashDestination);
        return -1;
    }
}


// int8_t back_to_app(int32_t buf_size)
// {
//     uint32_t AppRunFlashDestination = ApplicationAddress;
//     uint8_t *pu8_IV_IN_OUT          = IV;
//     uint8_t *pu8_key256bit          = Key;
//     uint8_t *pu8_temp               = (uint8_t *)BackAppAddress; // 原始数据
//     uint8_t  Temp[16];          // 原密文数据缓存
//     uint8_t *pTemp         = Temp;
//     uint16_t readTime      = 0,
//              readDataCount = 0; // 读取数据再解密的次数（每次解密16个字节
//     u32 AppSize            = 0; // 升级包的大小

//     log_d("back_to_app: Starting OTA decryption, buf_size=%d", buf_size);

//     /* */
//     if ((buf_size > (0x18010 - 1)) || (buf_size <= 0))
//     {
//         log_e("back_to_app: buf_size exceeds limit or invalid: %d
//         (max=0x%x)",
//               buf_size, 0x18010 - 1);
//         return -1;
//     }

//     memcpy(pTemp, pu8_temp, 16);
//     pu8_temp += 16;
//     Aes_IV_key256bit_Decode(pu8_IV_IN_OUT, pTemp,
//                             pu8_key256bit); // 解析得到自定义内容+文件大小
//     AppSize =
//         (pTemp[15] << 24) + (pTemp[14] << 16) + (pTemp[13] << 8) + pTemp[12];
//     log_d("back_to_app: Decrypted header, AppSize=0x%08X (%u bytes)",
//     AppSize,
//           AppSize);

//     /*计算需要解密多少次*/
//     readDataCount = AppSize / 16;
//     if (AppSize % 16 != 0)
//     {
//         readDataCount += 1;
//     }
//     log_d("back_to_app: Will decrypt %u blocks (16 bytes each)",
//     readDataCount);

//     // 擦除运行区数据
//     log_d("back_to_app: Erasing flash at 0x%08X, size=0x%x",
//     ApplicationAddress,
//           AppSize);
//     if (1 == Flash_erase(ApplicationAddress, AppSize))
//     {
//         log_e("back_to_app: Flash erase failed at address 0x%08X",
//               ApplicationAddress);
//         return -1;
//     }
//     log_d("back_to_app: Flash erase completed");

//     // 读数据的总次数
//     for (readTime = 0; readTime < readDataCount; readTime++)
//     {
//         // 加密原文读取16个字节到临时区中
//         pTemp = Temp;
//         memcpy(pTemp, pu8_temp, 16);
//         pu8_temp += 16;
//         Aes_IV_key256bit_Decode(pu8_IV_IN_OUT, pTemp,
//                                 pu8_key256bit); // 解密数据
//         // 解密后的数据存入App运行区中
//         for (uint8_t j = 0; j < 16; j += 4)
//         {
//             Flash_Write(AppRunFlashDestination, *(uint32_t *)pTemp);
//             if (*(uint32_t *)AppRunFlashDestination != *(uint32_t *)pTemp)
//             {
//                 log_e("back_to_app: Flash write verification failed at 0x%08X
//                 "
//                       "(block %u, offset %u)",
//                       AppRunFlashDestination, readTime, j);
//                 return -1;
//             }
//             AppRunFlashDestination += 4;
//             pTemp += 4;
//         }
//         if (readTime % 64 == 0) // 每64块打印一次进度
//         {
//             log_d("back_to_app: Decryption progress %u/%u blocks", readTime,
//                   readDataCount);
//         }
//     }
//     log_i("back_to_app: OTA decryption completed successfully, %u bytes "
//           "written to 0x%08X",
//           AppSize, ApplicationAddress);
//     return 0;
// }
