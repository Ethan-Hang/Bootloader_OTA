#ifndef DEBUG_H
#define DEBUG_H

#include <string.h>

#include "elog.h"

#define DEBUG                       (1)

/* Centralized log tags. Only tags defined here are allowed to output logs. */
#define MAIN_LOG_TAG                                 "MAIN"
#define OTA_LOG_TAG                                   "OTA"
#define EEPROM_LOG_TAG                             "EEPROM"
#define YMODEM_LOG_TAG                             "Ymodem"
#define YMODEM_FILE_INFO_LOG_TAG                 "FileInfo"
#define YMODEM_FILE_DATA_LOG_TAG                 "FileData"
#define YMODEM_PACKET_LOG_TAG                      "Packet"
#define YMODEM_ERROR_LOG_TAG                        "Error"
#define YMODEM_RESULT_LOG_TAG                      "Result"
#define AT24CXX_LOG_TAG                           "AT24Cxx"

static inline int debug_is_tag_allowed(const char *tag)
{
    if ((tag == NULL) || (tag[0] == '\0'))
    {
        return 0;
    }

    return (strcmp(            MAIN_LOG_TAG, tag) == 0)                      ||
           (strcmp(             OTA_LOG_TAG, tag) == 0)                      ||
           (strcmp(          EEPROM_LOG_TAG, tag) == 0)                      ||
           (strcmp(          YMODEM_LOG_TAG, tag) == 0)                      ||
           (strcmp(YMODEM_FILE_INFO_LOG_TAG, tag) == 0)                      ||
           (strcmp(YMODEM_FILE_DATA_LOG_TAG, tag) == 0)                      ||
           (strcmp(   YMODEM_PACKET_LOG_TAG, tag) == 0)                      ||
           (strcmp(    YMODEM_ERROR_LOG_TAG, tag) == 0)                      ||
           (strcmp(   YMODEM_RESULT_LOG_TAG, tag) == 0)                      ||
           (strcmp(         AT24CXX_LOG_TAG, tag) == 0);
}

/*
 * Usage:
 * DEBUG_OUT(i, MAIN_LOG_TAG, "System init ok");
 * DEBUG_OUT(w, OTA_LOG_TAG, "Update request timeout");
 */
#define DEBUG_OUT(LEVEL, TAG, ...)                                            \
    do                                                                        \
    {                                                                         \
        if (DEBUG)                                                            \
        {                                                                     \
            const char *debug_tag__ = (TAG);                                  \
            if (DEBUG && debug_is_tag_allowed(debug_tag__))                   \
            {                                                                 \
                elog_##LEVEL(debug_tag__, __VA_ARGS__);                       \
            }                                                                 \
        }                                                                     \
                                                                              \
    } while (0)

void debug_init(void);

#endif /* DEBUG_H */
