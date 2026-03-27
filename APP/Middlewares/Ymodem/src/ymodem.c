/* Includes ------------------------------------------------------------------*/
#include "common.h"
#include "elog.h"
#include "main.h"
#include "usart.h"
#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
uint8_t              file_name[FILE_NAME_LENGTH];
uint32_t             EraseCounter = 0x0;
uint32_t             NbrOfPage    = 0;
uint32_t             RamSource;
extern uint8_t       tab_1024[1024];

extern QueueHandle_t Q_YmodemReclength;
extern QueueHandle_t Queue_AppDataBuffer;
static uint16_t      s_u16_YmodRecLength;


/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

static void          Int2Str(uint8_t *str, int32_t intnum)
{
    uint32_t i, Div = 1000000000, j = 0, Status = 0;

    for (i = 0; i < 10; i++)
    {
        str[j++] = (intnum / Div) + 48;

        intnum   = intnum % Div;
        Div /= 10;
        if ((str[j - 1] == '0') & (Status == 0))
        {
            j = 0;
        }
        else
        {
            Status++;
        }
    }
}

static uint32_t Str2Int(uint8_t *inputstr, int32_t *intnum)
{
    uint32_t i = 0, res = 0;
    uint32_t val = 0;

    if (inputstr[0] == '0' && (inputstr[1] == 'x' || inputstr[1] == 'X'))
    {
        if (inputstr[2] == '\0')
        {
            return 0;
        }
        for (i = 2; i < 11; i++)
        {
            if (inputstr[i] == '\0')
            {
                *intnum = val;
                // 返回1
                res     = 1;
                break;
            }
            if (ISVALIDHEX(inputstr[i]))
            {
                val = (val << 4) + CONVERTHEX(inputstr[i]);
            }
            else
            {
                // 无效输入返回0
                res = 0;
                break;
            }
        }

        if (i >= 11)
        {
            res = 0;
        }
    }
    else // 最多10为2输入
    {
        for (i = 0; i < 11; i++)
        {
            if (inputstr[i] == '\0')
            {
                *intnum = val;
                // 返回1
                res     = 1;
                break;
            }
            else if ((inputstr[i] == 'k' || inputstr[i] == 'K') && (i > 0))
            {
                val     = val << 10;
                *intnum = val;
                res     = 1;
                break;
            }
            else if ((inputstr[i] == 'm' || inputstr[i] == 'M') && (i > 0))
            {
                val     = val << 20;
                *intnum = val;
                res     = 1;
                break;
            }
            else if (ISVALIDDEC(inputstr[i]))
            {
                val = val * 10 + CONVERTDEC(inputstr[i]);
            }
            else
            {
                // 无效输入返回0
                res = 0;
                break;
            }
        }
        // 超过10位无效，返回0
        if (i >= 11)
        {
            res = 0;
        }
    }

    return res;
}


/**
 * @brief  Receive byte from sender
 * @param  c: Character
 * @param  timeout: Timeout
 * @retval 0: Byte received
 *         -1: Timeout
 */
static int32_t Receive_Byte(uint8_t *c, uint16_t length, uint32_t timeout)
{
    HAL_StatusTypeDef hal_ret;

    /* Make sure UART RX DMA is in IDLE mode so RxEvent callback can report
     * actual frame length. */
    hal_ret = HAL_UARTEx_ReceiveToIdle_DMA(&huart1, c, length);
    if (hal_ret != HAL_OK)
    {
        HAL_UART_DMAStop(&huart1);
        hal_ret = HAL_UARTEx_ReceiveToIdle_DMA(&huart1, c, length);
        if (hal_ret != HAL_OK)
        {
            return YMODEM_PKT_TIMEOUT;
        }
    }

    BaseType_t retval = pdFALSE;
    retval            = xQueueReceive(Q_YmodemReclength, &s_u16_YmodRecLength,
                                      pdMS_TO_TICKS(timeout));
    if (pdFALSE == retval)
    {
        return YMODEM_PKT_TIMEOUT; /* Timeout */
    }
    return YMODEM_PKT_SUCCESS;     /* Byte received */
}

/**
 * @brief  Send a byte
 * @param  c: Character
 * @retval 0: Byte sent
 */
static uint32_t Send_Byte(uint8_t c)
{
    // SerialPutChar(c);
    HAL_UART_Transmit(&huart1, &c, 1, HAL_MAX_DELAY);
    return 0;
}

/**
 * @brief  Receive a packet from sender
 * @param  data
 * @param  length
 * @param  timeout
 *     0: end of transmission
 *    -1: abort by sender
 *    >0: packet length
 * @retval Ymodem_PacketStatus_t: SUCCESS/TIMEOUT/ABORT
 */
static int32_t Receive_Packet(uint8_t *data, int32_t *length, uint32_t timeout)
{
    uint16_t packet_size;
    uint8_t  c = 0;
    *length    = 0;
    if (Receive_Byte(data, 1030, timeout) != 0)
    {
        return (int32_t)YMODEM_PKT_TIMEOUT;
    }

    c = data[0];
    switch (c)
    {
    case SOH:
        packet_size = PACKET_SIZE;
        break;
    case STX:
        packet_size = PACKET_1K_SIZE;
        break;
    case EOT:
        return (int32_t)YMODEM_PKT_SUCCESS;
    case CA:
        if ((Receive_Byte(&c, 1, timeout) == 0) && (c == CA))
        {
            *length = -1;
            log_i("Ymodem", "Received double CA, aborting transfer...");
            return (int32_t)YMODEM_PKT_SUCCESS;
        }
        else
        {
            log_w("Ymodem", "Single CA received, waiting for second CA...");
            return (int32_t)YMODEM_PKT_TIMEOUT;
        }
    case ABORT1:
    case ABORT2:
        return (int32_t)YMODEM_PKT_ABORT;
    default:
        return (int32_t)YMODEM_PKT_TIMEOUT;
    }
    if (s_u16_YmodRecLength != (packet_size + PACKET_OVERHEAD))
    {
        return (int32_t)YMODEM_PKT_TIMEOUT;
    }

    if (data[PACKET_SEQNO_INDEX] !=
        ((data[PACKET_SEQNO_COMP_INDEX] ^ 0xff) & 0xff))
    {
        return (int32_t)YMODEM_PKT_TIMEOUT;
    }
    *length = packet_size;
    return (int32_t)YMODEM_PKT_SUCCESS;
}

/* Private state machine handler functions ---------------------------------*/

/**
 * @brief  Handle filename packet in receive state machine
 * @param  ctx: Receive context
 * @retval Ymodem_RxHandlerStatus_t: HANDLER_ERROR/CONTINUE/DONE
 */
static int32_t Ymodem_RxState_FileInfo(Ymodem_RxContext_t *ctx)
{
    /* Filename packet */
    if (ctx->packet_data[PACKET_HEADER] != 0)
    {
        /* Filename packet has valid data */
        for (ctx->i = 0, ctx->file_ptr = ctx->packet_data + PACKET_HEADER;
             (*ctx->file_ptr != 0) && (ctx->i < FILE_NAME_LENGTH);)
        {
            file_name[ctx->i++] = *ctx->file_ptr++;
        }
        file_name[ctx->i++] = '\0';
        for (ctx->i = 0, ctx->file_ptr++;
             (*ctx->file_ptr != ' ') && (ctx->i < FILE_SIZE_LENGTH);)
        {
            ctx->file_size[ctx->i++] = *ctx->file_ptr++;
        }
        ctx->file_size[ctx->i++] = '\0';
        Str2Int(ctx->file_size, &ctx->size);

        // BaseType_t      retval    = pdFALSE;
        // Ymodem_QueueMsg_t queue_msg;

        // queue_msg.type       = YMODEM_QMSG_FILE_INFO;
        // queue_msg.data_len   = 0;
        // queue_msg.file_size  = ctx->size;

        // retval = xQueueSendToBack(Queue_AppDataBuffer, &queue_msg,
        //                           0);
        // if (pdFALSE == retval)
        // {
        //     log_e("Ymodem", "Failed to send file info to queue");
        //     return (int32_t)YMODEM_RX_HANDLER_ERROR;
        // }

        
        if (ctx->buf[0] == ctx->packet_data)
        {
            ctx->packet_data = ctx->buf[1];
        }
        else
        {
            ctx->packet_data = ctx->buf[0];
        }

         /* Debug: Print received filename and size */

        /* Debug: Print received file size */
        elog_debug("FileInfo", "File size: %d bytes", ctx->size);

        Send_Byte(ACK);
        Send_Byte(CRC16);
        elog_debug("FileInfo", "Transition to FILE_DATA state");
        ctx->bytes_received = 0; /* Reset byte counter for new file */
        ctx->state          = YMODEM_RX_STATE_FILE_DATA;
        return (int32_t)YMODEM_RX_HANDLER_CONTINUE;
    }
    /* Filename packet is empty, end session */
    else
    {
        Send_Byte(ACK);
        elog_info("Ymodem", "Session complete, file transfer successful");
        ctx->file_done    = 1;
        ctx->session_done = 1;
        return (int32_t)YMODEM_RX_HANDLER_DONE;
    }
}

/**
 * @brief  Handle file data packet in receive state machine
 * @param  ctx: Receive context
 * @retval Ymodem_RxHandlerStatus_t: always CONTINUE
 */
static int32_t Ymodem_RxState_FileData(Ymodem_RxContext_t *ctx)
{
    // Only process positive packet lengths (actual data)
    if (ctx->packet_length > 0)
    {
        // Calculate bytes to copy (don't exceed total file size)
        int32_t bytes_to_copy = ctx->packet_length;
        if ((ctx->bytes_received + bytes_to_copy) > ctx->size)
        {
            bytes_to_copy = ctx->size - ctx->bytes_received;
        }

        // BaseType_t      retval    = pdFALSE;
        // Ymodem_QueueMsg_t queue_msg;

        // queue_msg.type        = YMODEM_QMSG_DATA;
        // queue_msg.data_len    = (uint32_t)bytes_to_copy;
        // queue_msg.file_size   = ctx->size;
        // memcpy(queue_msg.packet_data, ctx->packet_data + PACKET_HEADER,
        //      queue_msg.data_len);

        // retval = xQueueSendToBack(Queue_AppDataBuffer, &queue_msg,
        //                           0);
        // if (pdFALSE == retval)
        // {
        //     log_e("Ymodem", "Failed to send file data to queue");
        //     return (int32_t)YMODEM_RX_HANDLER_ERROR;
        // }
         /* Debug: Print received packet info */

        if (ctx->buf[0] == ctx->packet_data)
        {
            ctx->packet_data = ctx->buf[1];
        }
        else
        {
            ctx->packet_data = ctx->buf[0];
        }

        ctx->bytes_received += bytes_to_copy;

        // Calculate and display progress (integer math, no float)
        uint32_t progress_percent = (ctx->bytes_received * 100) / ctx->size;
        elog_debug("FileData", "Progress: %d/%d bytes (%d%%)",
                   ctx->bytes_received, ctx->size, progress_percent);
    }

    Send_Byte(ACK);
    return (int32_t)YMODEM_RX_HANDLER_CONTINUE;
}

/**
 * @brief  Receive a file using the ymodem protocol (FSM version)
 * @param  buf: Address of the first byte
 * @retval Ymodem_ReceiveStatus_t: File size on success, negative on error
 *    - YMODEM_RX_ABORTED: Abort by sender (0)
 *    - YMODEM_RX_SIZE_ERR: Image size exceeds Flash size (-1)
 *    - YMODEM_RX_TIMEOUT_ERR: Max retries exceeded (0)
 *    - YMODEM_RX_FLASH_ERR: Flash programming error (-2)
 *    - YMODEM_RX_USER_ABORT: User abort with Ctrl+C (-3)
 */
int32_t Ymodem_Receive(uint8_t (*buf)[1030])
{
    Ymodem_RxContext_t ctx;
    /* State handler function pointer array */
    int32_t (*state_handlers[])(Ymodem_RxContext_t *) = {
        Ymodem_RxState_FileInfo, /* YMODEM_RX_STATE_FILE_INFO */
        Ymodem_RxState_FileData  /* YMODEM_RX_STATE_FILE_DATA */
    };
    int32_t rx_result;
    int32_t state_result;

    /* Initialize context */
    ctx.buf              = buf;
    ctx.buf_ptr          = buf[0];
    ctx.packet_data      = ctx.buf_ptr;
    ctx.size             = 0;
    ctx.bytes_received   = 0;
    ctx.errors           = 0;
    ctx.session_done     = 0;
    ctx.packets_received = 0;
    ctx.session_begin    = 0;
    ctx.state            = YMODEM_RX_STATE_FILE_INFO;
    /* Initialize FlashDestination variable */
    // FlashDestination     = BackAppAddress;

    elog_info("Ymodem", "Starting reception... (bufA @0x%08x)(bufB @0x%08x)",
              (uint32_t)buf[0], (uint32_t)buf[1]);

    while (1)
    {
        for (ctx.packets_received = 0, ctx.file_done = 0;;)
        {
            rx_result = Receive_Packet(ctx.packet_data, &ctx.packet_length,
                                       NAK_TIMEOUT);

            switch (rx_result)
            {
            case YMODEM_PKT_SUCCESS:
                /* Packet received successfully */
                ctx.errors = 0;
                switch (ctx.packet_length)
                {
                /* Abort by sender */
                case -1:
                    Send_Byte(ACK);
                    return (int32_t)YMODEM_RX_ABORTED;
                /* End of transmission */
                case 0:
                    Send_Byte(ACK);
                    if (ctx.state == YMODEM_RX_STATE_FILE_DATA)
                    {
                        /* First EOT: transition back to FILE_INFO for EOF
                         * packet */
                        ctx.state     = YMODEM_RX_STATE_FILE_INFO;
                        ctx.file_done = 1;
                        elog_debug("Ymodem",
                                   "EOT received, waiting for EOF packet...");
                    }
                    else if (ctx.state == YMODEM_RX_STATE_FILE_INFO)
                    {
                        /* Second EOT or EOF packet received, end session */
                        ctx.file_done    = 1;
                        ctx.session_done = 1;
                        elog_info("Ymodem",
                                  "Session complete, file transfer successful");
                    }
                    break;
                /* Normal packet */
                default:
                {
                    uint8_t pkt_seqno =
                        ctx.packet_data[PACKET_SEQNO_INDEX] & 0xff;
                    uint8_t exp_seqno = ctx.packets_received & 0xff;

                    /* Debug: Print sequence number */
                    elog_debug("Packet", "Seqno: %d Expected: %d", pkt_seqno,
                               exp_seqno);

                    if (pkt_seqno != exp_seqno)
                    {
                        elog_warn("Packet", "Sequence mismatch, sending NAK");
                        Send_Byte(NAK);
                    }
                    else
                    {
                        /* Call state handler function pointer */
                        state_result = state_handlers[ctx.state](&ctx);
                        if (state_result == (int32_t)YMODEM_RX_HANDLER_ERROR)
                        {
                            return state_result;
                        }
                        if (state_result == (int32_t)YMODEM_RX_HANDLER_DONE)
                        {
                            /* State handler indicates completion */
                            break;
                        }
                        /* YMODEM_RX_HANDLER_CONTINUE: proceed normally */

                        ctx.packets_received++;
                        ctx.session_begin = 1;
                    }
                }
                }
                break;
            case YMODEM_PKT_ABORT:
                /* User abort */
                Send_Byte(CA);
                Send_Byte(CA);
                return (int32_t)YMODEM_RX_USER_ABORT;
            case YMODEM_PKT_TIMEOUT:
            default:
                /* Timeout or packet error */
                if (ctx.session_begin > 0)
                {
                    ctx.errors++;
                    elog_warn("Error", "Timeout/Error, count: %d/%d",
                              ctx.errors, MAX_ERRORS);
                }
                if (ctx.errors > MAX_ERRORS)
                {
                    elog_error("Error", "Max errors exceeded!");
                    Send_Byte(CA);
                    Send_Byte(CA);
                    return (int32_t)YMODEM_RX_TIMEOUT_ERR;
                }
                Send_Byte(CRC16);
                break;
            }
            if (ctx.file_done != 0)
            {
                break;
            }
        }
        if (ctx.session_done != 0)
        {
            break;
        }

        /* If file transfer complete but session not done, request next packet
         */
        /* (handles second EOT or empty filename packet per Ymodem protocol) */
        if (ctx.file_done != 0 && ctx.session_done == 0)
        {
            Send_Byte(CRC16);  /* Request next packet */
            ctx.file_done = 0; /* Reset to allow new file processing */
            ctx.errors    = 0; /* Reset error count for new packet reception */
            elog_debug("Ymodem", "Requesting next packet with CRC16...");
        }
    }

    /* Debug: Final result */
    elog_info("Result", "Total bytes received: %d", ctx.bytes_received);

    /* Complete the last incomplete block in external flash */
    // W25Q64_WriteData_End();

    return (int32_t)ctx.bytes_received; /* Return actual bytes received */
}

/**
 * @brief  check response using the ymodem protocol
 * @param  buf: Address of the first byte
 * @retval The size of the file
 */
int32_t Ymodem_CheckResponse(uint8_t c)
{
    return 0;
}

/**
 * @brief  Prepare the first block
 * @param  timeout
 *     0: end of transmission
 */
void Ymodem_PrepareIntialPacket(uint8_t *data, const uint8_t *fileName,
                                uint32_t *length)
{
    uint16_t i, j;
    uint8_t  file_ptr[10];

    /* Make first three packet */
    data[0] = SOH;
    data[1] = 0x00;
    data[2] = 0xff;

    /* Filename packet has valid data */
    for (i = 0; (fileName[i] != '\0') && (i < FILE_NAME_LENGTH); i++)
    {
        data[i + PACKET_HEADER] = fileName[i];
    }

    data[i + PACKET_HEADER] = 0x00;

    Int2Str(file_ptr, *length);
    for (j = 0, i = i + PACKET_HEADER + 1; file_ptr[j] != '\0';)
    {
        data[i++] = file_ptr[j++];
    }

    for (j = i; j < PACKET_SIZE + PACKET_HEADER; j++)
    {
        data[j] = 0;
    }
}

/**
 * @brief  Prepare the data packet
 * @param  timeout
 *     0: end of transmission
 */
void Ymodem_PreparePacket(uint8_t *SourceBuf, uint8_t *data, uint8_t pktNo,
                          uint32_t sizeBlk)
{
    uint16_t i, size, packetSize;
    uint8_t *file_ptr;

    /* Make first three packet */
    packetSize = sizeBlk >= PACKET_1K_SIZE ? PACKET_1K_SIZE : PACKET_SIZE;
    size       = sizeBlk < packetSize ? sizeBlk : packetSize;
    if (packetSize == PACKET_1K_SIZE)
    {
        data[0] = STX;
    }
    else
    {
        data[0] = SOH;
    }
    data[1]  = pktNo;
    data[2]  = (~pktNo);
    file_ptr = SourceBuf;

    /* Filename packet has valid data */
    for (i = PACKET_HEADER; i < size + PACKET_HEADER; i++)
    {
        data[i] = *file_ptr++;
    }
    if (size <= packetSize)
    {
        for (i = size + PACKET_HEADER; i < packetSize + PACKET_HEADER; i++)
        {
            data[i] = 0x1A; /* EOF (0x1A) or 0x00 */
        }
    }
}

/**
 * @brief  Update CRC16 for input byte
 * @param  CRC input value
 * @param  input byte
 * @retval None
 */
uint16_t UpdateCRC16(uint16_t crcIn, uint8_t byte)
{
    uint32_t crc = crcIn;
    uint32_t in  = byte | 0x100;
    do
    {
        crc <<= 1;
        in <<= 1;
        if (in & 0x100)
            ++crc;
        if (crc & 0x10000)
            crc ^= 0x1021;
    } while (!(in & 0x10000));
    return crc & 0xffffu;
}


/**
 * @brief  Cal CRC16 for YModem Packet
 * @param  data
 * @param  length
 * @retval None
 */
uint16_t Cal_CRC16(const uint8_t *data, uint32_t size)
{
    uint32_t       crc     = 0;
    const uint8_t *dataEnd = data + size;
    while (data < dataEnd)
        crc = UpdateCRC16(crc, *data++);

    crc = UpdateCRC16(crc, 0);
    crc = UpdateCRC16(crc, 0);
    return crc & 0xffffu;
}

/**
 * @brief  Cal Check sum for YModem Packet
 * @param  data
 * @param  length
 * @retval None
 */
uint8_t CalChecksum(const uint8_t *data, uint32_t size)
{
    uint32_t       sum     = 0;
    const uint8_t *dataEnd = data + size;
    while (data < dataEnd)
        sum += *data++;
    return sum & 0xffu;
}

/**
 * @brief  Transmit a data packet using the ymodem protocol
 * @param  data
 * @param  length
 * @retval None
 */
void Ymodem_SendPacket(uint8_t *data, uint16_t length)
{
    uint16_t i;
    i = 0;
    while (i < length)
    {
        Send_Byte(data[i]);
        i++;
    }
}

/*******************(C)COPYRIGHT 2010 STMicroelectronics *****END OF FILE****/
