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
 * @author Jack | R&D Dept. | EternalChip 立芯嵌入式
 * 
 * @brief Functions related to reading and writing in the chip's flash area.
 * 
 * Processing flow:
 * 
 * call directly.
 * 
 * @version V1.0 2024-09-13
 *
 * @note 1 tab == 4 spaces!
 * 
 *****************************************************************************/
/* Includes ------------------------------------------------------------------*/
#include "w25qxx_Handler.h"
#include "w25qxx.h"
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static st_W25Q_Handler s_ast_W25Q_Handler[2];
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
/* extern variables ---------------------------------------------------------*/

void SetBlockParmeter(u8 block_index,uint32_t app_size)
{
    s_ast_W25Q_Handler[block_index].write_index = app_size;
    s_ast_W25Q_Handler[block_index].write_databuf_index = app_size % BLOCK_SIZE;
    s_ast_W25Q_Handler[block_index].write_sector_index = app_size / BLOCK_SIZE;
}

uint32_t Read_BlockSize(u8 block_index)
{
    return s_ast_W25Q_Handler[block_index].write_index;
}

/**
 * @brief w25q初始化
 * 
 * @param None
 * 
 * @return: None
 */
void W25Q64_Init(void)
{
    W25Qx_Init();
    s_ast_W25Q_Handler[0].read_index = 0;
    s_ast_W25Q_Handler[0].read_sector_index = 0;
    s_ast_W25Q_Handler[0].write_databuf_index = 0;
    s_ast_W25Q_Handler[0].write_index = 0;
    s_ast_W25Q_Handler[0].write_sector_index = 0;

    s_ast_W25Q_Handler[1].read_index = 0;
    s_ast_W25Q_Handler[1].read_sector_index = 0;
    s_ast_W25Q_Handler[1].write_databuf_index = 0;
    s_ast_W25Q_Handler[1].write_index = 0;
    s_ast_W25Q_Handler[1].write_sector_index = 0;
}

void Erase_Flash_Block(u8 block_index)
{
	s_ast_W25Q_Handler[block_index].read_index = 0;
	s_ast_W25Q_Handler[block_index].read_sector_index = 0;
	s_ast_W25Q_Handler[block_index].write_databuf_index = 0;
	s_ast_W25Q_Handler[block_index].write_index = 0;
	s_ast_W25Q_Handler[block_index].write_sector_index = 0;
}

/**
 * @brief w25q芯片擦除
 * 
 * @param None
 * 
 * @return: None
 */
u8 W25Q64_EraseChip(void)
{
    if(0 == W25Qx_Erase_Chip())
    {
        s_ast_W25Q_Handler[0].read_index = 0;
        s_ast_W25Q_Handler[0].read_sector_index = 0;
        s_ast_W25Q_Handler[0].write_databuf_index = 0;
        s_ast_W25Q_Handler[0].write_index = 0;
        s_ast_W25Q_Handler[0].write_sector_index = 0;
    
        s_ast_W25Q_Handler[1].read_index = 0;
        s_ast_W25Q_Handler[1].read_sector_index = 0;
        s_ast_W25Q_Handler[1].write_databuf_index = 0;
        s_ast_W25Q_Handler[1].write_index = 0;
        s_ast_W25Q_Handler[1].write_sector_index = 0;
        return 0;
    }
    return 1;
}

/**
 * @brief w25q芯片擦除
 * 
 * @param None
 * 
 * @return: None
 */
u8 W25Q64_WriteData(u8 block_index, u8 *data, u32 length)
{
    u32 addr = 0;
    u16 index = 0;
    for(u16 i = 0; i < length; i++)
    {
        //1.数据写入数据缓冲区里面
        index = s_ast_W25Q_Handler[block_index].write_databuf_index;
        s_ast_W25Q_Handler[block_index].databuf[index] = *(data + i);
        s_ast_W25Q_Handler[block_index].write_databuf_index++;
        //2.判断数据有没有写满4096
        if(s_ast_W25Q_Handler[block_index].write_databuf_index == W25Qx_Para.SUBSECTOR_SIZE)
        {
            s_ast_W25Q_Handler[block_index].write_databuf_index = 0;
            //擦除1个sector
            addr = W25Qx_Para.SUBSECTOR_SIZE * s_ast_W25Q_Handler[block_index].write_sector_index;
            addr += block_index * BLOCK_SIZE;
            W25Qx_Erase_Block(addr);
            W25Qx_WriteEnable();
            //写满了一个sector，执行写入操作   4096个byte
            for(u8 j = 0; j < 16; j++)
            {
                //获取当前写入地址
                addr = (W25Qx_Para.SUBSECTOR_SIZE * s_ast_W25Q_Handler[block_index].write_sector_index) + \
                        (W25Qx_Para.PAGE_SIZE * j);
                addr += block_index * BLOCK_SIZE;
                //执行写入操作
                index = W25Qx_Para.PAGE_SIZE * j;
                W25Qx_Write(&s_ast_W25Q_Handler[block_index].databuf[index],addr,W25Qx_Para.PAGE_SIZE);
            }
            s_ast_W25Q_Handler[block_index].write_sector_index++;
            //记录总的写入数据长度
            s_ast_W25Q_Handler[block_index].write_index += W25Qx_Para.SUBSECTOR_SIZE;
        }
    }
    return 0;
}

u8 W25Q64_WriteData_End(u8 block_index)
{
    u32 addr = 0;
    u16 index = 0;
    u8  page_size = 0;
    //判断还有没有剩余数据没有执行写入
    if(0 != s_ast_W25Q_Handler[block_index].write_databuf_index)
    {
        //写入次数
        page_size = s_ast_W25Q_Handler[block_index].write_databuf_index / W25Qx_Para.PAGE_SIZE;
        //先执行擦除扇区
        addr = W25Qx_Para.SUBSECTOR_SIZE * s_ast_W25Q_Handler[block_index].write_sector_index;
        addr += block_index * BLOCK_SIZE;
        W25Qx_Erase_Block(addr);
        W25Qx_WriteEnable();
        for(u8 j = 0; j < page_size; j++)
        {
            //获取当前写入地址
            addr = (W25Qx_Para.SUBSECTOR_SIZE * s_ast_W25Q_Handler[block_index].write_sector_index) + \
                    (W25Qx_Para.PAGE_SIZE * j);
            addr += block_index * BLOCK_SIZE;
            //执行写入操作
            index = W25Qx_Para.PAGE_SIZE * j;
            W25Qx_Write(&s_ast_W25Q_Handler[block_index].databuf[index],addr,W25Qx_Para.PAGE_SIZE);
        }

        //有没有小于256的数据了
        if(0 != (s_ast_W25Q_Handler[block_index].write_databuf_index % W25Qx_Para.PAGE_SIZE))
        {
            //获取当前写入地址
            addr = (W25Qx_Para.SUBSECTOR_SIZE * s_ast_W25Q_Handler[block_index].write_sector_index) + \
                    (W25Qx_Para.PAGE_SIZE * page_size);
            addr += block_index * BLOCK_SIZE;
            //执行写入操作
            index = W25Qx_Para.PAGE_SIZE * page_size;
            W25Qx_Write(&s_ast_W25Q_Handler[block_index].databuf[index],addr, \
                        s_ast_W25Q_Handler[block_index].write_databuf_index % W25Qx_Para.PAGE_SIZE);
        }
        s_ast_W25Q_Handler[block_index].write_index += s_ast_W25Q_Handler[block_index].write_databuf_index;
    }
    return 0;
}

/*每次读取一个扇区的数据，外部接口需要一个4096的buffer
length:每次读取出来的长度，从缓冲区读取
return : 
        0:表示读取成功
        1:读取完毕  没有数据了
        2:读取失败  读取错误
*/
u8 W25Q64_ReadData(u8 block_index, u8 *data, u16 *length)
{
    u32 addr = 0;

    //1.先判断是否读取完毕
    if(s_ast_W25Q_Handler[block_index].write_index > s_ast_W25Q_Handler[block_index].read_index)
    {
        //判断下数据是否够4K
        if(s_ast_W25Q_Handler[block_index].write_sector_index > s_ast_W25Q_Handler[block_index].read_sector_index)
        {
            //读4K数据的操作
            *length = W25Qx_Para.SUBSECTOR_SIZE;
            addr = s_ast_W25Q_Handler[block_index].read_sector_index * W25Qx_Para.SUBSECTOR_SIZE;
            addr += block_index * BLOCK_SIZE;
            if(0 != W25Qx_Read(data,addr,*length))
                return 2;
            s_ast_W25Q_Handler[block_index].read_sector_index++;
        }
        else
        {
            //读4K以内的数据
            *length = s_ast_W25Q_Handler[block_index].write_index - s_ast_W25Q_Handler[block_index].read_index;
            addr = s_ast_W25Q_Handler[block_index].read_sector_index * W25Qx_Para.SUBSECTOR_SIZE;
            addr += block_index * BLOCK_SIZE;
            if(0 != W25Qx_Read(data,addr,*length))
                return 2;
        }
        s_ast_W25Q_Handler[block_index].read_index += *length;
        return 0;
    }
    else
    {
        return 1;
    }
}
