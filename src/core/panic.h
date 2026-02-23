#ifndef TBL_CORE_PANIC_H
#define TBL_CORE_PANIC_H

/* Fail-fast for internal invariants / programmer errors. */
void tbl_panic(const char *file, int line, const char *msg);

#define TBL_PANIC(MSG) tbl_panic(__FILE__, __LINE__, (MSG))

#define TBL_REQUIRE(COND, MSG) do { \
    if (!(COND)) { \
        TBL_PANIC(MSG); \
    } \
} while (0)

#ifdef TBL_PANIC_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include "core/safe.h"

void tbl_panic(const char *file, int line, const char *msg)
{
    if (!file) file = "?";
    if (!msg)  msg  = "panic";

    {
        char nbuf[32];
        unsigned long ln;
        ln = (line < 0) ? 0UL : (unsigned long)line;
        if (!tbl_ul_to_dec_ok(ln, nbuf, sizeof(nbuf))) {
            (void)tbl_strlcpy(nbuf, "0", sizeof(nbuf));
        }
        (void)tbl_fputs5_ok(stderr, "tablinum: PANIC at ", file, ":", nbuf, ": ");
        (void)tbl_fputs2_ok(stderr, msg, "\n");
    }
    fflush(stderr);
    abort();
}

#endif /* TBL_PANIC_IMPLEMENTATION */

#endif /* TBL_CORE_PANIC_H */
