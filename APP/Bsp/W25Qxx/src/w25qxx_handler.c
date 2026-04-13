/**
 ******************************************************************************
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

/* Includes ------------------------------------------------------------------*/
#include "w25qxx_handler.h"
#include "w25qxx.h"
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static st_W25Q_Handler s_st_W25Q_Handler_1;
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
/* extern variables ---------------------------------------------------------*/

/**
 * @brief
 * Initialize W25Q runtime handler state.
 *
 * @param[in] : None.
 *
 * @param[out] : None.
 *
 * @return
 * None.
 * */
void W25Q64_Init(void)
{
    /**
     * Reset read/write cursors for a new download session.
     **/
    W25Qx_Init();
    s_st_W25Q_Handler_1.read_index          = 0;
    s_st_W25Q_Handler_1.read_sector_index   = 0;
    s_st_W25Q_Handler_1.write_databuf_index = 0;
    s_st_W25Q_Handler_1.write_index         = 0;
    s_st_W25Q_Handler_1.write_sector_index  = 0;
}

/**
 * @brief
 * Erase whole W25Q64 chip and clear runtime indexes.
 *
 * @param[in] : None.
 *
 * @param[out] : None.
 *
 * @return
 * 0 on success, 1 on failure.
 * */
u8 W25Q64_EraseChip(void)
{
    if (0 == W25Qx_Erase_Chip())
    {
        /**
         * Keep software cursor state aligned with blank flash content.
         **/
        s_st_W25Q_Handler_1.read_index          = 0;
        s_st_W25Q_Handler_1.read_sector_index   = 0;
        s_st_W25Q_Handler_1.write_databuf_index = 0;
        s_st_W25Q_Handler_1.write_index         = 0;
        s_st_W25Q_Handler_1.write_sector_index  = 0;
        return 0;
    }
    return 1;
}

/**
 * @brief
 * Append payload into 4KB staging buffer and flush by subsector.
 *
 * @param[in]  data   : Pointer to input payload.
 *
 * @param[in]  length : Payload length in bytes.
 *
 * @param[out] : None.
 *
 * @return
 * 0 on completion.
 * */
u8 W25Q64_WriteData(u8 *data, u32 length)
{
    u32 addr  = 0;
    u16 index = 0;

    /**
     * Buffer incoming bytes and commit to flash when a full 4KB block is
     * ready.
     **/
    for (u16 i = 0; i < length; i++)
    {
        /* Stage one byte into RAM cache. */
        index = s_st_W25Q_Handler_1.write_databuf_index;
        s_st_W25Q_Handler_1.databuf[index] = *(data + i);
        s_st_W25Q_Handler_1.write_databuf_index++;

        /* Flush one full subsector once cache reaches 4096 bytes. */
        if (s_st_W25Q_Handler_1.write_databuf_index ==
            W25Qx_Para.SUBSECTOR_SIZE)
        {
            s_st_W25Q_Handler_1.write_databuf_index = 0;

            /* Erase and then program this subsector page by page. */
            addr = W25Qx_Para.SUBSECTOR_SIZE *
                   s_st_W25Q_Handler_1.write_sector_index;
            W25Qx_Erase_Block(addr);
            W25Qx_WriteEnable();

            for (u8 j = 0; j < 16; j++)
            {
                addr = (W25Qx_Para.SUBSECTOR_SIZE *
                        s_st_W25Q_Handler_1.write_sector_index) +
                       (W25Qx_Para.PAGE_SIZE * j);

                index = W25Qx_Para.PAGE_SIZE * j;
                W25Qx_Write(&s_st_W25Q_Handler_1.databuf[index], addr,
                            W25Qx_Para.PAGE_SIZE);
            }

            s_st_W25Q_Handler_1.write_sector_index++;
            s_st_W25Q_Handler_1.write_index += W25Qx_Para.SUBSECTOR_SIZE;
        }
    }
    return 0;
}

/**
 * @brief
 * Flush remaining cached data that does not fill a full 4KB subsector.
 *
 * @param[in] : None.
 *
 * @param[out] : None.
 *
 * @return
 * 0 on completion.
 * */
u8 W25Q64_WriteData_End(void)
{
    u32 addr      = 0;
    u16 index     = 0;
    u8  page_size = 0;

    /**
     * Commit residual bytes in cache after transfer is complete.
     **/
    if (0 != s_st_W25Q_Handler_1.write_databuf_index)
    {
        /* Number of complete pages that can be written directly. */
        page_size =
            s_st_W25Q_Handler_1.write_databuf_index / W25Qx_Para.PAGE_SIZE;

        /* Erase target subsector once before programming pages. */
        addr =
            W25Qx_Para.SUBSECTOR_SIZE * s_st_W25Q_Handler_1.write_sector_index;
        W25Qx_Erase_Block(addr);
        W25Qx_WriteEnable();
        for (u8 j = 0; j < page_size; j++)
        {
            addr = (W25Qx_Para.SUBSECTOR_SIZE *
                    s_st_W25Q_Handler_1.write_sector_index) +
                   (W25Qx_Para.PAGE_SIZE * j);

            index = W25Qx_Para.PAGE_SIZE * j;
            W25Qx_Write(&s_st_W25Q_Handler_1.databuf[index], addr,
                        W25Qx_Para.PAGE_SIZE);
        }

        /* Program last partial page if residual data exists. */
        if (0 !=
            (s_st_W25Q_Handler_1.write_databuf_index % W25Qx_Para.PAGE_SIZE))
        {
            addr = (W25Qx_Para.SUBSECTOR_SIZE *
                    s_st_W25Q_Handler_1.write_sector_index) +
                   (W25Qx_Para.PAGE_SIZE * page_size);

            index = W25Qx_Para.PAGE_SIZE * page_size;
            W25Qx_Write(&s_st_W25Q_Handler_1.databuf[index], addr,
                        s_st_W25Q_Handler_1.write_databuf_index %
                            W25Qx_Para.PAGE_SIZE);
        }
        s_st_W25Q_Handler_1.write_index +=
            s_st_W25Q_Handler_1.write_databuf_index;
    }
    return 0;
}

/**
 * @brief
 * Read data from W25Q64 into caller buffer.
 *
 * @param[in]  data   : Pointer to destination buffer.
 *
 * @param[out] length : Actual bytes read in this call.
 *
 * @return
 * 0 for success, 1 for no more data, 2 for flash read failure.
 * */
u8 W25Q64_ReadData(u8 *data, u16 *length)
{
    u32 addr = 0;

    /**
     * Prefer 4KB reads and fallback to tail length for the last chunk.
     **/
    if (s_st_W25Q_Handler_1.write_index > s_st_W25Q_Handler_1.read_index)
    {
        if (s_st_W25Q_Handler_1.write_sector_index >
            s_st_W25Q_Handler_1.read_sector_index)
        {
            *length = W25Qx_Para.SUBSECTOR_SIZE;
            addr    = s_st_W25Q_Handler_1.read_sector_index *
                   W25Qx_Para.SUBSECTOR_SIZE;
            if (0 != W25Qx_Read(data, addr, *length))
                return 2;
            s_st_W25Q_Handler_1.read_sector_index++;
        }
        else
        {
            *length = s_st_W25Q_Handler_1.write_index -
                      s_st_W25Q_Handler_1.read_index;
            addr = s_st_W25Q_Handler_1.read_sector_index *
                   W25Qx_Para.SUBSECTOR_SIZE;
            if (0 != W25Qx_Read(data, addr, *length))
                return 2;
        }
        s_st_W25Q_Handler_1.read_index += *length;
        return 0;
    }
    else
    {
        return 1;
    }
}
