/******************************************************************************
 * @file OTA.c
 *
 * @par dependencies
 * - string.h
 * - OTA.h
 *
 * @author Ethan-Hang
 *
 * @brief
 * APP-side OTA command processing and Ymodem download state machine.
 *
 * Processing flow:
 * 1. Wait for OTA command from serial port.
 * 2. Receive firmware through Ymodem and write to external flash.
 * 3. Update EEPROM state and wait for apply-update request.
 * 4. Reboot to bootloader for image apply.
 *
 * @version V1.0 2025-4-2
 *
 * @note 1 tab == 4 spaces!
 *
 *****************************************************************************/


#include <string.h>

#include "OTA.h"

uint8_t              g_au8_YmodemRec[2][1030];
static uint8_t       s_otacmd[4];
extern QueueHandle_t Q_YmodemReclength;

QueueHandle_t     Queue_AppDataBuffer;
SemaphoreHandle_t Semaphore_ExtFlashState;


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


void DownloadAppData_task_runnable(void *argument);

/**
 * @brief
 * Trigger software reset and stop normal interrupts.
 *
 * @param[in]  : None.
 *
 * @param[out] : None.
 *
 * @return
 * None.
 * */
void soft_reset(void)
{
    // Disable all interrupts except NMI and RESET.
    __disable_fault_irq();
    NVIC_SystemReset();
}

/**
 * @brief
 * Release OTA download IPC resources and worker task.
 *
 * @param[in]  : None.
 *
 * @param[out] : None.
 *
 * @return
 * None.
 * */
static void ota_cleanup_download_resources(void)
{
    Ymodem_RxContext_t *stop_ctx = NULL;

    if ((DownloadAppData_taskHandle != NULL) && (Queue_AppDataBuffer != NULL))
    {
        if (xQueueSendToBack(Queue_AppDataBuffer, &stop_ctx, portMAX_DELAY) !=
            pdPASS)
        {
            DEBUG_OUT(e, OTA_LOG_TAG,
                      "Failed to send stop signal to download task");
        }
        else if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2000)) == 0)
        {
            DEBUG_OUT(w, OTA_LOG_TAG, "Wait download task exit timeout");
        }
    }

    if (Queue_AppDataBuffer != NULL)
    {
        vQueueDelete(Queue_AppDataBuffer);
        Queue_AppDataBuffer = NULL;
    }
    if (Semaphore_ExtFlashState != NULL)
    {
        vSemaphoreDelete(Semaphore_ExtFlashState);
        Semaphore_ExtFlashState = NULL;
    }

    DownloadAppData_taskHandle = NULL;
}

/**
 * @brief
 * Scan key input with debounce and timeout.
 *
 * @param[in]  : None.
 *
 * @param[out] : None.
 *
 * @return
 * 1 when key is pressed, -1 on timeout.
 * */
static int8_t Key_Scan(void)
{
    DEBUG_OUT(d, OTA_LOG_TAG, "Scanning for key press...");
    for (uint32_t i = 0; i < 2000; i++)
    {
        osDelay(20);
        if (GPIO_PIN_RESET == HAL_GPIO_ReadPin(Key_GPIO_Port, Key_Pin))
        {
            osDelay(20);
            if (GPIO_PIN_RESET == HAL_GPIO_ReadPin(Key_GPIO_Port, Key_Pin))
            {
                DEBUG_OUT(d, OTA_LOG_TAG, "Key press detected");

                return 1;
            }
        }
        osDelay(80);
    }
    return -1;
}

/**
 * @brief
 * Wait for download command and prepare OTA resources.
 *
 * @param[in]  status    : Pointer to OTA runtime state.
 *
 * @param[in]  ee_status : Pointer to EEPROM OTA state.
 *
 * @param[out] : None.
 *
 * @return
 * None.
 * */
void ota_wait_for_download_req_handler(ota_download_status_t *status,
                                       ee_os_status_t        *ee_status)
{
    DEBUG_OUT(v, OTA_LOG_TAG, "Change to OTA_WAIT_FOR_DOWNLOAD_REQ state");

    HAL_UART_Transmit(&huart1, (uint8_t *)"Waiting for download request...\r\n",
                      33, HAL_MAX_DELAY);
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, s_otacmd, 4);
    __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);

    uint16_t rec_length = 0;
    xQueueReceive(Q_YmodemReclength, &rec_length, portMAX_DELAY);

    DEBUG_OUT(i, OTA_LOG_TAG, "received command %x %x %x, length: %d",
              s_otacmd[0], s_otacmd[1], s_otacmd[2], rec_length);

    if (3 == rec_length)
    {
        if (s_otacmd[0] == 0x11 && s_otacmd[1] == 0x22 && s_otacmd[2] == 0x33)
        {
            /* Ensure each OTA session starts writing from external flash offset
             * 0. */
            W25Q64_Init();
            DEBUG_OUT(d, OTA_LOG_TAG,
                      "W25Q handler state reset for new download");

            *status             = OTA_DOWNLOAD;

            Queue_AppDataBuffer = xQueueCreate(4, sizeof(Ymodem_RxContext_t *));
            Semaphore_ExtFlashState = xSemaphoreCreateBinary();
            if (NULL == Queue_AppDataBuffer || NULL == Semaphore_ExtFlashState)
            {
                DEBUG_OUT(e, OTA_LOG_TAG, "Failed to create OTA IPC resources");
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

            xTaskNotifyStateClear((TaskHandle_t)OTA_taskHandle);
            DownloadAppData_taskHandle =
                osThreadNew(DownloadAppData_task_runnable, NULL,
                            &DownloadAppData_task_attributes);
            if (NULL == DownloadAppData_taskHandle)
            {
                DEBUG_OUT(e, OTA_LOG_TAG,
                          "Failed to create DownloadAppData task");
                vQueueDelete(Queue_AppDataBuffer);
                Queue_AppDataBuffer = NULL;
                vSemaphoreDelete(Semaphore_ExtFlashState);
                Semaphore_ExtFlashState = NULL;
                *status                 = OTA_WAIT_FOR_DOWNLOAD_REQ;
                return;
            }

            *ee_status = EE_OTA_DOWNLOADING;
            ee_WriteBytes((uint8_t *)ee_status, 0x00, 1);
            DEBUG_OUT(d, OTA_LOG_TAG,
                      "EEPROM status set to EE_OTA_DOWNLOADING");
        }
        else
        {
            HAL_UART_AbortReceive(&huart1);
            *status = OTA_WAIT_FOR_DOWNLOAD_REQ;
        }
    }
    else
    {
        HAL_UART_AbortReceive(&huart1);
        *status = OTA_WAIT_FOR_DOWNLOAD_REQ;
    }

    memset(s_otacmd, 0, sizeof(s_otacmd));
}

/**
 * @brief
 * Receive firmware data and update OTA status in EEPROM.
 *
 * @param[in]  status    : Pointer to OTA runtime state.
 *
 * @param[in]  ee_status : Pointer to EEPROM OTA state.
 *
 * @param[out] : None.
 *
 * @return
 * None.
 * */
void ota_download_handler(ota_download_status_t *status,
                          ee_os_status_t        *ee_status)
{
    DEBUG_OUT(v, OTA_LOG_TAG, "Change to OTA_DOWNLOAD state");

    int32_t app_data_length = Ymodem_Receive(g_au8_YmodemRec);
    W25Q64_WriteData_End();

    if (app_data_length > 0)
    {
        *status = OTA_WAIT_REQ;
        DEBUG_OUT(i, OTA_LOG_TAG, "Received app data, length: %d",
                  app_data_length);

        *ee_status = EE_OTA_DOWNLOAD_FINISHED;
    }
    else
    {
        *status    = OTA_WAIT_FOR_DOWNLOAD_REQ;
        *ee_status = EE_OTA_NO_APP_UPDATE;
    }

    uint8_t        wr_ok = ee_WriteBytes((uint8_t *)ee_status, 0x00, 1);
    ee_os_status_t ee_read_status           = EE_OTA_NO_APP_UPDATE;
    int32_t        app_data_length_readback = 0;
    uint8_t        rd_ok = ee_ReadBytes((uint8_t *)&ee_read_status, 0x00, 1);
    if ((wr_ok == 0) || (rd_ok == 0))
    {
        DEBUG_OUT(e, OTA_LOG_TAG, "EEPROM transaction failed (wr=%d, rd=%d)",
                  wr_ok, rd_ok);
    }
    DEBUG_OUT(d, OTA_LOG_TAG, "EE wrote 0x%x, EE read status: 0x%x", *ee_status,
              ee_read_status);

    if (ee_read_status != *ee_status)
    {
        DEBUG_OUT(e, OTA_LOG_TAG, "EEPROM read-back verification failed");
    }

    wr_ok = ee_WriteBytes((uint8_t *)&app_data_length, 0x01,
                          sizeof(app_data_length));
    rd_ok = ee_ReadBytes((uint8_t *)&app_data_length_readback, 0x01,
                         sizeof(app_data_length_readback));
    if ((wr_ok == 0) || (rd_ok == 0))
    {
        DEBUG_OUT(e, OTA_LOG_TAG, "EEPROM transaction failed (wr=%d, rd=%d)",
                  wr_ok, rd_ok);
    }
    DEBUG_OUT(d, OTA_LOG_TAG,
              "EE wrote app data length 0x%x, EE read back 0x%x",
              app_data_length, app_data_length_readback);
    if (app_data_length_readback != app_data_length)
    {
        DEBUG_OUT(e, OTA_LOG_TAG,
                  "EEPROM read-back verification failed for app data length");
    }

    osDelay(1000);
    ota_cleanup_download_resources();
}

/**
 * @brief
 * Wait for update-apply request after download is finished.
 *
 * @param[in]  status    : Pointer to OTA runtime state.
 *
 * @param[in]  ee_status : Pointer to EEPROM OTA state.
 *
 * @param[out] : None.
 *
 * @return
 * None.
 * */
void ota_wait_req_handler(ota_download_status_t *status,
                          ee_os_status_t        *ee_status)
{

    HAL_UART_Transmit(&huart1, (uint8_t *)"Waiting for update request...\r\n",
                      32, HAL_MAX_DELAY);
    // Start DMA reception to receive post-download update command.
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, s_otacmd, 4);
    __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);

    uint16_t rec_length = 0;
    xQueueReceive(Q_YmodemReclength, &rec_length, portMAX_DELAY);
    if (3 == rec_length)
    {
        if (s_otacmd[0] == 0x77 && s_otacmd[1] == 0x88 && s_otacmd[2] == 0x99)
        {
            *status = OTA_DOWNLOAD_END;
            HAL_UART_Transmit(
                &huart1,
                (uint8_t *)"Update request received, "
                           "press key to reboot and apply update\r\n",
                53, HAL_MAX_DELAY);
        }
        else
        {
            HAL_UART_AbortReceive(&huart1);
            *status = OTA_WAIT_REQ;
        }
    }
    else
    {
        HAL_UART_AbortReceive(&huart1);
        *status = OTA_WAIT_REQ;
    }

    memset(s_otacmd, 0, sizeof(s_otacmd));
}

/**
 * @brief
 * Handle reboot confirmation stage after OTA download.
 *
 * @param[in]  status    : Pointer to OTA runtime state.
 *
 * @param[in]  ee_status : Pointer to EEPROM OTA state.
 *
 * @param[out] : None.
 *
 * @return
 * None.
 * */
void ota_download_end_handler(ota_download_status_t *status,
                              ee_os_status_t        *ee_status)
{
    DEBUG_OUT(v, OTA_LOG_TAG, "Change to OTA_DOWNLOAD_END state");

    int8_t retval = Key_Scan();
    if (1 == retval)
    {
        soft_reset();
    }
    else
    {
        *status = OTA_DOWNLOAD_END;
    }
}

/**
 * @brief
 * OTA state machine task entry.
 *
 * @param[in]  argument  : RTOS thread argument, not used.
 *
 * @param[out] : None.
 *
 * @return
 * None.
 * */
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
    ee_os_status_t        ee_status  = EE_OTA_NO_APP_UPDATE;
    ee_WriteBytes((uint8_t *)&ee_status, EE_OTA_NO_APP_UPDATE, 1);

    /* Infinite loop. */
    for (;;)
    {
        ota_download_state_machine[ota_status](&ota_status, &ee_status);
    }
}

/**
 * @brief
 * Download worker task, writes Ymodem payload into external flash.
 *
 * @param[in]  argument  : RTOS thread argument, not used.
 *
 * @param[out] : None.
 *
 * @return
 * None.
 * */
void DownloadAppData_task_runnable(void *argument)
{
    BaseType_t          retval = pdFALSE;
    Ymodem_RxContext_t *ctx    = NULL;

    retval = xQueueReceive(Queue_AppDataBuffer, &ctx, portMAX_DELAY);
    if (NULL == ctx || pdFALSE == retval || ctx->size <= 0)
    {
        DEBUG_OUT(e, OTA_LOG_TAG, "Failed to receive app data size from queue");
        xTaskNotifyGive((TaskHandle_t)OTA_taskHandle);
        vTaskDelete(NULL);
    }
    DEBUG_OUT(i, OTA_LOG_TAG, "W25Q: Received file size: %d bytes", ctx->size);
    xSemaphoreGive(Semaphore_ExtFlashState);

    for (;;)
    {
        retval = xQueueReceive(Queue_AppDataBuffer, &ctx, portMAX_DELAY);

        if (pdFALSE == retval)
        {
            continue;
        }

        if (NULL == ctx)
        {
            DEBUG_OUT(i, OTA_LOG_TAG,
                      "DownloadAppData task received stop signal");
            break;
        }

        if (ctx->size <= 0)
        {
            DEBUG_OUT(
                w, OTA_LOG_TAG,
                "No more data to write or failed to receive data from queue, "
                "exiting task");
            xSemaphoreGive(Semaphore_ExtFlashState);
            continue;
        }

        W25Q64_WriteData(ctx->packet_data + PACKET_HEADER, ctx->packet_length);

        xSemaphoreGive(Semaphore_ExtFlashState);
    }

    xTaskNotifyGive((TaskHandle_t)OTA_taskHandle);
    vTaskDelete(NULL);
}
