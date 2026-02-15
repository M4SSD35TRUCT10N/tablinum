#ifndef TBL_CORE_SAFE_H
#define TBL_CORE_SAFE_H

#include <stddef.h>

/* OpenBSD-like helpers (C89). Return length of src / length tried. */
size_t tbl_strlcpy(char *dst, const char *src, size_t dstsz);
size_t tbl_strlcat(char *dst, const char *src, size_t dstsz);

/* Checked arithmetic for size_t (avoid overflow). Return 1 on success. */
int tbl_size_add(size_t a, size_t b, size_t *out);
int tbl_size_mul(size_t a, size_t b, size_t *out);

/* Parse unsigned decimal into u32-range (0..0xFFFFFFFF). Return 1 on success. */
int tbl_parse_u32(const char *s, unsigned long *out);

int tbl_u32_to_dec(unsigned long v, char *buf, size_t bufsz);

#ifdef TBL_SAFE_IMPLEMENTATION

#include <string.h>

size_t tbl_strlcpy(char *dst, const char *src, size_t dstsz)
{
    size_t srclen;
    size_t n;

    if (!src) src = "";
    srclen = strlen(src);

    if (!dst || dstsz == 0) {
        return srclen;
    }

    n = (srclen >= dstsz) ? (dstsz - 1) : srclen;
    if (n > 0) {
        memcpy(dst, src, n);
    }
    dst[n] = '\0';

    return srclen;
}

size_t tbl_strlcat(char *dst, const char *src, size_t dstsz)
{
    size_t dlen;
    size_t slen;

    if (!dst || dstsz == 0) {
        return 0;
    }
    if (!src) src = "";

    dlen = strlen(dst);
    slen = strlen(src);

    if (dlen >= dstsz) {
        return dstsz + slen;
    }

    return dlen + tbl_strlcpy(dst + dlen, src, dstsz - dlen);
}

int tbl_size_add(size_t a, size_t b, size_t *out)
{
    size_t maxv;

    if (!out) return 0;
    maxv = (size_t)-1;

    if (b > (maxv - a)) return 0;
    *out = a + b;
    return 1;
}

int tbl_size_mul(size_t a, size_t b, size_t *out)
{
    size_t maxv;

    if (!out) return 0;
    maxv = (size_t)-1;

    if (a != 0 && b > (maxv / a)) return 0;
    *out = a * b;
    return 1;
}

int tbl_parse_u32(const char *s, unsigned long *out)
{
    unsigned long v;
    unsigned long d;
    int saw_digit;
    const unsigned long maxv = 0xFFFFFFFFUL;

    if (!s || !out) return 0;

    v = 0;
    saw_digit = 0;

    /* skip leading whitespace */
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') {
        s++;
    }

    while (*s >= '0' && *s <= '9') {
        d = (unsigned long)(*s - '0');
        saw_digit = 1;

        /* v = v*10 + d with overflow + range check */
        if (v > (maxv / 10UL)) return 0;
        v = v * 10UL;
        if (v > (maxv - d)) return 0;
        v = v + d;

        s++;
    }

    /* allow trailing whitespace only */
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') {
        s++;
    }
    if (*s != '\0') return 0;
    if (!saw_digit) return 0;

    *out = v;
    return 1;
}

int tbl_u32_to_dec(unsigned long v, char *buf, size_t bufsz)
{
    char tmp[16];
    size_t n;
    size_t i;

    if (!buf || bufsz == 0) return 0;

    /* u32 max = 10 digits, tmp is plenty */
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

    /* reverse */
    for (i = 0; i < n; ++i) {
        buf[i] = tmp[n - 1 - i];
    }
    buf[n] = '\0';
    return 1;
}

#endif /* TBL_SAFE_IMPLEMENTATION */

#endif /* TBL_CORE_SAFE_H */
