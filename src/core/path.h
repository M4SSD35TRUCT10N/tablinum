#ifndef TBL_CORE_PATH_H
#define TBL_CORE_PATH_H

#include <stddef.h>

/* platform separator: '\\' on Windows, '/' otherwise */
char tbl_path_sep(void);

/* returns 1 if absolute-ish path, 0 otherwise */
int tbl_path_is_abs(const char *p);

/* Join a + b into dst (overflow-safe). Returns 1 on success, 0 on error. */
int tbl_path_join2(char *dst, size_t dstsz, const char *a, const char *b);

/* Normalize separators in-place (convert '/' and '\\' to platform sep, collapse repeats). */
void tbl_path_norm_seps(char *s);

#ifdef TBL_PATH_IMPLEMENTATION

#include <string.h>
#include "core/safe.h"

static int tbl_path_is_sep_char(int c)
{
    return (c == '/' || c == '\\');
}

char tbl_path_sep(void)
{
#ifdef _WIN32
    return '\\';
#else
    return '/';
#endif
}

int tbl_path_is_abs(const char *p)
{
    if (!p || !p[0]) return 0;

#ifdef _WIN32
    /* "C:\..." or "\..." or "/..." */
    if (tbl_path_is_sep_char((int)(unsigned char)p[0])) return 1;
    if (((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) &&
        p[1] == ':' &&
        (p[2] == '\0' || tbl_path_is_sep_char((int)(unsigned char)p[2]))) {
        return 1;
    }
    return 0;
#else
    /* POSIX/Plan9: "/..." */
    if (p[0] == '/') return 1;
    return 0;
#endif
}

void tbl_path_norm_seps(char *s)
{
    size_t r;
    size_t w;
    char sep;
    int prev_was_sep;

    if (!s) return;

    sep = tbl_path_sep();
    r = 0;
    w = 0;
    prev_was_sep = 0;

    while (s[r] != '\0') {
        char c = s[r];

        if (tbl_path_is_sep_char((int)(unsigned char)c)) {
            if (!prev_was_sep) {
                s[w++] = sep;
                prev_was_sep = 1;
            }
        } else {
            s[w++] = c;
            prev_was_sep = 0;
        }
        r++;
    }

    s[w] = '\0';
}

int tbl_path_join2(char *dst, size_t dstsz, const char *a, const char *b)
{
    size_t alen;
    size_t blen;
    char sep;
    int need_sep;
    const char *bp;

    if (!dst || dstsz == 0) return 0;
    sep = tbl_path_sep();

    if (!a) a = "";
    if (!b) b = "";

    alen = strlen(a);
    blen = strlen(b);

    /* start with a */
    if (tbl_strlcpy(dst, a, dstsz) >= dstsz) return 0;

    /* if b is empty, just normalize and done */
    if (blen == 0) {
        tbl_path_norm_seps(dst);
        return 1;
    }

    /* determine if we need a separator */
    need_sep = 0;
    if (alen > 0) {
        char last = a[alen - 1];
        if (!tbl_path_is_sep_char((int)(unsigned char)last)) {
            need_sep = 1;
        }
    }

    /* skip leading seps in b if a is non-empty (so join stays sane) */
    bp = b;
    if (alen > 0) {
        while (*bp && tbl_path_is_sep_char((int)(unsigned char)*bp)) {
            bp++;
        }
    }

    if (need_sep) {
        char tmp[2];
        tmp[0] = sep;
        tmp[1] = '\0';
        if (tbl_strlcat(dst, tmp, dstsz) >= dstsz) return 0;
    }

    if (tbl_strlcat(dst, bp, dstsz) >= dstsz) return 0;

    tbl_path_norm_seps(dst);
    return 1;
}

#endif /* TBL_PATH_IMPLEMENTATION */

#endif /* TBL_CORE_PATH_H */
