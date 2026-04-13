/******************************************************************************
 * @file OTA.h
 *
 * @par dependencies
 * - main.h
 * - usart.h
 * - at24cxx_driver.h
 * - w25qxx_handler.h
 * - cmsis_os2.h
 * - FreeRTOS.h
 * - queue.h
 * - semphr.h
 * - task.h
 * - ymodem.h
 * - Debug.h
 *
 * @author Ethan-Hang
 *
 * @brief
 * APP OTA state definitions and task interface.
 *
 * Processing flow:
 * 1. Receive OTA commands and download status.
 * 2. Store OTA state in EEPROM for bootloader handoff.
 * 3. Expose OTA task entry for scheduler startup.
 *
 * @version V1.0 2026-3-28
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/
#ifndef __OTA_H__
#define __OTA_H__

#include "main.h"
#include "usart.h"

#include "at24cxx_driver.h"
#include "w25qxx_handler.h"

#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "ymodem.h"

#include "Debug.h"

/**
 * @brief OTA runtime state in APP.
 */
typedef enum
{
    OTA_WAIT_FOR_DOWNLOAD_REQ = 0,
    OTA_DOWNLOAD              = 1,
    OTA_WAIT_REQ              = 2,
    OTA_DOWNLOAD_END          = 3,
} ota_download_status_t;

/**
 * @brief OTA status stored in EEPROM.
 */
typedef enum
{
    EE_OTA_NO_APP_UPDATE     = 0x00,
    EE_OTA_DOWNLOADING       = 0x11,
    EE_OTA_DOWNLOAD_FINISHED = 0x22,
    EE_OTA_APP_CHECK_START   = 0x33,
    EE_OTA_APP_CHECK_SUCCESS = 0x44,
} ee_os_status_t;

/**
 * @brief OTA task entry function.
 *
 * @param[in] argument : Thread argument from RTOS.
 *
 * @return
 * None.
 */
void ota_task_runnable(void *argument);

#endif /* __OTA_H__ */
