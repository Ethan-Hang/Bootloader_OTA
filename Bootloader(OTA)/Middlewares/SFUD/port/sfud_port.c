/*
 * This file is part of the Serial Flash Universal Driver Library.
 *
 * Copyright (c) 2016-2018, Armink, <armink.ztl@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * 'Software'), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Function: Portable interface for each platform.
 * Created on: 2016-04-23
 */

#include <sfud.h>
#include <stdarg.h>
#include "main.h"
#include "Spi.h"
#include "string.h"

#ifdef SFUD_DEBUG_MODE
static char log_buf[256];
#endif

void sfud_log_debug(const char *file, const long line, const char *format, ...);

/**
 * SPI写读数据函数
 */
static sfud_err spi_write_read(const sfud_spi *spi, const uint8_t *write_buf,
                               size_t write_size, uint8_t *read_buf,
                               size_t read_size)
{
    sfud_err result = SFUD_SUCCESS;

    // 拉低CS
    GPIO_ResetBits(F_CS_GPIO_Port, F_CS_Pin);

    // 发送写数据
    if (write_size > 0)
    {
        if (!SPI1_WriteByte((uint8_t *)write_buf, write_size, 1000))
        {
            result = SFUD_ERR_TIMEOUT;
            goto exit;
        }
    }

    // 接收读数据
    if (read_size > 0)
    {
        /* SPI1_ReadByte 使用同一缓冲区发送和接收，先填充 0xFF 作为 Dummy。 */
        memset(read_buf, 0xFF, read_size);

        if (!SPI1_ReadByte(read_buf, read_size, 1000))
        {
            result = SFUD_ERR_TIMEOUT;
            goto exit;
        }
    }

exit:
    // 拉高CS
    GPIO_SetBits(F_CS_GPIO_Port, F_CS_Pin);
    return result;
}

#ifdef SFUD_USING_QSPI
/**
 * read flash data by QSPI
 */
static sfud_err qspi_read(const struct __sfud_spi *spi, uint32_t addr,
                          sfud_qspi_read_cmd_format *qspi_read_cmd_format,
                          uint8_t *read_buf, size_t read_size)
{
    sfud_err result = SFUD_SUCCESS;

    /**
     * add your qspi read flash data code
     */

    return result;
}
#endif /* SFUD_USING_QSPI */

/**
 * SPI总线锁定
 */
static void spi_lock(const sfud_spi *spi)
{
    /* Bare-metal bootloader: keep as no-op to avoid blocking SysTick delay. */
    (void)spi;
}

/**
 * SPI总线解锁
 */
static void spi_unlock(const sfud_spi *spi)
{
    (void)spi;
}

/**
 * 延时函数
 */
static void retry_delay_100us(void)
{
    // 使用现有的延时函数，100us延时
    delay_ms(1); // 假设Delay(1)为1ms，可根据实际调整
}

/**
 * SFUD SPI端口初始化
 */
sfud_err sfud_spi_port_init(sfud_flash *flash)
{
    sfud_err result = SFUD_SUCCESS;

    switch (flash->index)
    {
    case SFUD_W25Q64_DEVICE_INDEX:
    {
        // 初始化SPI1
        SPI1_Init();

        // 配置SFUD SPI接口
        flash->spi.wr        = spi_write_read;
        flash->spi.lock      = spi_lock;
        flash->spi.unlock    = spi_unlock;
        flash->spi.user_data = NULL;

        // 配置重试参数
        flash->retry.delay   = retry_delay_100us;
        flash->retry.times   = 10000;

        break;
    }
    default:
        result = SFUD_ERR_NOT_FOUND;
        break;
    }

    return result;
}

/**
 * 调试信息输出
 *
 * @param file the file which has call this function
 * @param line the line number which has call this function
 * @param format output format
 * @param ... args
 */
void sfud_log_debug(const char *file, const long line, const char *format, ...)
{
#ifdef SFUD_DEBUG_MODE
    va_list args;
    va_start(args, format);
    printf("[SFUD](%s:%ld) ", file, line);
    vsnprintf(log_buf, sizeof(log_buf), format, args);
    printf("%s\n", log_buf);
    va_end(args);
#endif
}

/**
 * 常规信息输出
 *
 * @param format output format
 * @param ... args
 */
void sfud_log_info(const char *format, ...)
{
#ifdef SFUD_DEBUG_MODE
    va_list args;
    va_start(args, format);
    printf("[SFUD]");
    vsnprintf(log_buf, sizeof(log_buf), format, args);
    printf("%s\n", log_buf);
    va_end(args);
#endif
}
