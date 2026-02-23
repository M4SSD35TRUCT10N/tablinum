#ifndef TBL_TEST_H
#define TBL_TEST_H

#include <stdio.h>
#include <string.h>

/* Provide core/safe.h implementation for standalone test executables. */
#define TBL_SAFE_IMPLEMENTATION
#include "core/safe.h"

/* Define T_TESTNAME before including this header, otherwise it falls back to __FILE__. */
#ifndef T_TESTNAME
#define T_TESTNAME __FILE__
#endif

/* Test output must not rely on fprintf/sprintf. Keep it simple and C89. */

static void t_puts(FILE *fp, const char *s)
{
    if (!fp) return;
    if (!s) s = "";
    (void)fputs(s, fp);
}

static void t_putc(FILE *fp, int ch)
{
    if (!fp) return;
    (void)fputc(ch, fp);
}

static int t_ul_to_dec(unsigned long v, char *buf, size_t bufsz)
{
    char tmp[32];
    size_t n;
    size_t i;

    if (!buf || bufsz == 0) return 0;

    n = 0;

    if (v == 0UL) {
        if (bufsz < 2) return 0;
        buf[0] = '0';
        buf[1] = '\0';
        return 1;
    }

    while (v != 0UL) {
        unsigned long d = v % 10UL;
        if (n >= sizeof(tmp)) return 0;
        tmp[n++] = (char)('0' + (char)d);
        v /= 10UL;
    }

    if (n + 1 > bufsz) return 0;

    for (i = 0; i < n; ++i) {
        buf[i] = tmp[n - 1 - i];
    }
    buf[n] = '\0';
    return 1;
}

static void t_put_ul(FILE *fp, unsigned long v)
{
    char buf[32];
    if (!t_ul_to_dec(v, buf, sizeof(buf))) {
        t_puts(fp, "?");
        return;
    }
    t_puts(fp, buf);
}

static void t_put_int(FILE *fp, int v)
{
    if (v < 0) {
        t_putc(fp, '-');
        /* avoid UB on INT_MIN: cast to unsigned long in a conservative way */
        t_put_ul(fp, (unsigned long)(-(v + 1)) + 1UL);
    } else {
        t_put_ul(fp, (unsigned long)v);
    }
}

/* --- output helpers --- */

#define T_TRACE(MSG) do { \
    t_puts(stderr, T_TESTNAME); t_puts(stderr, ": "); t_puts(stderr, (MSG)); t_putc(stderr, '\n'); \
} while (0)

#define T_FAIL(MSG) do { \
    t_puts(stderr, T_TESTNAME); t_puts(stderr, ": "); t_puts(stderr, (MSG)); t_putc(stderr, '\n'); \
    return 1; \
} while (0)

/* --- assertions --- */

#define T_ASSERT(COND) do { \
    if (!(COND)) { \
        t_puts(stderr, T_TESTNAME); t_puts(stderr, ": assert failed: "); t_puts(stderr, #COND); \
        t_puts(stderr, " (line "); t_put_int(stderr, __LINE__); t_puts(stderr, ")"); t_putc(stderr, '\n'); \
        return 1; \
    } \
} while (0)

#define T_ASSERT_EQ_INT(A,B) do { \
    int _a = (A); \
    int _b = (B); \
    if (_a != _b) { \
        t_puts(stderr, T_TESTNAME); t_puts(stderr, ": assert failed: "); t_puts(stderr, #A); t_puts(stderr, " == "); t_puts(stderr, #B); \
        t_puts(stderr, " (got "); t_put_int(stderr, _a); t_puts(stderr, ", expected "); t_put_int(stderr, _b); t_puts(stderr, ") (line "); t_put_int(stderr, __LINE__); t_puts(stderr, ")"); t_putc(stderr, '\n'); \
        return 1; \
    } \
} while (0)

#define T_ASSERT_EQ_ULONG(A,B) do { \
    unsigned long _a = (unsigned long)(A); \
    unsigned long _b = (unsigned long)(B); \
    if (_a != _b) { \
        t_puts(stderr, T_TESTNAME); t_puts(stderr, ": assert failed: "); t_puts(stderr, #A); t_puts(stderr, " == "); t_puts(stderr, #B); \
        t_puts(stderr, " (got "); t_put_ul(stderr, _a); t_puts(stderr, ", expected "); t_put_ul(stderr, _b); t_puts(stderr, ") (line "); t_put_int(stderr, __LINE__); t_puts(stderr, ")"); t_putc(stderr, '\n'); \
        return 1; \
    } \
} while (0)

#define T_ASSERT_STREQ(A,B) do { \
    const char *_a = (A); \
    const char *_b = (B); \
    if (!_a) _a = "(null)"; \
    if (!_b) _b = "(null)"; \
    if (strcmp(_a, _b) != 0) { \
        t_puts(stderr, T_TESTNAME); t_puts(stderr, ": assert failed: \""); t_puts(stderr, _a); t_puts(stderr, "\" == \""); t_puts(stderr, _b); t_puts(stderr, "\" (line "); t_put_int(stderr, __LINE__); t_puts(stderr, ")"); t_putc(stderr, '\n'); \
        return 1; \
    } \
} while (0)

/* Use at end of main() */
#define T_OK() do { \
    t_puts(stdout, "OK "); t_puts(stdout, T_TESTNAME); t_putc(stdout, '\n'); \
    return 0; \
} while (0)

#endif /* TBL_TEST_H */
