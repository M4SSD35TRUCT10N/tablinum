#ifndef TBL_CORE_EXPORT_H
#define TBL_CORE_EXPORT_H

#include <stddef.h>

/* Export a job (DIP-light):
   - reads <repo_root>/records/<jobid>.ini
   - locates CAS object by sha256
   - writes payload to <out_dir>/<payload>
   - copies record to <out_dir>/record.ini
   Returns 0 on success, non-zero on error.
*/

int tbl_export_job(const char *repo_root,
                   const char *jobid,
                   const char *out_dir,
                   char *err, size_t errsz);

#ifdef TBL_EXPORT_IMPLEMENTATION

#include <stdio.h>
#include <string.h>

#include "core/safe.h"
#include "core/str.h"
#include "core/path.h"
#include "core/cas.h"
#include "core/record.h"
#include "core/events.h"
#include "core/sha256.h"
#include "os/fs.h"

static void tbl_export_seterr(char *err, size_t errsz, const char *msg)
{
    if (!err || errsz == 0) return;
    err[0] = '\0';
    if (!msg) msg = "export error";
    (void)tbl_strlcpy(err, msg, errsz);
}

static int tbl_export_copy_file(const char *src, const char *dst, char *err, size_t errsz)
{
    FILE *in;
    FILE *out;
    unsigned char buf[16384];
    size_t rd;

    in = fopen(src, "rb");
    if (!in) { tbl_export_seterr(err, errsz, "cannot open source"); return 2; }

    out = fopen(dst, "wb");
    if (!out) { fclose(in); tbl_export_seterr(err, errsz, "cannot create destination"); return 2; }

    for (;;) {
        rd = fread(buf, 1, sizeof(buf), in);
        if (rd > 0) {
            if (fwrite(buf, 1, rd, out) != rd) {
                fclose(in);
                fclose(out);
                tbl_export_seterr(err, errsz, "write error");
                return 2;
            }
        }
        if (rd < sizeof(buf)) {
            if (ferror(in)) {
                fclose(in);
                fclose(out);
                tbl_export_seterr(err, errsz, "read error");
                return 2;
            }
            break;
        }
    }

    fclose(in);
    if (fclose(out) != 0) {
        tbl_export_seterr(err, errsz, "flush error");
        return 2;
    }

    return 0;
}

static int tbl_export_hash_file_hex(const char *path, char out_hex[65], char *err, size_t errsz)
{
    FILE *fp;
    unsigned char buf[16384];
    size_t rd;
    tbl_sha256_t st;
    unsigned char dig[32];

    if (!out_hex) {
        tbl_export_seterr(err, errsz, "invalid args");
        return 2;
    }
    out_hex[0] = '\0';

    if (!path || !path[0]) {
        tbl_export_seterr(err, errsz, "invalid args");
        return 2;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        tbl_export_seterr(err, errsz, "cannot open input");
        return 2;
    }

    tbl_sha256_init(&st);
    for (;;) {
        rd = fread(buf, 1, sizeof(buf), fp);
        if (rd > 0) tbl_sha256_update(&st, buf, rd);
        if (rd < sizeof(buf)) {
            if (ferror(fp)) {
                fclose(fp);
                tbl_export_seterr(err, errsz, "read error");
                return 2;
            }
            break;
        }
    }
    fclose(fp);

    tbl_sha256_final(&st, dig);
    if (!tbl_sha256_hex(dig, out_hex, 65)) {
        tbl_export_seterr(err, errsz, "hex buffer too small");
        return 2;
    }
    return 0;
}

static int tbl_export_write_manifest(const char *manifest_path,
                                    const char *payload_name,
                                    const char payload_sha[65],
                                    const char record_sha[65],
                                    char *err, size_t errsz)
{
    FILE *fp;

    if (!manifest_path || !manifest_path[0] || !payload_name || !payload_name[0] ||
        !payload_sha || !payload_sha[0] || !record_sha || !record_sha[0]) {
        tbl_export_seterr(err, errsz, "invalid args");
        return 2;
    }

    fp = fopen(manifest_path, "wb");
    if (!fp) {
        tbl_export_seterr(err, errsz, "cannot create manifest");
        return 2;
    }

    /* sha256sum-compatible: <hex><two spaces><relative path> */
    if (fprintf(fp, "%s  %s\n", payload_sha, payload_name) < 0 ||
        fprintf(fp, "%s  record.ini\n", record_sha) < 0) {
        fclose(fp);
        tbl_export_seterr(err, errsz, "manifest write error");
        return 2;
    }

    if (fclose(fp) != 0) {
        tbl_export_seterr(err, errsz, "manifest flush error");
        return 2;
    }

    return 0;
}

int tbl_export_job(const char *repo_root,
                   const char *jobid,
                   const char *out_dir,
                   char *err, size_t errsz)
{
    tbl_record_t rec;
    char objpath[1024];
    char out_payload[1024];
    char out_record[1024];
    char out_manifest[1024];
    char record_path[1024];
    char payload_sha[65];
    char record_sha[65];
    int rc;
    int ex;

    if (err && errsz) err[0] = '\0';
    if (!repo_root || !repo_root[0] || !jobid || !jobid[0] || !out_dir || !out_dir[0]) {
        tbl_export_seterr(err, errsz, "invalid args");
        return 2;
    }

    /* ensure out dir exists */
    if (tbl_fs_mkdir_p(out_dir) != 0) {
        tbl_export_seterr(err, errsz, "cannot create output dir");
        return 2;
    }

    rc = tbl_record_read_repo(repo_root, jobid, &rec, err, errsz);
    if (rc != 0) {
        (void)tbl_events_append(repo_root, "export.error", jobid, "error", "", err && err[0] ? err : "record read failed", 0, 0);
        return 2;
    }

    if (!tbl_streq(rec.status, "ok")) {
        tbl_export_seterr(err, errsz, "record status is not ok");
        (void)tbl_events_append(repo_root, "export.skip", jobid, rec.status, rec.sha256, rec.reason, 0, 0);
        return 2;
    }

    if (!tbl_cas_object_path(repo_root, rec.sha256, objpath, sizeof(objpath))) {
        tbl_export_seterr(err, errsz, "object path too long");
        (void)tbl_events_append(repo_root, "export.error", jobid, "error", rec.sha256, "object path too long", 0, 0);
        return 2;
    }

    ex = 0;
    (void)tbl_fs_exists(objpath, &ex);
    if (!ex) {
        tbl_export_seterr(err, errsz, "object missing");
        (void)tbl_events_append(repo_root, "export.fail", jobid, "fail", rec.sha256, "object missing", 0, 0);
        return 2;
    }

    /* out payload */
    if (!tbl_path_join2(out_payload, sizeof(out_payload), out_dir, rec.payload[0] ? rec.payload : "payload.bin")) {
        tbl_export_seterr(err, errsz, "payload output path too long");
        return 2;
    }

    rc = tbl_export_copy_file(objpath, out_payload, err, errsz);
    if (rc != 0) {
        (void)tbl_events_append(repo_root, "export.error", jobid, "error", rec.sha256, err && err[0] ? err : "copy payload failed", 0, 0);
        return 2;
    }

    /* copy record file */
    if (!tbl_record_path(repo_root, jobid, record_path, sizeof(record_path))) {
        tbl_export_seterr(err, errsz, "record path too long");
        return 2;
    }

    if (!tbl_path_join2(out_record, sizeof(out_record), out_dir, "record.ini")) {
        tbl_export_seterr(err, errsz, "record output path too long");
        return 2;
    }

    rc = tbl_export_copy_file(record_path, out_record, err, errsz);
    if (rc != 0) {
        (void)tbl_events_append(repo_root, "export.error", jobid, "error", rec.sha256, err && err[0] ? err : "copy record failed", 0, 0);
        return 2;
    }

    /* manifest (DIP-light integrity): sha256sum-compatible list of exported files */
    if (!tbl_path_join2(out_manifest, sizeof(out_manifest), out_dir, "manifest-sha256.txt")) {
        tbl_export_seterr(err, errsz, "manifest output path too long");
        (void)tbl_events_append(repo_root, "export.error", jobid, "error", rec.sha256, "manifest output path too long", 0, 0);
        return 2;
    }

    /* hash exported files to ensure copy integrity (fail-fast) */
    rc = tbl_export_hash_file_hex(out_payload, payload_sha, err, errsz);
    if (rc != 0) {
        (void)tbl_events_append(repo_root, "export.error", jobid, "error", rec.sha256, err && err[0] ? err : "hash payload failed", 0, 0);
        return 2;
    }
    rc = tbl_export_hash_file_hex(out_record, record_sha, err, errsz);
    if (rc != 0) {
        (void)tbl_events_append(repo_root, "export.error", jobid, "error", rec.sha256, err && err[0] ? err : "hash record failed", 0, 0);
        return 2;
    }

    rc = tbl_export_write_manifest(out_manifest,
                                  rec.payload[0] ? rec.payload : "payload.bin",
                                  payload_sha,
                                  record_sha,
                                  err, errsz);
    if (rc != 0) {
        (void)tbl_events_append(repo_root, "export.error", jobid, "error", rec.sha256, err && err[0] ? err : "manifest write failed", 0, 0);
        return 2;
    }

    (void)tbl_events_append(repo_root, "export.ok", jobid, "ok", rec.sha256, "", 0, 0);
    return 0;
}

#endif /* TBL_EXPORT_IMPLEMENTATION */

#endif /* TBL_CORE_EXPORT_H */
