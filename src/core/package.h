#ifndef TBL_CORE_PACKAGE_H
#define TBL_CORE_PACKAGE_H

#include <stddef.h>
#include "tablinum.h"

/* Build an E-ARK inspired, OAIS-light package from a stored job.

   Layout (package root = out_dir):
     metadata/
       record.ini
       package.ini
       events.log          (filtered lines for this job)
       manifest-sha256.txt (sha256sum-compatible list of files in this package)
     representations/
       rep0/
         data/
           <payload>

   KIND:
     - AIP: archival package (durable record must be status=ok)
     - SIP: submission-style package (currently identical layout; kind stored in package.ini)

   Returns 0 on success, non-zero on error.
*/
int tbl_package_job(const char *repo_root,
                    const char *jobid,
                    const char *out_dir,
                    tbl_pkg_kind_t kind,
                    char *err, size_t errsz);

#ifdef TBL_PACKAGE_IMPLEMENTATION

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "core/safe.h"
#include "core/str.h"
#include "core/path.h"
#include "core/cas.h"
#include "core/record.h"
#include "core/events.h"
#include "core/sha256.h"
#include "os/fs.h"

static void tbl_pkg_seterr(char *err, size_t errsz, const char *msg)
{
    if (!err || errsz == 0) return;
    err[0] = '\0';
    if (!msg) msg = "package error";
    (void)tbl_strlcpy(err, msg, errsz);
}

static int tbl_pkg_copy_file(const char *src, const char *dst, char *err, size_t errsz)
{
    FILE *in;
    FILE *out;
    unsigned char buf[16384];
    size_t rd;

    in = fopen(src, "rb");
    if (!in) { tbl_pkg_seterr(err, errsz, "cannot open source"); return 2; }

    out = fopen(dst, "wb");
    if (!out) { fclose(in); tbl_pkg_seterr(err, errsz, "cannot create destination"); return 2; }

    for (;;) {
        rd = fread(buf, 1, sizeof(buf), in);
        if (rd > 0) {
            if (fwrite(buf, 1, rd, out) != rd) {
                fclose(in);
                fclose(out);
                tbl_pkg_seterr(err, errsz, "write error");
                return 2;
            }
        }
        if (rd < sizeof(buf)) {
            if (ferror(in)) {
                fclose(in);
                fclose(out);
                tbl_pkg_seterr(err, errsz, "read error");
                return 2;
            }
            break;
        }
    }

    fclose(in);
    if (fclose(out) != 0) {
        tbl_pkg_seterr(err, errsz, "flush error");
        return 2;
    }

    return 0;
}

static int tbl_pkg_hash_file_hex(const char *path, char out_hex[65], char *err, size_t errsz)
{
    FILE *fp;
    unsigned char buf[16384];
    size_t rd;
    tbl_sha256_t st;
    unsigned char dig[32];

    if (!out_hex) {
        tbl_pkg_seterr(err, errsz, "invalid args");
        return 2;
    }
    out_hex[0] = '\0';

    if (!path || !path[0]) {
        tbl_pkg_seterr(err, errsz, "invalid args");
        return 2;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        tbl_pkg_seterr(err, errsz, "cannot open input");
        return 2;
    }

    tbl_sha256_init(&st);
    for (;;) {
        rd = fread(buf, 1, sizeof(buf), fp);
        if (rd > 0) tbl_sha256_update(&st, buf, rd);
        if (rd < sizeof(buf)) {
            if (ferror(fp)) {
                fclose(fp);
                tbl_pkg_seterr(err, errsz, "read error");
                return 2;
            }
            break;
        }
    }
    fclose(fp);

    tbl_sha256_final(&st, dig);
    if (!tbl_sha256_hex(dig, out_hex, 65)) {
        tbl_pkg_seterr(err, errsz, "hex buffer too small");
        return 2;
    }
    return 0;
}

static int tbl_pkg_extract_events(const char *repo_root,
                                 const char *jobid,
                                 const char *out_path,
                                 unsigned long *out_lines,
                                 char *err, size_t errsz)
{
    char src_path[1024];
    FILE *in;
    FILE *out;
    char line[2048];
    char needle1[512];
    char needle2[512];
    unsigned long nlines;

    if (out_lines) *out_lines = 0UL;

    if (!repo_root || !repo_root[0] || !jobid || !jobid[0] || !out_path || !out_path[0]) {
        tbl_pkg_seterr(err, errsz, "invalid args");
        return 2;
    }

    if (!tbl_path_join2(src_path, sizeof(src_path), repo_root, "events.log")) {
        tbl_pkg_seterr(err, errsz, "events path too long");
        return 2;
    }

    /* Always create out_path (empty is fine if src missing). */
    out = fopen(out_path, "wb");
    if (!out) {
        tbl_pkg_seterr(err, errsz, "cannot create events log");
        return 2;
    }

    /* If repo events.log does not exist, keep empty file. */
    in = fopen(src_path, "rb");
    if (!in) {
        (void)fclose(out);
        return 0;
    }

    needle1[0] = '\0';
    needle2[0] = '\0';
    (void)tbl_strlcpy(needle1, " job=", sizeof(needle1));
    (void)tbl_strlcat(needle1, jobid, sizeof(needle1));
    (void)tbl_strlcpy(needle2, "job=", sizeof(needle2));
    (void)tbl_strlcat(needle2, jobid, sizeof(needle2));

    nlines = 0UL;
    while (fgets(line, (int)sizeof(line), in) != 0) {
        if (strstr(line, needle1) || strstr(line, needle2)) {
            if (fputs(line, out) == EOF) {
                fclose(in);
                fclose(out);
                tbl_pkg_seterr(err, errsz, "events write error");
                return 2;
            }
            nlines++;
        }
    }

    fclose(in);
    if (fclose(out) != 0) {
        tbl_pkg_seterr(err, errsz, "events flush error");
        return 2;
    }

    if (out_lines) *out_lines = nlines;
    return 0;
}

static int tbl_pkg_write_package_ini(const char *path,
                                    const char *kind_str,
                                    const char *jobid,
                                    const char *payload_name,
                                    const char *payload_sha,
                                    const char *record_sha,
                                    const char *cas_sha,
                                    unsigned long created_ts,
                                    unsigned long events_lines,
                                    char *err, size_t errsz)
{
    FILE *fp;

    if (!path || !path[0] || !kind_str || !kind_str[0] || !jobid || !jobid[0] ||
        !payload_name || !payload_name[0] || !payload_sha || !payload_sha[0] ||
        !record_sha || !record_sha[0] || !cas_sha || !cas_sha[0]) {
        tbl_pkg_seterr(err, errsz, "invalid args");
        return 2;
    }

    fp = fopen(path, "wb");
    if (!fp) {
        tbl_pkg_seterr(err, errsz, "cannot create package.ini");
        return 2;
    }

    if (fprintf(fp,
                "[package]\n"
                "schema = tablinum.package.v1\n"
                "kind = %s\n"
                "jobid = %s\n"
                "created_ts = %lu\n"
                "tool = %s %s\n"
                "payload_name = %s\n"
                "payload_sha256 = %s\n"
                "record_sha256 = %s\n"
                "cas_sha256 = %s\n"
                "events_lines = %lu\n",
                kind_str,
                jobid,
                created_ts,
                TBL_NAME, TBL_VERSION,
                payload_name,
                payload_sha,
                record_sha,
                cas_sha,
                events_lines) < 0) {
        fclose(fp);
        tbl_pkg_seterr(err, errsz, "package.ini write error");
        return 2;
    }

    if (fclose(fp) != 0) {
        tbl_pkg_seterr(err, errsz, "package.ini flush error");
        return 2;
    }

    return 0;
}

static int tbl_pkg_write_manifest(const char *path,
                                 const char *payload_rel,
                                 const char *payload_sha,
                                 const char *record_sha,
                                 const char *package_sha,
                                 const char *events_sha,
                                 char *err, size_t errsz)
{
    FILE *fp;

    if (!path || !path[0] || !payload_rel || !payload_rel[0] ||
        !payload_sha || !payload_sha[0] || !record_sha || !record_sha[0] ||
        !package_sha || !package_sha[0] || !events_sha || !events_sha[0]) {
        tbl_pkg_seterr(err, errsz, "invalid args");
        return 2;
    }

    fp = fopen(path, "wb");
    if (!fp) {
        tbl_pkg_seterr(err, errsz, "cannot create manifest");
        return 2;
    }

    /* sha256sum-compatible: <hex><two spaces><relative path> */
    if (fprintf(fp, "%s  %s\n", payload_sha, payload_rel) < 0 ||
        fprintf(fp, "%s  metadata/record.ini\n", record_sha) < 0 ||
        fprintf(fp, "%s  metadata/package.ini\n", package_sha) < 0 ||
        fprintf(fp, "%s  metadata/events.log\n", events_sha) < 0) {
        fclose(fp);
        tbl_pkg_seterr(err, errsz, "manifest write error");
        return 2;
    }

    if (fclose(fp) != 0) {
        tbl_pkg_seterr(err, errsz, "manifest flush error");
        return 2;
    }

    return 0;
}

int tbl_package_job(const char *repo_root,
                    const char *jobid,
                    const char *out_dir,
                    tbl_pkg_kind_t kind,
                    char *err, size_t errsz)
{
    tbl_record_t rec;
    char objpath[1024];
    char record_path[1024];

    char meta_dir[1024];
    char rep_data_dir[1024];

    char out_payload[1024];
    char out_record[1024];
    char out_events[1024];
    char out_package_ini[1024];
    char out_manifest[1024];

    char payload_rel[1024];

    char sha_payload[65];
    char sha_record[65];
    char sha_events[65];
    char sha_package[65];

    unsigned long created_ts;
    unsigned long events_lines;

    int rc;
    int ex;

    const char *kind_str;

    if (err && errsz) err[0] = '\0';
    if (!repo_root || !repo_root[0] || !jobid || !jobid[0] || !out_dir || !out_dir[0]) {
        tbl_pkg_seterr(err, errsz, "invalid args");
        return 2;
    }

    kind_str = (kind == TBL_PKG_SIP) ? "sip" : "aip";

    /* ensure package root exists */
    if (tbl_fs_mkdir_p(out_dir) != 0) {
        tbl_pkg_seterr(err, errsz, "cannot create output dir");
        return 2;
    }

    /* read durable record */
    rc = tbl_record_read_repo(repo_root, jobid, &rec, err, errsz);
    if (rc != 0) {
        (void)tbl_events_append(repo_root, "package.error", jobid, "error", "", err && err[0] ? err : "record read failed", 0, 0);
        return 2;
    }

    /* For now: require ok record for both AIP and SIP packaging. */
    if (!tbl_streq(rec.status, "ok")) {
        tbl_pkg_seterr(err, errsz, "record status is not ok");
        (void)tbl_events_append(repo_root, "package.skip", jobid, rec.status, rec.sha256, rec.reason, 0, 0);
        return 2;
    }

    if (!tbl_cas_object_path(repo_root, rec.sha256, objpath, sizeof(objpath))) {
        tbl_pkg_seterr(err, errsz, "object path too long");
        (void)tbl_events_append(repo_root, "package.error", jobid, "error", rec.sha256, "object path too long", 0, 0);
        return 2;
    }

    ex = 0;
    (void)tbl_fs_exists(objpath, &ex);
    if (!ex) {
        tbl_pkg_seterr(err, errsz, "object missing");
        (void)tbl_events_append(repo_root, "package.fail", jobid, "fail", rec.sha256, "object missing", 0, 0);
        return 2;
    }

    if (!tbl_record_path(repo_root, jobid, record_path, sizeof(record_path))) {
        tbl_pkg_seterr(err, errsz, "record path too long");
        return 2;
    }

    /* create directories */
    if (!tbl_path_join2(meta_dir, sizeof(meta_dir), out_dir, "metadata")) {
        tbl_pkg_seterr(err, errsz, "metadata path too long");
        return 2;
    }
    if (tbl_fs_mkdir_p(meta_dir) != 0) {
        tbl_pkg_seterr(err, errsz, "cannot create metadata dir");
        return 2;
    }

    if (!tbl_path_join2(rep_data_dir, sizeof(rep_data_dir), out_dir, "representations/rep0/data")) {
        tbl_pkg_seterr(err, errsz, "representations path too long");
        return 2;
    }
    if (tbl_fs_mkdir_p(rep_data_dir) != 0) {
        tbl_pkg_seterr(err, errsz, "cannot create representations dir");
        return 2;
    }

    /* payload */
    if (!tbl_path_join2(out_payload, sizeof(out_payload), rep_data_dir, rec.payload[0] ? rec.payload : "payload.bin")) {
        tbl_pkg_seterr(err, errsz, "payload output path too long");
        return 2;
    }

    rc = tbl_pkg_copy_file(objpath, out_payload, err, errsz);
    if (rc != 0) {
        (void)tbl_events_append(repo_root, "package.error", jobid, "error", rec.sha256, err && err[0] ? err : "copy payload failed", 0, 0);
        return 2;
    }

    /* record.ini */
    if (!tbl_path_join2(out_record, sizeof(out_record), meta_dir, "record.ini")) {
        tbl_pkg_seterr(err, errsz, "record output path too long");
        return 2;
    }

    rc = tbl_pkg_copy_file(record_path, out_record, err, errsz);
    if (rc != 0) {
        (void)tbl_events_append(repo_root, "package.error", jobid, "error", rec.sha256, err && err[0] ? err : "copy record failed", 0, 0);
        return 2;
    }

    /* events.log (filtered) */
    if (!tbl_path_join2(out_events, sizeof(out_events), meta_dir, "events.log")) {
        tbl_pkg_seterr(err, errsz, "events output path too long");
        return 2;
    }
    events_lines = 0UL;
    rc = tbl_pkg_extract_events(repo_root, jobid, out_events, &events_lines, err, errsz);
    if (rc != 0) {
        (void)tbl_events_append(repo_root, "package.error", jobid, "error", rec.sha256, err && err[0] ? err : "events extract failed", 0, 0);
        return 2;
    }

    /* compute hashes */
    rc = tbl_pkg_hash_file_hex(out_payload, sha_payload, err, errsz);
    if (rc != 0) return 2;
    rc = tbl_pkg_hash_file_hex(out_record, sha_record, err, errsz);
    if (rc != 0) return 2;
    rc = tbl_pkg_hash_file_hex(out_events, sha_events, err, errsz);
    if (rc != 0) return 2;

    /* package.ini */
    if (!tbl_path_join2(out_package_ini, sizeof(out_package_ini), meta_dir, "package.ini")) {
        tbl_pkg_seterr(err, errsz, "package.ini path too long");
        return 2;
    }

    created_ts = (unsigned long)time(0);
    rc = tbl_pkg_write_package_ini(out_package_ini,
                                  kind_str,
                                  jobid,
                                  rec.payload[0] ? rec.payload : "payload.bin",
                                  sha_payload,
                                  sha_record,
                                  rec.sha256,
                                  created_ts,
                                  events_lines,
                                  err, errsz);
    if (rc != 0) {
        (void)tbl_events_append(repo_root, "package.error", jobid, "error", rec.sha256, err && err[0] ? err : "write package.ini failed", 0, 0);
        return 2;
    }

    rc = tbl_pkg_hash_file_hex(out_package_ini, sha_package, err, errsz);
    if (rc != 0) return 2;

    /* manifest-sha256.txt */
    if (!tbl_path_join2(out_manifest, sizeof(out_manifest), meta_dir, "manifest-sha256.txt")) {
        tbl_pkg_seterr(err, errsz, "manifest path too long");
        return 2;
    }

    payload_rel[0] = '\0';
    (void)tbl_strlcpy(payload_rel, "representations/rep0/data/", sizeof(payload_rel));
    (void)tbl_strlcat(payload_rel, rec.payload[0] ? rec.payload : "payload.bin", sizeof(payload_rel));

    rc = tbl_pkg_write_manifest(out_manifest,
                               payload_rel,
                               sha_payload,
                               sha_record,
                               sha_package,
                               sha_events,
                               err, errsz);
    if (rc != 0) {
        (void)tbl_events_append(repo_root, "package.error", jobid, "error", rec.sha256, err && err[0] ? err : "manifest write failed", 0, 0);
        return 2;
    }

    (void)tbl_events_append(repo_root, "package.ok", jobid, kind_str, rec.sha256, "", 0, 0);
    return 0;
}

#endif /* TBL_PACKAGE_IMPLEMENTATION */

#endif /* TBL_CORE_PACKAGE_H */
