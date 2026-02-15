#ifndef TBL_OS_TIME_H
#define TBL_OS_TIME_H

/* Sleep for ms (best effort). */
void tbl_sleep_ms(unsigned long ms);

#ifdef TBL_TIME_IMPLEMENTATION

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void tbl_sleep_ms(unsigned long ms)
{
    Sleep((DWORD)ms);
}

#else
#ifdef __PLAN9__
#include <u.h>
#include <libc.h>

/* Plan 9 sleep() is in milliseconds */
void tbl_sleep_ms(unsigned long ms)
{
    sleep((long)ms);
}

#else
#include <unistd.h>

/* POSIX sleep granularity: seconds; we do a simple ms->sec rounding up */
void tbl_sleep_ms(unsigned long ms)
{
    unsigned long sec;

    sec = ms / 1000UL;
    if ((ms % 1000UL) != 0UL) {
        sec++;
    }
    if (sec == 0UL) sec = 1UL;
    sleep((unsigned int)sec);
}
#endif
#endif

#endif /* TBL_TIME_IMPLEMENTATION */

#endif /* TBL_OS_TIME_H */
