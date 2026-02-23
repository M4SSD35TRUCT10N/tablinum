#ifndef TBL_CORE_SAFE_H
#define TBL_CORE_SAFE_H

#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>

/* OpenBSD-like helpers (C89). Return length of src / length tried. */
size_t tbl_strlcpy(char *dst, const char *src, size_t dstsz);
size_t tbl_strlcat(char *dst, const char *src, size_t dstsz);

/* Checked arithmetic for size_t (avoid overflow). Return 1 on success. */
int tbl_size_add(size_t a, size_t b, size_t *out);
int tbl_size_mul(size_t a, size_t b, size_t *out);

/* Parse unsigned decimal into u32-range (0..0xFFFFFFFF). Return 1 on success. */
int tbl_parse_u32(const char *s, unsigned long *out);

int tbl_u32_to_dec(unsigned long v, char *buf, size_t bufsz);

/* Unsigned long to decimal string (C89). Return 1 on success. */
int tbl_ul_to_dec(unsigned long v, char *buf, size_t bufsz);

/* Non-truncating variants (return 1 if full copy/concat fit). */
int tbl_strlcpy_ok(char *dst, const char *src, size_t dstsz);
int tbl_strlcat_ok(char *dst, const char *src, size_t dstsz);

/* Explicit _ok aliases for boolean-return helpers (avoid rc/ok confusion). */
int tbl_size_add_ok(size_t a, size_t b, size_t *out);
int tbl_size_mul_ok(size_t a, size_t b, size_t *out);
int tbl_parse_u32_ok(const char *s, unsigned long *out);
int tbl_u32_to_dec_ok(unsigned long v, char *buf, size_t bufsz);
int tbl_ul_to_dec_ok(unsigned long v, char *buf, size_t bufsz);

/* Safe stdio helpers (no direct sprintf/fprintf in core). Return 1 on success. */
int tbl_fwrite_all_ok(FILE *fp, const void *buf, size_t len);
int tbl_fputs_ok(FILE *fp, const char *s);
int tbl_vfprintf_ok(FILE *fp, const char *fmt, va_list ap);
int tbl_fflush_ok(FILE *fp);
int tbl_fputc_ok(FILE *fp, int ch);
int tbl_fputs2_ok(FILE *fp, const char *a, const char *b);
int tbl_fputs3_ok(FILE *fp, const char *a, const char *b, const char *c);
int tbl_fputs4_ok(FILE *fp, const char *a, const char *b, const char *c, const char *d);
int tbl_fputs5_ok(FILE *fp, const char *a, const char *b, const char *c, const char *d, const char *e);

#if defined(TBL_STRICT_NAMES) && !defined(TBL_SAFE_IMPLEMENTATION)
/* Forbid ambiguous boolean helpers without _ok suffix. */
#define tbl_size_add    TBL_FORBIDDEN_use_tbl_size_add_ok
#define tbl_size_mul    TBL_FORBIDDEN_use_tbl_size_mul_ok
#define tbl_parse_u32   TBL_FORBIDDEN_use_tbl_parse_u32_ok
#define tbl_u32_to_dec  TBL_FORBIDDEN_use_tbl_u32_to_dec_ok
#define tbl_ul_to_dec   TBL_FORBIDDEN_use_tbl_ul_to_dec_ok


#if defined(TBL_STRICT_NAMES)
/* Forbid ambiguous boolean helpers without _ok suffix in project code. */
#define tbl_size_add    TBL_FORBIDDEN_use_tbl_size_add_ok
#define tbl_size_mul    TBL_FORBIDDEN_use_tbl_size_mul_ok
#define tbl_parse_u32   TBL_FORBIDDEN_use_tbl_parse_u32_ok
#define tbl_u32_to_dec  TBL_FORBIDDEN_use_tbl_u32_to_dec_ok
#define tbl_ul_to_dec   TBL_FORBIDDEN_use_tbl_ul_to_dec_ok
#define tbl_sha256_hex  TBL_FORBIDDEN_use_tbl_sha256_hex_ok
#endif

#if defined(TBL_FORBID_STDIO_FORMAT) && !defined(TBL_SAFE_IMPLEMENTATION)
/* Forbid stdio formatting functions in project code. Use core/safe.h helpers. */
#define printf   TBL_FORBIDDEN_printf_use_tbl_logf_or_safe_helpers
#define fprintf  TBL_FORBIDDEN_fprintf_use_tbl_fputsN_ok
#define sprintf  TBL_FORBIDDEN_sprintf_use_safe_helpers
#define vfprintf TBL_FORBIDDEN_vfprintf_use_tbl_vfprintf_ok
#define vsprintf TBL_FORBIDDEN_vsprintf_use_safe_helpers
#endif

#endif


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

/* ---- non-truncating wrappers ---- */
int tbl_strlcpy_ok(char *dst, const char *src, size_t dstsz)
{
    return (tbl_strlcpy(dst, src, dstsz) < dstsz) ? 1 : 0;
}

int tbl_strlcat_ok(char *dst, const char *src, size_t dstsz)
{
    return (tbl_strlcat(dst, src, dstsz) < dstsz) ? 1 : 0;
}

/* ---- unsigned long to decimal ---- */
int tbl_ul_to_dec(unsigned long v, char *buf, size_t bufsz)
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

/* ---- explicit _ok aliases ---- */
int tbl_size_add_ok(size_t a, size_t b, size_t *out) { return tbl_size_add(a, b, out); }
int tbl_size_mul_ok(size_t a, size_t b, size_t *out) { return tbl_size_mul(a, b, out); }
int tbl_parse_u32_ok(const char *s, unsigned long *out) { return tbl_parse_u32(s, out); }
int tbl_u32_to_dec_ok(unsigned long v, char *buf, size_t bufsz) { return tbl_u32_to_dec(v, buf, bufsz); }
int tbl_ul_to_dec_ok(unsigned long v, char *buf, size_t bufsz) { return tbl_ul_to_dec(v, buf, bufsz); }

/* ---- safe stdio helpers ---- */
int tbl_fwrite_all_ok(FILE *fp, const void *buf, size_t len)
{
    size_t n;
    if (!fp) return 0;
    if (!buf && len != 0) return 0;
    if (len == 0) return 1;
    n = fwrite(buf, 1, len, fp);
    return (n == len) ? 1 : 0;
}

int tbl_vfprintf_ok(FILE *fp, const char *fmt, va_list ap)
{
    int rc;
    if (!fp || !fmt) return 0;
    rc = vfprintf(fp, fmt, ap);
    return (rc < 0) ? 0 : 1;
}

int tbl_fflush_ok(FILE *fp)
{
    if (!fp) return 0;
    return (fflush(fp) == 0) ? 1 : 0;
}

int tbl_fputs_ok(FILE *fp, const char *s)
{
    if (!s) s = "";
    return tbl_fwrite_all_ok(fp, s, strlen(s));
}

int tbl_fputc_ok(FILE *fp, int ch)
{
    if (!fp) return 0;
    return (fputc(ch, fp) == EOF) ? 0 : 1;
}

int tbl_fputs2_ok(FILE *fp, const char *a, const char *b)
{
    return tbl_fputs_ok(fp, a) && tbl_fputs_ok(fp, b);
}

int tbl_fputs3_ok(FILE *fp, const char *a, const char *b, const char *c)
{
    return tbl_fputs_ok(fp, a) && tbl_fputs_ok(fp, b) && tbl_fputs_ok(fp, c);
}

int tbl_fputs4_ok(FILE *fp, const char *a, const char *b, const char *c, const char *d)
{
    return tbl_fputs_ok(fp, a) && tbl_fputs_ok(fp, b) && tbl_fputs_ok(fp, c) && tbl_fputs_ok(fp, d);
}

int tbl_fputs5_ok(FILE *fp, const char *a, const char *b, const char *c, const char *d, const char *e)
{
    return tbl_fputs_ok(fp, a) && tbl_fputs_ok(fp, b) && tbl_fputs_ok(fp, c) && tbl_fputs_ok(fp, d) && tbl_fputs_ok(fp, e);
}

#endif /* TBL_SAFE_IMPLEMENTATION */

#endif /* TBL_CORE_SAFE_H */
