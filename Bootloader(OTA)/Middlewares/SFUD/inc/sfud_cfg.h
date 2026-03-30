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
 * Function: It is the configure head file for this library.
 * Created on: 2016-04-23
 */

#ifndef _SFUD_CFG_H_
#define _SFUD_CFG_H_

// 关闭调试模式以减少代码量
// #define SFUD_DEBUG_MODE

// 启用SFDP支持
// #define SFUD_USING_SFDP

// 启用快速读模式
#define SFUD_USING_FAST_READ

// 启用Flash信息表
#define SFUD_USING_FLASH_INFO_TABLE

// 定义Flash设备索引
enum
{
    SFUD_W25Q64_DEVICE_INDEX = 0,
};

// 定义Flash设备表
#define SFUD_FLASH_DEVICE_TABLE                                                \
    {                                                                          \
        [SFUD_W25Q64_DEVICE_INDEX] = {.name = "W25Q64", .spi.name = "SPI1"},   \
    }

// 不使用QSPI模式
// #define SFUD_USING_QSPI

#endif /* _SFUD_CFG_H_ */
