#ifndef TBL_CORE_INI_H
#define TBL_CORE_INI_H

#include <stddef.h>

typedef int (*tbl_ini_kv_cb)(void *ud,
                            const char *section,
                            const char *key,
                            const char *value,
                            int line_no);

enum {
    TBL_INI_OK = 0,
    TBL_INI_EIO = 1,
    TBL_INI_EPARSE = 2,
    TBL_INI_ECALLBACK = 3
};

/* Parse from file path */
int tbl_ini_parse_file(const char *path,
                       tbl_ini_kv_cb cb,
                       void *ud,
                       char *err,
                       size_t errsz);

/* Parse from memory (used by tests, also useful later) */
int tbl_ini_parse_buf(const char *buf,
                      size_t len,
                      tbl_ini_kv_cb cb,
                      void *ud,
                      char *err,
                      size_t errsz);

#ifdef TBL_INI_IMPLEMENTATION

#include <stdio.h>
#include <string.h>

#include "core/safe.h"

#ifndef TBL_INI_MAX_LINE
#define TBL_INI_MAX_LINE 4096
#endif

#ifndef TBL_INI_MAX_SECTION
#define TBL_INI_MAX_SECTION 256
#endif

static int tbl_ini_is_space(int c)
{
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v');
}

static char *tbl_ini_ltrim(char *s)
{
    if (!s) return s;
    while (*s && tbl_ini_is_space((int)(unsigned char)*s)) {
        s++;
    }
    return s;
}

static void tbl_ini_rtrim(char *s)
{
    size_t n;

    if (!s) return;

    n = strlen(s);
    while (n > 0 && tbl_ini_is_space((int)(unsigned char)s[n - 1])) {
        s[n - 1] = '\0';
        n--;
    }
}

static int tbl_ini_is_comment_line(const char *s)
{
    if (!s) return 1;
    if (*s == '\0') return 1;
    if (*s == ';' || *s == '#') return 1;
    return 0;
}

static void tbl_ini_seterr(char *err, size_t errsz, const char *msg, int line_no)
{
    char num[16];

    if (!err || errsz == 0) return;
    err[0] = '\0';

    if (!msg) msg = "ini error";

    (void)tbl_u32_to_dec_ok((unsigned long)line_no, num, sizeof(num));

    (void)tbl_strlcpy(err, msg, errsz);
    (void)tbl_strlcat(err, " (line ", errsz);
    (void)tbl_strlcat(err, num, errsz);
    (void)tbl_strlcat(err, ")", errsz);
}

static int tbl_ini_parse_line(char *line,
                              char *cur_section,
                              size_t cur_sectionsz,
                              tbl_ini_kv_cb cb,
                              void *ud,
                              char *err,
                              size_t errsz,
                              int line_no)
{
    char *p;
    char *eq;
    char *key;
    char *val;
    char *rb;

    if (!line || !cur_section || cur_sectionsz == 0) {
        tbl_ini_seterr(err, errsz, "internal ini parser error", line_no);
        return TBL_INI_EPARSE;
    }

    /* strip newline(s) */
    rb = line;
    while (*rb) {
        if (*rb == '\n' || *rb == '\r') {
            *rb = '\0';
            break;
        }
        rb++;
    }

    p = tbl_ini_ltrim(line);
    tbl_ini_rtrim(p);

    /* allow UTF-8 BOM on first non-empty line */
    if (line_no == 1 && (unsigned char)p[0] == 0xEF &&
        (unsigned char)p[1] == 0xBB &&
        (unsigned char)p[2] == 0xBF) {
        p += 3;
        p = tbl_ini_ltrim(p);
        tbl_ini_rtrim(p);
    }

    if (tbl_ini_is_comment_line(p)) {
        return TBL_INI_OK;
    }

    /* section: [....] (we accept anything until ']') */
    if (*p == '[') {
        char *lb = p + 1;
        char *end = strchr(lb, ']');
        char *after;

        if (!end) {
            tbl_ini_seterr(err, errsz, "missing closing ']'", line_no);
            return TBL_INI_EPARSE;
        }

        *end = '\0';
        lb = tbl_ini_ltrim(lb);
        tbl_ini_rtrim(lb);

        if (!lb[0]) {
            tbl_ini_seterr(err, errsz, "empty section name", line_no);
            return TBL_INI_EPARSE;
        }

        /* ensure only whitespace/comment after ']' */
        after = end + 1;
        after = tbl_ini_ltrim(after);
        if (after[0] != '\0' && after[0] != ';' && after[0] != '#') {
            tbl_ini_seterr(err, errsz, "trailing junk after section", line_no);
            return TBL_INI_EPARSE;
        }

        if (tbl_strlcpy(cur_section, lb, cur_sectionsz) >= cur_sectionsz) {
            tbl_ini_seterr(err, errsz, "section name too long", line_no);
            return TBL_INI_EPARSE;
        }

        return TBL_INI_OK;
    }

    /* key=value */
    eq = strchr(p, '=');
    if (!eq) {
        tbl_ini_seterr(err, errsz, "missing '='", line_no);
        return TBL_INI_EPARSE;
    }

    *eq = '\0';
    key = tbl_ini_ltrim(p);
    tbl_ini_rtrim(key);

    val = tbl_ini_ltrim(eq + 1);
    tbl_ini_rtrim(val);

    if (!key[0]) {
        tbl_ini_seterr(err, errsz, "empty key", line_no);
        return TBL_INI_EPARSE;
    }

    if (!cb) {
        tbl_ini_seterr(err, errsz, "missing callback", line_no);
        return TBL_INI_EPARSE;
    }

    if (cb(ud, cur_section, key, val, line_no) != 0) {
        if (err && errsz && err[0] == '\0') {
            tbl_ini_seterr(err, errsz, "callback rejected entry", line_no);
        }
        return TBL_INI_ECALLBACK;
    }

    return TBL_INI_OK;
}

int tbl_ini_parse_file(const char *path,
                       tbl_ini_kv_cb cb,
                       void *ud,
                       char *err,
                       size_t errsz)
{
    FILE *fp;
    char line[TBL_INI_MAX_LINE];
    char section[TBL_INI_MAX_SECTION];
    int line_no;
    int rc;

    if (err && errsz) err[0] = '\0';
    if (!path || !path[0]) {
        tbl_ini_seterr(err, errsz, "missing ini path", 0);
        return TBL_INI_EIO;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        tbl_ini_seterr(err, errsz, "cannot open ini file", 0);
        return TBL_INI_EIO;
    }

    section[0] = '\0';
    line_no = 0;
    rc = TBL_INI_OK;

    while (fgets(line, (int)sizeof(line), fp) != NULL) {
        size_t n;
        int has_nl;

        line_no++;
        n = strlen(line);
        has_nl = (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) ? 1 : 0;

        /* If no newline and not EOF -> line too long. */
        if (!has_nl) {
            int c = fgetc(fp);
            if (c != EOF) {
                /* consume rest of line */
                while (c != EOF && c != '\n') {
                    c = fgetc(fp);
                }
                tbl_ini_seterr(err, errsz, "line too long", line_no);
                rc = TBL_INI_EPARSE;
                break;
            }
        }

        rc = tbl_ini_parse_line(line, section, sizeof(section), cb, ud, err, errsz, line_no);
        if (rc != TBL_INI_OK) {
            break;
        }
    }

    fclose(fp);
    return rc;
}

int tbl_ini_parse_buf(const char *buf,
                      size_t len,
                      tbl_ini_kv_cb cb,
                      void *ud,
                      char *err,
                      size_t errsz)
{
    char line[TBL_INI_MAX_LINE];
    char section[TBL_INI_MAX_SECTION];
    size_t i;
    size_t start;
    int line_no;
    int rc;

    if (err && errsz) err[0] = '\0';
    if (!buf && len != 0) {
        tbl_ini_seterr(err, errsz, "missing ini buffer", 0);
        return TBL_INI_EPARSE;
    }

    section[0] = '\0';
    i = 0;
    start = 0;
    line_no = 0;
    rc = TBL_INI_OK;

    while (i <= len) {
        int at_end;
        int is_nl;
        size_t linelen;

        at_end = (i == len) ? 1 : 0;
        is_nl = (!at_end && (buf[i] == '\n' || buf[i] == '\r')) ? 1 : 0;

        if (at_end || is_nl) {
            linelen = i - start;

            if (linelen >= sizeof(line)) {
                tbl_ini_seterr(err, errsz, "line too long", line_no + 1);
                return TBL_INI_EPARSE;
            }

            /* copy line slice */
            if (linelen > 0) {
                memcpy(line, buf + start, linelen);
            }
            line[linelen] = '\0';

            line_no++;
            rc = tbl_ini_parse_line(line, section, sizeof(section), cb, ud, err, errsz, line_no);
            if (rc != TBL_INI_OK) {
                return rc;
            }

            /* consume \r\n */
            if (!at_end && buf[i] == '\r' && (i + 1) < len && buf[i + 1] == '\n') {
                i++;
            }

            start = i + 1;
        }

        i++;
    }

    return TBL_INI_OK;
}

#endif /* TBL_INI_IMPLEMENTATION */

#endif /* TBL_CORE_INI_H */
