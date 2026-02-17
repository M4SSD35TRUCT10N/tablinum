#ifndef TBL_CORE_VERIFY_H
#define TBL_CORE_VERIFY_H

#include <stddef.h>

/* Verify a job record by recomputing SHA-256 from CAS object and comparing to record.
   Returns:
     0 = OK
     1 = mismatch / not verifiable (e.g. record status != ok)
     2 = hard error (I/O, missing record/object)
*/

int tbl_verify_job(const char *repo_root,
                   const char *jobid,
                   char *err, size_t errsz);

#ifdef TBL_VERIFY_IMPLEMENTATION

#include <stdio.h>
#include <string.h>

#include "core/safe.h"
#include "core/str.h"
#include "core/path.h"
#include "core/sha256.h"
#include "core/cas.h"
#include "core/record.h"
#include "core/events.h"
#include "os/fs.h"

static void tbl_verify_seterr(char *err, size_t errsz, const char *msg)
{
    if (!err || errsz == 0) return;
    err[0] = '\0';
    if (!msg) msg = "verify error";
    (void)tbl_strlcpy(err, msg, errsz);
}

static int tbl_verify_hash_file(const char *path, char *out_hex, size_t out_hex_sz, char *err, size_t errsz)
{
    FILE *fp;
    unsigned char buf[16384];
    size_t rd;
    tbl_sha256_t st;
    unsigned char dig[32];

    if (!path || !path[0] || !out_hex) {
        tbl_verify_seterr(err, errsz, "invalid args");
        return 2;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        tbl_verify_seterr(err, errsz, "cannot open object");
        return 2;
    }

    tbl_sha256_init(&st);

    for (;;) {
        rd = fread(buf, 1, sizeof(buf), fp);
        if (rd > 0) tbl_sha256_update(&st, buf, rd);
        if (rd < sizeof(buf)) {
            if (ferror(fp)) {
                fclose(fp);
                tbl_verify_seterr(err, errsz, "read error");
                return 2;
            }
            break;
        }
    }

    fclose(fp);
    tbl_sha256_final(&st, dig);

    if (!tbl_sha256_hex(dig, out_hex, out_hex_sz)) {
        tbl_verify_seterr(err, errsz, "hex buffer too small");
        return 2;
    }

    return 0;
}

int tbl_verify_job(const char *repo_root,
                   const char *jobid,
                   char *err, size_t errsz)
{
    tbl_record_t rec;
    char objpath[1024];
    char hex[65];
    int ex;
    int rc;

    if (err && errsz) err[0] = '\0';
    if (!repo_root || !repo_root[0] || !jobid || !jobid[0]) {
        tbl_verify_seterr(err, errsz, "invalid args");
        return 2;
    }

    rc = tbl_record_read_repo(repo_root, jobid, &rec, err, errsz);
    if (rc != 0) {
        (void)tbl_events_append(repo_root, "verify.error", jobid, "error", "", err && err[0] ? err : "record read failed", 0, 0);
        return 2;
    }

    if (!tbl_streq(rec.status, "ok")) {
        (void)tbl_events_append(repo_root, "verify.skip", jobid, rec.status, rec.sha256, rec.reason, 0, 0);
        tbl_verify_seterr(err, errsz, "record status is not ok");
        return 1;
    }

    if (!rec.sha256[0] || tbl_strlen(rec.sha256) != 64) {
        tbl_verify_seterr(err, errsz, "record sha256 missing/invalid");
        (void)tbl_events_append(repo_root, "verify.fail", jobid, "fail", "", "record sha256 invalid", 0, 0);
        return 1;
    }

    if (!tbl_cas_object_path(repo_root, rec.sha256, objpath, sizeof(objpath))) {
        tbl_verify_seterr(err, errsz, "object path too long");
        (void)tbl_events_append(repo_root, "verify.error", jobid, "error", rec.sha256, "object path too long", 0, 0);
        return 2;
    }

    ex = 0;
    (void)tbl_fs_exists(objpath, &ex);
    if (!ex) {
        tbl_verify_seterr(err, errsz, "object missing");
        (void)tbl_events_append(repo_root, "verify.fail", jobid, "fail", rec.sha256, "object missing", 0, 0);
        return 2;
    }

    hex[0] = '\0';
    rc = tbl_verify_hash_file(objpath, hex, sizeof(hex), err, errsz);
    if (rc != 0) {
        (void)tbl_events_append(repo_root, "verify.error", jobid, "error", rec.sha256, err && err[0] ? err : "hash failed", 0, 0);
        return 2;
    }

    if (!tbl_streq(hex, rec.sha256)) {
        tbl_verify_seterr(err, errsz, "sha256 mismatch");
        (void)tbl_events_append(repo_root, "verify.fail", jobid, "fail", rec.sha256, "sha256 mismatch", 0, 0);
        return 1;
    }

    (void)tbl_events_append(repo_root, "verify.ok", jobid, "ok", rec.sha256, "", 0, 0);
    return 0;
}

#endif /* TBL_VERIFY_IMPLEMENTATION */

#endif /* TBL_CORE_VERIFY_H */
