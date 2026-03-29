#ifndef DEBUG_H
#define DEBUG_H

#include <stddef.h>
#include "elog.h"

#define DEBUG (1)

/*
 * Usage:
 * DEBUG_OUT(i, "", "Info message without custom tag");
 * DEBUG_OUT(w, "TAG1", "Warning message with custom tag");
 */
#define DEBUG_OUT(LEVEL, TAG, ...)                                             \
    do                                                                         \
    {                                                                          \
        if (DEBUG)                                                             \
        {                                                                      \
            if (((TAG) != NULL) && ((TAG)[0] != '\0'))                         \
            {                                                                  \
                elog_##LEVEL((TAG), __VA_ARGS__);                              \
            }                                                                  \
            else                                                               \
            {                                                                  \
                log_##LEVEL(__VA_ARGS__);                                      \
            }                                                                  \
        }                                                                      \
    } while (0)

void debug_init(void);

#endif /* DEBUG_H */
