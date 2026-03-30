#ifndef DEBUG_H
#define DEBUG_H

#include <string.h>

#include "elog.h"

#define DEBUG                 (1)

/* Centralized log tags. Only tags defined here are allowed to output logs. */
#define MAIN_LOG_TAG          "APP_MAIN"
#define OTA_LOG_TAG           "APP_OTA"
#define AT24CXX_LOG_TAG       "APP_AT24CXX"
#define YMODEM_LOG_TAG        "APP_YMODEM"
#define YMODEM_FILE_LOG_TAG   "APP_YMODEM_FILE"
#define YMODEM_DATA_LOG_TAG   "APP_YMODEM_DATA"
#define YMODEM_PACKET_LOG_TAG "APP_YMODEM_PACKET"

static inline int debug_is_tag_allowed(const char *tag)
{
    if ((tag == NULL) || (tag[0] == '\0'))
    {
        return 0;
    }

    return (strcmp(tag, MAIN_LOG_TAG) == 0)              ||
           (strcmp(tag, OTA_LOG_TAG) == 0)               ||
           (strcmp(tag, AT24CXX_LOG_TAG) == 0)           ||
           (strcmp(tag, YMODEM_LOG_TAG) == 0)            ||
           (strcmp(tag, YMODEM_FILE_LOG_TAG) == 0)       ||
           (strcmp(tag, YMODEM_DATA_LOG_TAG) == 0)       ||
           (strcmp(tag, YMODEM_PACKET_LOG_TAG) == 0);
}

/*
 * Usage:
 * DEBUG_OUT(i, MAIN_LOG_TAG, "System init ok");
 * DEBUG_OUT(w, OTA_LOG_TAG, "Update request timeout");
 */
#define DEBUG_OUT(LEVEL, TAG, ...)                                            \
    do                                                                        \
    {                                                                         \
        const char *debug_tag__ = (TAG);                                      \
        if (DEBUG && debug_is_tag_allowed(debug_tag__))                       \
        {                                                                     \
            elog_##LEVEL(debug_tag__, __VA_ARGS__);                           \
        }                                                                     \
    } while (0)

void debug_init(void);

#endif /* DEBUG_H */
