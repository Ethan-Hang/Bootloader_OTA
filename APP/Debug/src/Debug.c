/**
 ******************************************************************************
 * @file Debug.c
 *
 * @par dependencies
 * - Debug.h
 *
 * @author Ethan-Hang
 *
 * @brief
 * EasyLogger runtime initialization wrapper.
 *
 * @version V1.0 2026-4-3
 *
 * @note 1 tab == 4 spaces!
 ******************************************************************************
 */

#include "Debug.h"

/**
 * @brief
 * Initialize logger backend and per-level output format.
 *
 * @param[in] : None.
 *
 * @param[out] : None.
 *
 * @return
 * None.
 * */
void debug_init(void)
{
#if DEBUG
    /**
     * Configure output format for all enabled log levels.
     **/
    elog_init();
    elog_set_fmt(ELOG_LVL_ASSERT, ELOG_FMT_LVL | ELOG_FMT_TAG);
    elog_set_fmt(ELOG_LVL_ERROR, ELOG_FMT_LVL | ELOG_FMT_TAG);
    elog_set_fmt(ELOG_LVL_WARN, ELOG_FMT_LVL | ELOG_FMT_TAG);
    elog_set_fmt(ELOG_LVL_INFO, ELOG_FMT_LVL | ELOG_FMT_TAG);
    elog_set_fmt(ELOG_LVL_DEBUG, ELOG_FMT_LVL | ELOG_FMT_TAG);
    elog_start();
#endif
}
