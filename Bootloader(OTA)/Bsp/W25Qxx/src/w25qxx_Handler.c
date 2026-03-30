/******************************************************************************
 * Copyright (C) 2024 EternalChip, Inc.(Gmbh) or its affiliates.
 *
 * All Rights Reserved.
 *
 * @file W25Q_Handler.c
 *
 * @par dependencies
 * - W25Q_Handler.h
 *
 * @author Jack | R&D Dept. | EternalChip
 *
 * @brief Functions related to reading and writing in the chip's flash area.
 *
 * @version V1.0 2024-09-13
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/
/* Includes ------------------------------------------------------------------*/
#include "w25qxx_Handler.h"
#include "../../../Middlewares/SFUD/inc/sfud.h"
#include "w25qxx.h"
#include <string.h>

/* Private variables ---------------------------------------------------------*/
static st_W25Q_Handler   s_ast_W25Q_Handler[2];

static const sfud_flash *W25Q64_GetReadyFlash(void)
{
    const sfud_flash *flash = sfud_get_device(SFUD_W25Q64_DEVICE_INDEX);

    if ((flash != NULL) && (flash->init_ok == false))
    {
        (void)sfud_init();
        flash = sfud_get_device(SFUD_W25Q64_DEVICE_INDEX);
    }

    if ((flash == NULL) || (flash->init_ok == false))
    {
        return NULL;
    }

    return flash;
}

void SetBlockParmeter(u8 block_index, uint32_t app_size)
{
    const uint32_t subsector_size               = W25QXXXX_SUBSECTOR_SIZE;

    s_ast_W25Q_Handler[block_index].write_index = app_size;
    s_ast_W25Q_Handler[block_index].write_databuf_index =
        (u16)(app_size % subsector_size);
    s_ast_W25Q_Handler[block_index].write_sector_index =
        (u8)(app_size / subsector_size);
    s_ast_W25Q_Handler[block_index].read_index        = 0;
    s_ast_W25Q_Handler[block_index].read_sector_index = 0;
}

uint32_t Read_BlockSize(u8 block_index)
{
    return s_ast_W25Q_Handler[block_index].write_index;
}

void W25Q64_Init(void)
{
    (void)sfud_init();
    memset(s_ast_W25Q_Handler, 0, sizeof(s_ast_W25Q_Handler));
}

void Erase_Flash_Block(u8 block_index)
{
    uint32_t erase_addr = block_index * BLOCK_SIZE;
    uint32_t erase_end  = erase_addr + BLOCK_SIZE;

    while (erase_addr < erase_end)
    {
        W25Qx_Erase_Block(erase_addr);
        erase_addr += W25QXXXX_SUBSECTOR_SIZE;
    }

    memset(&s_ast_W25Q_Handler[block_index], 0, sizeof(st_W25Q_Handler));
}

u8 W25Q64_EraseChip(void)
{
    memset(s_ast_W25Q_Handler, 0, sizeof(s_ast_W25Q_Handler));
    return 0;
}

u8 W25Q64_WriteData(u8 block_index, u8 *data, u32 length)
{
    const uint32_t    subsector_size = W25QXXXX_SUBSECTOR_SIZE;
    const sfud_flash *flash          = W25Q64_GetReadyFlash();
    u16               index          = 0;

    if ((flash == NULL) || (data == NULL) || (length == 0U))
    {
        return 1;
    }

    for (u32 i = 0; i < length; i++)
    {
        index = s_ast_W25Q_Handler[block_index].write_databuf_index;
        s_ast_W25Q_Handler[block_index].databuf[index] = *(data + i);
        s_ast_W25Q_Handler[block_index].write_databuf_index++;

        if (s_ast_W25Q_Handler[block_index].write_databuf_index ==
            subsector_size)
        {
            uint32_t write_addr =
                (s_ast_W25Q_Handler[block_index].write_sector_index *
                 subsector_size) +
                (block_index * BLOCK_SIZE);

            if (SFUD_SUCCESS !=
                sfud_erase_write(flash, write_addr, subsector_size,
                                 s_ast_W25Q_Handler[block_index].databuf))
            {
                return 1;
            }

            s_ast_W25Q_Handler[block_index].write_databuf_index = 0;
            s_ast_W25Q_Handler[block_index].write_sector_index++;
            s_ast_W25Q_Handler[block_index].write_index += subsector_size;
        }
    }

    return 0;
}

u8 W25Q64_WriteData_End(u8 block_index)
{
    const sfud_flash *flash = W25Q64_GetReadyFlash();

    if (flash == NULL)
    {
        return 1;
    }

    if (0 != s_ast_W25Q_Handler[block_index].write_databuf_index)
    {
        uint32_t write_addr =
            (s_ast_W25Q_Handler[block_index].write_sector_index *
             W25QXXXX_SUBSECTOR_SIZE) +
            (block_index * BLOCK_SIZE);

        if (SFUD_SUCCESS !=
            sfud_erase_write(
                flash, write_addr,
                s_ast_W25Q_Handler[block_index].write_databuf_index,
                s_ast_W25Q_Handler[block_index].databuf))
        {
            return 1;
        }

        s_ast_W25Q_Handler[block_index].write_index +=
            s_ast_W25Q_Handler[block_index].write_databuf_index;
        s_ast_W25Q_Handler[block_index].write_databuf_index = 0;
        s_ast_W25Q_Handler[block_index].write_sector_index =
            (u8)(s_ast_W25Q_Handler[block_index].write_index /
                 W25QXXXX_SUBSECTOR_SIZE);
    }

    return 0;
}

/*
 * Read one chunk each call, max 4096 bytes.
 * return:
 *   0: read success
 *   1: no more data
 *   2: read fail
 */
u8 W25Q64_ReadData(u8 block_index, u8 *data, u16 *length)
{
    const uint32_t    subsector_size = W25QXXXX_SUBSECTOR_SIZE;
    const sfud_flash *flash          = W25Q64_GetReadyFlash();
    uint32_t          remain         = 0;
    uint32_t          read_addr      = 0;

    if ((flash == NULL) || (data == NULL) || (length == NULL))
    {
        return 2;
    }

    if (s_ast_W25Q_Handler[block_index].write_index >
        s_ast_W25Q_Handler[block_index].read_index)
    {
        remain = s_ast_W25Q_Handler[block_index].write_index -
                 s_ast_W25Q_Handler[block_index].read_index;

        if (remain > subsector_size)
        {
            *length = (u16)subsector_size;
        }
        else
        {
            *length = (u16)remain;
        }

        read_addr = (block_index * BLOCK_SIZE) +
                    s_ast_W25Q_Handler[block_index].read_index;

        if (SFUD_SUCCESS != sfud_read(flash, read_addr, *length, data))
        {
            return 2;
        }

        s_ast_W25Q_Handler[block_index].read_index += *length;
        s_ast_W25Q_Handler[block_index].read_sector_index =
            (u8)(s_ast_W25Q_Handler[block_index].read_index / subsector_size);
        return 0;
    }

    return 1;
}
