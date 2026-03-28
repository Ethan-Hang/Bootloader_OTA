#include "OTA.h"
#include <string.h>
#include "ymodem.h"

uint8_t              g_au8_YmodemRec[2][1030];
static uint8_t       s_otacmd[4];
extern QueueHandle_t Q_YmodemReclength;

QueueHandle_t        Queue_AppDataBuffer;
SemaphoreHandle_t    Semaphore_ExtFlashState;


osThreadId_t         DownloadAppData_taskHandle;
const osThreadAttr_t DownloadAppData_task_attributes = {
    .name       = "DownloadAppData_task",
    .stack_size = 512 * 4,
    .priority   = (osPriority_t)osPriorityNormal1,
};

osThreadId_t         OTA_taskHandle;
const osThreadAttr_t OTA_task_attributes = {
    .name       = "OTA_task",
    .stack_size = 512 * 4,
    .priority   = (osPriority_t)osPriorityNormal1,
};


void          DownloadAppData_task_runnable(void *argument);

static int8_t Key_Scan(void)
{
    for (uint32_t i = 0; i < 2000; i++)
    {
        if (GPIO_PIN_RESET == HAL_GPIO_ReadPin(Key_GPIO_Port, Key_Pin))
        {
            osDelay(20);
            if (GPIO_PIN_RESET == HAL_GPIO_ReadPin(Key_GPIO_Port, Key_Pin))
            {
                /*判断为按下*/
                return 1;
            }
            osDelay(80);
        }
    }
    return -1;
}

void ota_wait_for_download_req_handler(ota_download_status_t *status,
                                       ee_os_status_t        *ee_status)
{
    log_w("Change to OTA_WAIT_FOR_DOWNLOAD_REQ state");

    HAL_UART_Transmit(&huart1, (uint8_t *)"Waiting for download request...\r\n",
                      33, HAL_MAX_DELAY);
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, s_otacmd, 4);

    uint16_t rec_length = 0;
    xQueueReceive(Q_YmodemReclength, &rec_length, portMAX_DELAY);

    log_i("received command %x %x %x, length: %d", s_otacmd[0], s_otacmd[1],
          s_otacmd[2], rec_length);

    if (3 == rec_length)
    {
        if (s_otacmd[0] == 0x11 && s_otacmd[1] == 0x22 && s_otacmd[2] == 0x33)
        {
            *status             = OTA_DOWNLOAD;

            Queue_AppDataBuffer = xQueueCreate(4, sizeof(Ymodem_QueueMsg_t**));
            Semaphore_ExtFlashState = xSemaphoreCreateBinary();
            if (NULL == Queue_AppDataBuffer || NULL == Semaphore_ExtFlashState)
            {
                log_e("Failed to create OTA IPC resources");
                if (NULL != Queue_AppDataBuffer)
                {
                    vQueueDelete(Queue_AppDataBuffer);
                    Queue_AppDataBuffer = NULL;
                }
                if (NULL != Semaphore_ExtFlashState)
                {
                    vSemaphoreDelete(Semaphore_ExtFlashState);
                    Semaphore_ExtFlashState = NULL;
                }
                *status = OTA_WAIT_FOR_DOWNLOAD_REQ;
                return;
            }

            /* Give the binary semaphore to make it available */
            xSemaphoreGive(Semaphore_ExtFlashState);

            xTaskNotifyStateClear((TaskHandle_t)OTA_taskHandle);
            DownloadAppData_taskHandle =
                osThreadNew(DownloadAppData_task_runnable, NULL,
                            &DownloadAppData_task_attributes);
            if (NULL == DownloadAppData_taskHandle)
            {
                log_e("Failed to create DownloadAppData task");
                vQueueDelete(Queue_AppDataBuffer);
                Queue_AppDataBuffer = NULL;
                vSemaphoreDelete(Semaphore_ExtFlashState);
                Semaphore_ExtFlashState = NULL;
                *status                 = OTA_WAIT_FOR_DOWNLOAD_REQ;
                return;
            }

            *ee_status = OTA_DOWNLOADING;
            ee_WriteBytes((uint8_t *)ee_status, 0x00, 1);
        }
        else
        {
            *status = OTA_WAIT_FOR_DOWNLOAD_REQ;
        }
    }
    else
    {
        *status = OTA_WAIT_FOR_DOWNLOAD_REQ;
    }

    memset(s_otacmd, 0, sizeof(s_otacmd));
}

void ota_download_handler(ota_download_status_t *status,
                          ee_os_status_t        *ee_status)
{
    log_w("Change to OTA_DOWNLOAD state");

    int32_t app_data_length = 0;
    app_data_length         = Ymodem_Receive(g_au8_YmodemRec);
    if (app_data_length > 0)
    {
        *status = OTA_WAIT_REQ;
        log_i("Received app data, length: %d", app_data_length);

        W25Q64_WriteData_End();

        *ee_status = OTA_DOWNLOAD_FINISHED;
    }
    else
    {
        *ee_status = OTA_EMPTY;
        *status    = OTA_WAIT_FOR_DOWNLOAD_REQ;
    }

    ee_WriteBytes((uint8_t *)ee_status, 0x00, 1);
    log_i("EE Writed %d EE read status: %d", *ee_status,
          ee_ReadBytes((uint8_t *)ee_status, 0x00, 1));

    osDelay(1000);
    // cleanup
    vTaskDelete(DownloadAppData_taskHandle);
    vQueueDelete(Queue_AppDataBuffer);
    vSemaphoreDelete(Semaphore_ExtFlashState);
}

void ota_wait_req_handler(ota_download_status_t *status,
                          ee_os_status_t        *ee_status)
{
    log_w("Change to OTA_WAIT_REQ state");

    HAL_UART_Transmit(&huart1, (uint8_t *)"Waiting for update request...\r\n",
                      32, HAL_MAX_DELAY);
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, s_otacmd, 4);

    uint16_t rec_length = 0;
    xQueueReceive(Q_YmodemReclength, &rec_length, portMAX_DELAY);
    if (3 == rec_length)
    {
        if (s_otacmd[0] == 0x77 && s_otacmd[1] == 0x88 && s_otacmd[2] == 0x99)
        {
            *status = OTA_DOWNLOAD_END;
        }
    }
    else
    {
        *status = OTA_WAIT_REQ;
    }

    memset(s_otacmd, 0, sizeof(s_otacmd));
}

void ota_download_end_handler(ota_download_status_t *status,
                              ee_os_status_t        *ee_status)
{
    log_w("Change to OTA_DOWNLOAD_END state");

    int8_t retval = Key_Scan();
    if (1 == retval)
    {
        // diable all interrupts except NMI & RESET
        __disable_fault_irq();
        NVIC_SystemReset();
    }
    else
    {
        *status = OTA_DOWNLOAD_END;
    }
}

void ota_task_runnable(void *argument)
{
    /* USER CODE BEGIN key_task_runnable */
    void (*ota_download_state_machine[])(ota_download_status_t *,
                                         ee_os_status_t *) = {
        [OTA_WAIT_FOR_DOWNLOAD_REQ] = ota_wait_for_download_req_handler,
        [OTA_DOWNLOAD]              = ota_download_handler,
        [OTA_WAIT_REQ]              = ota_wait_req_handler,
        [OTA_DOWNLOAD_END]          = ota_download_end_handler,
    };
    ota_download_status_t ota_status = OTA_WAIT_FOR_DOWNLOAD_REQ;
    ee_os_status_t        ee_status  = OTA_EMPTY;
    ee_WriteBytes((uint8_t *)&ee_status, 0x00, 1);

    /* Infinite loop */
    for (;;)
    {
        ota_download_state_machine[ota_status](&ota_status, &ee_status);
    }
}

void DownloadAppData_task_runnable(void *argument)
{
    BaseType_t          retval = pdFALSE;
    Ymodem_RxContext_t *ctx    = NULL;


    retval = xQueueReceive(Queue_AppDataBuffer, &ctx, portMAX_DELAY);
    if (pdFALSE == retval || NULL == ctx)
    {
        log_e("Failed to receive app data size from queue");
        while (1)
        {
            
        }
    }
    log_i("W25Q: Received file size: %d bytes", ctx->size);
    xSemaphoreGive(Semaphore_ExtFlashState);

    for (;;)
    {
        retval = xQueueReceive(Queue_AppDataBuffer, &ctx, portMAX_DELAY);

        xSemaphoreTake(Semaphore_ExtFlashState, 0);
        if (NULL == ctx || pdFALSE == retval || ctx->size <= 0)
        {
            log_w("No more data to write or failed to receive data from queue, exiting task");
            xSemaphoreGive(Semaphore_ExtFlashState);
            continue;
        }

        W25Q64_WriteData(ctx->packet_data, ctx->packet_length);

        xSemaphoreGive(Semaphore_ExtFlashState);

    }
}
