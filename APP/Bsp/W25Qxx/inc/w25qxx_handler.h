/******************************************************************************
 * @file w25qxx_handler.c
 *
 * @par dependencies
 * - w25qxx_handler.h
 * - w25qxx.h
 *
 * @author Ethan-Hang
 *
 * @brief
 * W25Q64 buffered read and write helper implementation.
 *
 * @version V1.0 2026-4-3
 *
 * @note 1 tab == 4 spaces!
 ******************************************************************************
 */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __W25Q_HANDLER_H
#define __W25Q_HANDLER_H
/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Exported types ------------------------------------------------------------*/
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;
/*
最小读写单元Pag = 256 byte
一个扇区 = 16个Pag = 4096 byte
一个块 = 16个扇区 = 64KB
*/
typedef struct
{
    u8  databuf[4096];      // 按找4096个数据进行读写设置
    u16 write_databuf_index;
    u32 write_index;
    u8  write_sector_index; // 4096
    u32 read_index;
    u8  read_sector_index;
} st_W25Q_Handler;

/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
#define EXTERN_Flash

/* Exported functions ------------------------------------------------------- */
/**
 * @brief
 * Initialize W25Q64 handler runtime context.
 *
 * @param[in] : None.
 *
 * @param[out] : None.
 *
 * @return
 * None.
 * */
void W25Q64_Init(void);

/**
 * @brief
 * Erase whole W25Q64 chip.
 *
 * @param[in] : None.
 *
 * @param[out] : None.
 *
 * @return
 * 0 on success, 1 on failure.
 * */
u8   W25Q64_EraseChip(void);

/**
 * @brief
 * Append data stream into W25Q64 with internal staging.
 *
 * @param[in]  data   : Source data pointer.
 *
 * @param[in]  length : Data length in bytes.
 *
 * @param[out] : None.
 *
 * @return
 * 0 on completion.
 * */
u8   W25Q64_WriteData(u8 *data, u32 length);

/**
 * @brief
 * Flush remaining staged data into flash.
 *
 * @param[in] : None.
 *
 * @param[out] : None.
 *
 * @return
 * 0 on completion.
 * */
u8   W25Q64_WriteData_End(void);

/**
 * @brief
 * Read next data block from flash.
 *
 * @param[in]  data   : Destination buffer.
 *
 * @param[out] length : Returned data length for this call.
 *
 * @return
 * 0 success, 1 no data, 2 read failure.
 * */
u8   W25Q64_ReadData(u8 *data, u16 *length);
#endif /* __FLASH_H */
