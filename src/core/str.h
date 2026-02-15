#ifndef TBL_CORE_STR_H
#define TBL_CORE_STR_H

#include <stddef.h>

/* NULL-safe string helpers (C89).
   - NULL is treated like "" where it makes sense.
   - Byte-wise only (no locale/UTF magic). */

size_t tbl_strlen(const char *s);

int tbl_streq(const char *a, const char *b);
int tbl_strneq(const char *a, const char *b, size_t n);

int tbl_str_starts_with(const char *s, const char *prefix);
int tbl_str_ends_with(const char *s, const char *suffix);

#ifdef TBL_STR_IMPLEMENTATION

#include <string.h>

size_t tbl_strlen(const char *s)
{
    if (!s) return 0;
    return strlen(s);
}

int tbl_streq(const char *a, const char *b)
{
    if (!a) a = "";
    if (!b) b = "";
    return (strcmp(a, b) == 0) ? 1 : 0;
}

int tbl_strneq(const char *a, const char *b, size_t n)
{
    if (!a) a = "";
    if (!b) b = "";
    return (strncmp(a, b, n) == 0) ? 1 : 0;
}

int tbl_str_starts_with(const char *s, const char *prefix)
{
    size_t ps;

    if (!s) s = "";
    if (!prefix) prefix = "";

    ps = tbl_strlen(prefix);
    return tbl_strneq(s, prefix, ps);
}

int tbl_str_ends_with(const char *s, const char *suffix)
{
    size_t sl;
    size_t su;

    if (!s) s = "";
    if (!suffix) suffix = "";

    sl = tbl_strlen(s);
    su = tbl_strlen(suffix);

    if (su == 0) return 1; /* "" matches */
    if (su > sl) return 0;

    return (strcmp(s + (sl - su), suffix) == 0) ? 1 : 0;
}

#endif /* TBL_STR_IMPLEMENTATION */

#endif /* TBL_CORE_STR_H */

