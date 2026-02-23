#ifndef TBL_CORE_AUDIT_H
#define TBL_CORE_AUDIT_H

#include <stddef.h>

/* Needed for tbl_strlcpy() used in the implementation block. Keeping this
 * prototype visible here avoids implicit-declaration warnings on toolchains
 * that are picky about include ordering.
 */
#include "core/safe.h"

/* Verify the ops audit hash-chain at <repo_root>/audit/ops.log.
 * Return Tablinum exit codes:
 *   0 ok
 *   3 notfound
 *   4 io
 *   5 integrity (format or hash-chain broken)
 */
int tbl_audit_verify_ops(const char *repo_root, char *err, size_t errsz);

#ifdef TBL_AUDIT_IMPLEMENTATION

#include <stdio.h>
#include <string.h>

#include "core/path.h"
#include "core/safe.h"
#include "core/sha256.h"
#include "core/str.h"
#include "os/fs.h"

/* Keep exit codes local to avoid include cycles. */
enum {
    TBLX_EXIT_OK = 0,
    TBLX_EXIT_NOTFOUND = 3,
    TBLX_EXIT_IO = 4,
    TBLX_EXIT_INTEGRITY = 5
};

static void tbl_audit_seterr(char *err, size_t errsz, const char *msg)
{
    if (!err || errsz == 0) return;
    err[0] = '\0';
    if (!msg) msg = "audit error";
    (void)tbl_strlcpy(err, msg, errsz);
}

static void tbl_audit_seterr_line(char *err, size_t errsz, unsigned long line_no, const char *tail)
{
    char nbuf[32];
    char msg[256];

    if (!tail) tail = "error";
    if (!tbl_ul_to_dec_ok(line_no, nbuf, sizeof(nbuf))) {
        tbl_audit_seterr(err, errsz, "audit integrity: line ?: format error");
        return;
    }

    msg[0] = '\0';
    (void)tbl_strlcpy(msg, "audit integrity: line ", sizeof(msg));
    (void)tbl_strlcat(msg, nbuf, sizeof(msg));
    (void)tbl_strlcat(msg, ": ", sizeof(msg));
    (void)tbl_strlcat(msg, tail, sizeof(msg));

    tbl_audit_seterr(err, errsz, msg);
}

static void tbl_audit_zero64(char out64[65])
{
    int i;
    for (i = 0; i < 64; ++i) out64[i] = '0';
    out64[64] = '\0';
}

static int tbl_audit_is_hex64(const char *s)
{
    int i;
    if (!s) return 0;
    for (i = 0; i < 64; ++i) {
        char c = s[i];
        if (!c) return 0;
        if (!((c >= '0' && c <= '9') ||
              (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F'))) return 0;
    }
    return s[64] == '\0';
}

static void tbl_audit_hash_chain(const char prev64[65], const char *canonical, char out64[65])
{
    tbl_sha256_t st;
    unsigned char dig[32];
    tbl_sha256_init(&st);
    tbl_sha256_update(&st, prev64, 64);
    tbl_sha256_update(&st, "\n", 1);
    tbl_sha256_update(&st, canonical, strlen(canonical));
    tbl_sha256_final(&st, dig);
    (void)tbl_sha256_hex_ok(dig, out64, 65);
}

static int tbl_audit_parse_line(const char *line,
                               char prev_out[65],
                               char hash_out[65],
                               const char **canonical_out)
{
    const char *p;
    int i;

    if (!line || !prev_out || !hash_out || !canonical_out) return 0;

    if (!tbl_str_starts_with(line, "prev=")) return 0;
    p = line + 5;
    for (i = 0; i < 64; ++i) {
        if (!p[i]) return 0;
        prev_out[i] = p[i];
    }
    prev_out[64] = '\0';
    p += 64;
    if (*p != ' ') return 0;
    p++;

    if (!tbl_str_starts_with(p, "hash=")) return 0;
    p += 5;
    for (i = 0; i < 64; ++i) {
        if (!p[i]) return 0;
        hash_out[i] = p[i];
    }
    hash_out[64] = '\0';
    p += 64;
    if (*p != ' ') return 0;
    p++;

    if (!p[0]) return 0;
    *canonical_out = p;
    return 1;
}

int tbl_audit_verify_ops(const char *repo_root, char *err, size_t errsz)
{
    char audit_dir[1024];
    char audit_path[1024];
    int ex;
    FILE *fp;
    char buf[4096];
    unsigned long line_no;
    char expected_prev[65];

    if (err && errsz) err[0] = '\0';
    if (!repo_root || !repo_root[0]) {
        tbl_audit_seterr(err, errsz, "invalid args");
        return TBLX_EXIT_IO;
    }

    if (!tbl_path_join2(audit_dir, sizeof(audit_dir), repo_root, "audit") ||
        !tbl_path_join2(audit_path, sizeof(audit_path), audit_dir, "ops.log")) {
        tbl_audit_seterr(err, errsz, "audit path too long");
        return TBLX_EXIT_IO;
    }

    ex = 0;
    (void)tbl_fs_exists(audit_path, &ex);
    if (!ex) {
        tbl_audit_seterr(err, errsz, "audit log not found");
        return TBLX_EXIT_NOTFOUND;
    }

    fp = fopen(audit_path, "rb");
    if (!fp) {
        tbl_audit_seterr(err, errsz, "cannot open audit log");
        return TBLX_EXIT_IO;
    }

    tbl_audit_zero64(expected_prev);
    line_no = 0UL;

    while (fgets(buf, (int)sizeof(buf), fp) != NULL) {
        size_t len;
        char prev[65];
        char hash[65];
        const char *canonical;
        char calc[65];

        line_no++;

        /* reject CR anywhere (LF-only). */
        if (strchr(buf, '\r') != NULL) {
            fclose(fp);
            tbl_audit_seterr_line(err, errsz, line_no, "CR found");
            return TBLX_EXIT_INTEGRITY;
        }

        len = strlen(buf);
        if (len == 0) continue;
        if (buf[len - 1] != '\n') {
            fclose(fp);
            tbl_audit_seterr_line(err, errsz, line_no, "missing LF or line too long");
            return TBLX_EXIT_INTEGRITY;
        }
        buf[len - 1] = '\0';

        if (buf[0] == '\0') {
            fclose(fp);
            tbl_audit_seterr_line(err, errsz, line_no, "empty line");
            return TBLX_EXIT_INTEGRITY;
        }

        canonical = NULL;
        if (!tbl_audit_parse_line(buf, prev, hash, &canonical)) {
            fclose(fp);
            tbl_audit_seterr_line(err, errsz, line_no, "invalid format");
            return TBLX_EXIT_INTEGRITY;
        }

        if (!tbl_audit_is_hex64(prev) || !tbl_audit_is_hex64(hash)) {
            fclose(fp);
            tbl_audit_seterr_line(err, errsz, line_no, "invalid hex");
            return TBLX_EXIT_INTEGRITY;
        }

        if (!tbl_streq(prev, expected_prev)) {
            fclose(fp);
            tbl_audit_seterr_line(err, errsz, line_no, "prev mismatch");
            return TBLX_EXIT_INTEGRITY;
        }

        /* Optional sanity check: canonical begins with ts= and contains event= */
        if (!tbl_str_starts_with(canonical, "ts=") || strstr(canonical, " event=") == NULL) {
            fclose(fp);
            tbl_audit_seterr_line(err, errsz, line_no, "canonical malformed");
            return TBLX_EXIT_INTEGRITY;
        }

        tbl_audit_hash_chain(prev, canonical, calc);
        if (!tbl_streq(calc, hash)) {
            fclose(fp);
            tbl_audit_seterr_line(err, errsz, line_no, "hash mismatch");
            return TBLX_EXIT_INTEGRITY;
        }

        (void)tbl_strlcpy(expected_prev, hash, sizeof(expected_prev));
    }

    if (ferror(fp)) {
        fclose(fp);
        tbl_audit_seterr(err, errsz, "read error");
        return TBLX_EXIT_IO;
    }

    fclose(fp);
    return TBLX_EXIT_OK;
}

#endif /* TBL_AUDIT_IMPLEMENTATION */

#endif /* TBL_CORE_AUDIT_H */
