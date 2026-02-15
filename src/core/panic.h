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

void tbl_panic(const char *file, int line, const char *msg)
{
    if (!file) file = "?";
    if (!msg)  msg  = "panic";

    fprintf(stderr, "tablinum: PANIC at %s:%d: %s\n", file, line, msg);
    fflush(stderr);
    abort();
}

#endif /* TBL_PANIC_IMPLEMENTATION */

#endif /* TBL_CORE_PANIC_H */
