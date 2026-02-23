#ifndef TBL_CORE_LOG_H
#define TBL_CORE_LOG_H

#include <stdarg.h>
#include "core/safe.h"

typedef enum tbl_log_level_e {
    TBL_LOG_ERROR = 0,
    TBL_LOG_WARN  = 1,
    TBL_LOG_INFO  = 2,
    TBL_LOG_DEBUG = 3
} tbl_log_level_t;

/* Global log level (process-wide). */
void tbl_log_set_level(tbl_log_level_t lvl);
tbl_log_level_t tbl_log_get_level(void);

/* printf-style logging (fmt MUST be trusted). */
void tbl_logf(tbl_log_level_t lvl, const char *fmt, ...);
void tbl_vlogf(tbl_log_level_t lvl, const char *fmt, va_list ap);

#ifdef TBL_LOG_IMPLEMENTATION

#include <stdio.h>
#include "core/safe.h"

static tbl_log_level_t g_tbl_log_level = TBL_LOG_INFO;
static FILE *g_tbl_log_fp = NULL;

void tbl_log_set_level(tbl_log_level_t lvl)
{
    g_tbl_log_level = lvl;
}

tbl_log_level_t tbl_log_get_level(void)
{
    return g_tbl_log_level;
}

static const char *tbl_log_level_str(tbl_log_level_t lvl)
{
    switch (lvl) {
        case TBL_LOG_ERROR: return "ERROR";
        case TBL_LOG_WARN:  return "WARN";
        case TBL_LOG_INFO:  return "INFO";
        case TBL_LOG_DEBUG: return "DEBUG";
        default:            return "LOG";
    }
}

void tbl_vlogf(tbl_log_level_t lvl, const char *fmt, va_list ap)
{
    FILE *fp;

    if ((int)lvl > (int)g_tbl_log_level) {
        return;
    }

    fp = g_tbl_log_fp ? g_tbl_log_fp : stderr;

    if (!fmt) fmt = "";

        (void)tbl_fputs3_ok(fp, "[", tbl_log_level_str(lvl), "] ");
    (void)tbl_vfprintf_ok(fp, fmt, ap);
        (void)tbl_fputc_ok(fp, '\n');
    (void)tbl_fflush_ok(fp);
}

void tbl_logf(tbl_log_level_t lvl, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    tbl_vlogf(lvl, fmt, ap);
    va_end(ap);
}

#endif /* TBL_LOG_IMPLEMENTATION */

#endif /* TBL_CORE_LOG_H */
