#ifndef __OTA_H__
#define __OTA_H__


#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "ymodem.h"
#include "cmsis_os2.h"
#include "usart.h"
#include "main.h"
#include "Debug.h"
#include "at24cxx_driver.h"
#include "w25qxx_handler.h"

typedef enum
{
    OTA_WAIT_FOR_DOWNLOAD_REQ = 0,
    OTA_DOWNLOAD              = 1,
    OTA_WAIT_REQ              = 2,
    OTA_DOWNLOAD_END          = 3,
} ota_download_status_t;

typedef enum
{
    OTA_EMPTY               = 0x00,
    OTA_DOWNLOADING         = 0x11,
    OTA_DOWNLOAD_FINISHED   = 0x22,
} ee_os_status_t;


void ota_task_runnable(void *argument);

#endif /* __OTA_H__ */
