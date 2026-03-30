#include "sfud_adapter.h"
#include "string.h"

static uint32_t          block_sizes[2]   = {0}; // BLOCK_1和BLOCK_2的大小
static uint32_t          write_offsets[2] = {0}; // 当前写入偏移
static const sfud_flash *flash            = NULL;

/**
 * SFUD Flash初始化
 */
void                     SFUD_W25Q64_Init(void)
{
    // 初始化SFUD库
    if (sfud_init() == SFUD_SUCCESS)
    {
        // 获取Flash设备
        flash = sfud_get_device(SFUD_W25Q64_DEVICE_INDEX);
        if (flash == NULL)
        {
            // 初始化失败处理
            while (1)
                ; // 或者其他错误处理
        }
    }
}

/**
 * 擦除整个Flash芯片
 */
uint8_t SFUD_W25Q64_EraseChip(void)
{
    if (flash == NULL)
        return 0;

    sfud_err result = sfud_chip_erase(flash);
    return (result == SFUD_SUCCESS) ? 1 : 0;
}

/**
 * 擦除Flash块
 */
void SFUD_Erase_Flash_Block(uint8_t block_index)
{
    if (flash == NULL)
        return;

    uint32_t addr = block_index * BLOCK_SIZE;
    sfud_erase(flash, addr, BLOCK_SIZE);
    write_offsets[block_index] = 0; // 重置写入偏移
}

/**
 * 写入数据到Flash（使用擦除写入）
 */
uint8_t SFUD_W25Q64_WriteData(uint8_t block_index, uint8_t *data,
                              uint32_t length)
{
    if (flash == NULL)
        return 0;

    uint32_t addr   = (block_index * BLOCK_SIZE) + write_offsets[block_index];

    // 使用SFUD的擦除写入功能（自动处理擦除和写入）
    sfud_err result = sfud_erase_write(flash, addr, length, data);

    if (result == SFUD_SUCCESS)
    {
        write_offsets[block_index] += length;
        return 1;
    }
    return 0;
}

/**
 * 写入数据结束（保持接口兼容）
 */
uint8_t SFUD_W25Q64_WriteData_End(uint8_t block_index)
{
    // SFUD不需要特殊的写入结束操作
    return 1;
}

/**
 * 从Flash读取数据
 */
uint8_t SFUD_W25Q64_ReadData(uint8_t block_index, uint8_t *data,
                             uint16_t *length)
{
    if (flash == NULL)
        return 0;

    uint32_t addr   = block_index * BLOCK_SIZE;
    sfud_err result = sfud_read(flash, addr, *length, data);
    return (result == SFUD_SUCCESS) ? 1 : 0;
}

/**
 * 设置块参数
 */
void SFUD_SetBlockParmeter(uint8_t block_index, uint32_t app_size)
{
    if (block_index < 2)
    {
        block_sizes[block_index]   = app_size;
        write_offsets[block_index] = 0; // 重置写入偏移
    }
}

/**
 * 读取块大小
 */
uint32_t SFUD_Read_BlockSize(uint8_t block_index)
{
    if (block_index < 2)
    {
        return block_sizes[block_index];
    }
    return 0;
}
