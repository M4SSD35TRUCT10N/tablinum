#ifndef TBL_CORE_PACKAGE_H
#define TBL_CORE_PACKAGE_H

#include <stddef.h>
#include <time.h>
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
    if (!tbl_sha256_hex_ok(dig, out_hex, 65)) {
        tbl_pkg_seterr(err, errsz, "hex buffer too small");
        return 2;
    }
    return 0;
}

static int tbl_pkg_extract_events(const char *repo_root,
                                 const char *jobid,
                                 const char *out_path,
                                 unsigned long *out_lines,
                                 int *out_is_job_stream,
                                 char *err, size_t errsz)
{
    char legacy_path[1024];
    char jobs_dir[1024];
    char job_dir[1024];
    char job_path[1024];

    FILE *in;
    int is_job_stream;
    FILE *out;
    char line[2048];
    char needle1[512];
    char needle2[512];
    unsigned long nlines;

    if (out_lines) *out_lines = 0UL;
    if (out_is_job_stream) *out_is_job_stream = 0;

    if (!repo_root || !repo_root[0] || !jobid || !jobid[0] || !out_path || !out_path[0]) {
        tbl_pkg_seterr(err, errsz, "invalid args");
        return 2;
    }

    /* Always create out_path (empty is fine if no source exists). */
    out = fopen(out_path, "wb");
    if (!out) {
        tbl_pkg_seterr(err, errsz, "cannot create events log");
        return 2;
    }

    /* Prefer per-job events: <repo_root>/jobs/<jobid>/events.log */
    in = 0;
    is_job_stream = 0;
    if (tbl_path_join2(jobs_dir, sizeof(jobs_dir), repo_root, "jobs") &&
        tbl_path_join2(job_dir, sizeof(job_dir), jobs_dir, jobid) &&
        tbl_path_join2(job_path, sizeof(job_path), job_dir, "events.log")) {
        in = fopen(job_path, "rb");
        if (in) { is_job_stream = 1; if (out_is_job_stream) *out_is_job_stream = 1; }
    }

    /* Fallback legacy combined stream: <repo_root>/events.log filtered by job=<jobid> */
    if (!in) {
        if (!tbl_path_join2(legacy_path, sizeof(legacy_path), repo_root, "events.log")) {
            fclose(out);
            tbl_pkg_seterr(err, errsz, "events path too long");
            return 2;
        }
        in = fopen(legacy_path, "rb");
    }

    /* If no source exists, keep empty file. */
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
        int match = 1;

        if (!is_job_stream) {
            match = (strstr(line, needle1) != 0) || (strstr(line, needle2) != 0);
        }

        if (match) {
            /* normalize to LF-only for deterministic packages */
            char clean[2048];
            size_t i, j;
            j = 0;
            for (i = 0; line[i] && j + 1 < sizeof(clean); ++i) {
                if (line[i] == '\r') continue;
                clean[j++] = line[i];
            }
            clean[j] = '\0';

            if (fputs(clean, out) == EOF) {
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
                                    unsigned long created_utc,
                                    const char *events_source,
                                    const char *tool_commit_opt,
                                    char *err, size_t errsz)
{
    FILE *fp;

    if (!path || !path[0] || !kind_str || !kind_str[0] || !jobid || !jobid[0]) {
        tbl_pkg_seterr(err, errsz, "invalid args");
        return 2;
    }

    fp = fopen(path, "wb");
    if (!fp) {
        tbl_pkg_seterr(err, errsz, "cannot create package.ini");
        return 2;
    }

        /* Strict, versioned schema core (deterministic order, LF-only). */
    {
        char nbuf[32];
        const char *es;

        es = (events_source && events_source[0]) ? events_source : "legacy";
        if (!tbl_ul_to_dec_ok(created_utc, nbuf, sizeof(nbuf))) {
            fclose(fp);
            tbl_pkg_seterr(err, errsz, "package.ini created_utc encode error");
            return 2;
        }

        if (!tbl_fputs_ok(fp, "[package]\n") ||
            !tbl_fputs_ok(fp, "schema_version = 1\n") ||
            !tbl_fputs3_ok(fp, "kind = ", kind_str, "\n") ||
            !tbl_fputs3_ok(fp, "jobid = ", jobid, "\n") ||
            !tbl_fputs3_ok(fp, "created_utc = ", nbuf, "\n") ||
            !tbl_fputs3_ok(fp, "events_source = ", es, "\n") ||
            !tbl_fputs3_ok(fp, "tool_version = ", TBL_VERSION, "\n")) {
            fclose(fp);
            tbl_pkg_seterr(err, errsz, "package.ini write error");
            return 2;
        }
    }

    if (tool_commit_opt && tool_commit_opt[0]) {
        if (!tbl_fputs3_ok(fp, "tool_commit = ", tool_commit_opt, "\n")) {
            fclose(fp);
            tbl_pkg_seterr(err, errsz, "package.ini write error");
            return 2;
        }
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
    if (!tbl_fputs4_ok(fp, payload_sha, "  ", payload_rel, "\n") ||
        !tbl_fputs3_ok(fp, record_sha, "  metadata/record.ini", "\n") ||
        !tbl_fputs3_ok(fp, package_sha, "  metadata/package.ini", "\n") ||
        !tbl_fputs3_ok(fp, events_sha, "  metadata/events.log", "\n")) {
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
    int events_is_job;

    int rc;
    int ex;

    const char *kind_str;
    const char *tool_commit;

    if (err && errsz) err[0] = '\0';
    if (!repo_root || !repo_root[0] || !jobid || !jobid[0] || !out_dir || !out_dir[0]) {
        tbl_pkg_seterr(err, errsz, "invalid args");
        return 2;
    }

    kind_str = (kind == TBL_PKG_SIP) ? "sip" : "aip";

    /* Optional tool commit (derived from build meta, if present). */
    tool_commit = 0;
#ifdef TBL_BUILD_META
    if (TBL_BUILD_META[0]) {
        tool_commit = TBL_BUILD_META;
        if (tool_commit[0] == '+') tool_commit++;
    }
#endif

    /* ensure package root exists */
    if (tbl_fs_mkdir_p(out_dir) != 0) {
        tbl_pkg_seterr(err, errsz, "cannot create output dir");
        return 2;
    }

    /* read durable record */
    rc = tbl_record_read_repo(repo_root, jobid, &rec, err, errsz);
    if (rc != 0) {
        (void)0; /* package: no repo side effects */
        return 2;
    }

    /* For now: require ok record for both AIP and SIP packaging. */
    if (!tbl_streq(rec.status, "ok")) {
        tbl_pkg_seterr(err, errsz, "record status is not ok");
        (void)0; /* package: no repo side effects */
        return 2;
    }

    if (!tbl_cas_object_path(repo_root, rec.sha256, objpath, sizeof(objpath))) {
        tbl_pkg_seterr(err, errsz, "object path too long");
        (void)0; /* package: no repo side effects */
        return 2;
    }

    ex = 0;
    (void)tbl_fs_exists(objpath, &ex);
    if (!ex) {
        tbl_pkg_seterr(err, errsz, "object missing");
        (void)0; /* package: no repo side effects */
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
        (void)0; /* package: no repo side effects */
        return 2;
    }

    /* record.ini */
    if (!tbl_path_join2(out_record, sizeof(out_record), meta_dir, "record.ini")) {
        tbl_pkg_seterr(err, errsz, "record output path too long");
        return 2;
    }

    rc = tbl_pkg_copy_file(record_path, out_record, err, errsz);
    if (rc != 0) {
        (void)0; /* package: no repo side effects */
        return 2;
    }

    /* events.log (filtered) */
    if (!tbl_path_join2(out_events, sizeof(out_events), meta_dir, "events.log")) {
        tbl_pkg_seterr(err, errsz, "events output path too long");
        return 2;
    }
    events_lines = 0UL;
    events_is_job = 0;
    rc = tbl_pkg_extract_events(repo_root, jobid, out_events, &events_lines, &events_is_job, err, errsz);
    if (rc != 0) {
        (void)0; /* package: no repo side effects */
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

    created_ts = (rec.stored_at != 0UL) ? rec.stored_at : (unsigned long)time(0);
    rc = tbl_pkg_write_package_ini(out_package_ini,
                                kind_str,
                                jobid,
                                created_ts,
                                events_is_job ? "job" : "legacy",
                                tool_commit,
                                err, errsz);
    if (rc != 0) {
        (void)0; /* package: no repo side effects */
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
        (void)0; /* package: no repo side effects */
        return 2;
    }

    (void)0; /* package: no repo side effects */
    return 0;
}

#endif /* TBL_PACKAGE_IMPLEMENTATION */

#endif /* TBL_CORE_PACKAGE_H */
