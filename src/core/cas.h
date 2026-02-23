#ifndef TBL_CORE_CAS_H
#define TBL_CORE_CAS_H

#include <stddef.h>

/* Content-addressed store (SHA-256) on top of repo root.
   Layout: <repo>/sha256/ab/cdef... (first 2 hex chars are directory). */

int tbl_cas_put_file(const char *repo_root, const char *src_path,
                     char *out_sha256hex, size_t out_sha256hex_sz,
                     char *err, size_t errsz);

/* Compute the object path for a given sha256 hex (needs out_path_sz large enough). */
int tbl_cas_object_path(const char *repo_root, const char *sha256hex,
                        char *out_path, size_t out_path_sz);

#ifdef TBL_CAS_IMPLEMENTATION

#include <stdio.h>
#include <string.h>

#include "core/safe.h"
#include "core/path.h"
#include "core/sha256.h"
#include "core/str.h"
#include "os/fs.h"

static void tbl_cas_seterr(char *err, size_t errsz, const char *msg)
{
    if (!err || errsz == 0) return;
    err[0] = '\0';
    if (!msg) msg = "cas error";
    (void)tbl_strlcpy(err, msg, errsz);
}

static int tbl_cas_hash_file(const char *path, char *out_hex, size_t out_hex_sz, char *err, size_t errsz)
{
    FILE *fp;
    unsigned char buf[16384];
    size_t rd;
    tbl_sha256_t st;
    unsigned char dig[32];

    if (!path || !path[0] || !out_hex) {
        tbl_cas_seterr(err, errsz, "invalid args");
        return 1;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        tbl_cas_seterr(err, errsz, "cannot open input");
        return 1;
    }

    tbl_sha256_init(&st);

    for (;;) {
        rd = fread(buf, 1, sizeof(buf), fp);
        if (rd > 0) {
            tbl_sha256_update(&st, buf, rd);
        }
        if (rd < sizeof(buf)) {
            if (ferror(fp)) {
                fclose(fp);
                tbl_cas_seterr(err, errsz, "read error");
                return 1;
            }
            break;
        }
    }

    fclose(fp);
    tbl_sha256_final(&st, dig);

    if (!tbl_sha256_hex_ok(dig, out_hex, out_hex_sz)) {
        tbl_cas_seterr(err, errsz, "hex buffer too small");
        return 1;
    }

    return 0;
}

int tbl_cas_object_path(const char *repo_root, const char *sha256hex,
                        char *out_path, size_t out_path_sz)
{
    char d1[4];
    char rest[70];
    char p1[1024];

    if (!repo_root || !repo_root[0] || !sha256hex || !out_path) return 0;
    if (tbl_strlen(sha256hex) != 64) return 0;

    d1[0] = sha256hex[0];
    d1[1] = sha256hex[1];
    d1[2] = '\0';

    rest[0] = '\0';
    if (tbl_strlcpy(rest, sha256hex + 2, sizeof(rest)) >= sizeof(rest)) return 0;

    if (!tbl_path_join2(p1, sizeof(p1), repo_root, "sha256")) return 0;
    if (!tbl_path_join2(out_path, out_path_sz, p1, d1)) return 0;

    /* out_path currently dir; append rest as filename */
    if (!tbl_path_join2(out_path, out_path_sz, out_path, rest)) return 0;

    return 1;
}

static int tbl_cas_copy_file(const char *src, const char *dst_tmp, const char *dst_final, char *err, size_t errsz)
{
    FILE *in;
    FILE *out;
    unsigned char buf[16384];
    size_t rd;

    in = fopen(src, "rb");
    if (!in) { tbl_cas_seterr(err, errsz, "cannot open input"); return 1; }

    out = fopen(dst_tmp, "wb");
    if (!out) { fclose(in); tbl_cas_seterr(err, errsz, "cannot create temp"); return 1; }

    for (;;) {
        rd = fread(buf, 1, sizeof(buf), in);
        if (rd > 0) {
            if (fwrite(buf, 1, rd, out) != rd) {
                fclose(in);
                fclose(out);
                (void)tbl_fs_remove_file(dst_tmp);
                tbl_cas_seterr(err, errsz, "write error");
                return 1;
            }
        }
        if (rd < sizeof(buf)) {
            if (ferror(in)) {
                fclose(in);
                fclose(out);
                (void)tbl_fs_remove_file(dst_tmp);
                tbl_cas_seterr(err, errsz, "read error");
                return 1;
            }
            break;
        }
    }

    fclose(in);
    if (fclose(out) != 0) {
        (void)tbl_fs_remove_file(dst_tmp);
        tbl_cas_seterr(err, errsz, "flush error");
        return 1;
    }

    if (tbl_fs_rename_atomic(dst_tmp, dst_final, 0) != 0) {
        (void)tbl_fs_remove_file(dst_tmp);
        tbl_cas_seterr(err, errsz, "rename error");
        return 1;
    }

    return 0;
}

int tbl_cas_put_file(const char *repo_root, const char *src_path,
                     char *out_sha256hex, size_t out_sha256hex_sz,
                     char *err, size_t errsz)
{
    char sha[65];
    char objpath[1024];
    char objdir[1024];
    char tmp[1100];
    int ex;
    size_t n;

    if (err && errsz) err[0] = '\0';

    if (!repo_root || !repo_root[0] || !src_path || !src_path[0]) {
        tbl_cas_seterr(err, errsz, "invalid args");
        return 1;
    }

    if (tbl_cas_hash_file(src_path, sha, sizeof(sha), err, errsz) != 0) {
        return 1;
    }

    if (!tbl_cas_object_path(repo_root, sha, objpath, sizeof(objpath))) {
        tbl_cas_seterr(err, errsz, "object path too long");
        return 1;
    }

    /* mkdir <repo>/sha256/ab */
    if (tbl_strlcpy(objdir, objpath, sizeof(objdir)) >= sizeof(objdir)) {
        tbl_cas_seterr(err, errsz, "path too long");
        return 1;
    }
    /* chop filename */
    n = tbl_strlen(objdir);
    while (n > 0) {
        if (objdir[n-1] == '/' || objdir[n-1] == '\\') break;
        n--;
    }
    if (n == 0) { tbl_cas_seterr(err, errsz, "bad path"); return 1; }
    objdir[n-1] = '\0';

    if (tbl_fs_mkdir_p(objdir) != 0) {
        tbl_cas_seterr(err, errsz, "cannot create object dir");
        return 1;
    }

    ex = 0;
    (void)tbl_fs_exists(objpath, &ex);
    if (!ex) {
        /* tmp path: <objpath>.tmp.<pid> */
        if (tbl_strlcpy(tmp, objpath, sizeof(tmp)) >= sizeof(tmp) ||
            tbl_strlcat(tmp, ".tmp.", sizeof(tmp)) >= sizeof(tmp)) {
            tbl_cas_seterr(err, errsz, "tmp path too long");
            return 1;
        }
        {
            char num[16];
            if (!tbl_u32_to_dec_ok(tbl_fs_pid_u32(), num, sizeof(num))) {
                tbl_cas_seterr(err, errsz, "pid conv failed");
                return 1;
            }
            if (tbl_strlcat(tmp, num, sizeof(tmp)) >= sizeof(tmp)) {
                tbl_cas_seterr(err, errsz, "tmp path too long");
                return 1;
            }
        }

        if (tbl_cas_copy_file(src_path, tmp, objpath, err, errsz) != 0) {
            return 1;
        }
    }

    if (out_sha256hex && out_sha256hex_sz) {
        if (tbl_strlcpy(out_sha256hex, sha, out_sha256hex_sz) >= out_sha256hex_sz) {
            tbl_cas_seterr(err, errsz, "sha buffer too small");
            return 1;
        }
    }

    return 0;
}

#endif /* TBL_CAS_IMPLEMENTATION */

#endif /* TBL_CORE_CAS_H */
