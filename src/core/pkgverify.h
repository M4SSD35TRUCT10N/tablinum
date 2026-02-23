#ifndef TBL_CORE_PKGVERIFY_H
#define TBL_CORE_PKGVERIFY_H

#include <stddef.h>

/* Verify a packaged OUTDIR created by `tablinum package` (E-ARK-lite v1).
 * Return Tablinum exit codes:
 *   0 ok
 *   3 notfound
 *   4 io
 *   5 integrity
 *   6 schema
 */
int tbl_verify_package_dir(const char *pkg_dir, char *err, size_t errsz);

/* Verify (strict) then ingest into repo (CAS + record + events).
 * Return Tablinum exit codes (see above).
 */
int tbl_ingest_package_dir(const char *repo_root, const char *pkg_dir, char *err, size_t errsz);

#ifdef TBL_PKGVERIFY_IMPLEMENTATION

#include <stdio.h>
#include <string.h>

#include "os/fs.h"
#include "core/path.h"
#include "core/safe.h"
#include "core/str.h"
#include "core/ini.h"
#include "core/record.h"
#include "core/cas.h"
#include "core/events.h"
#include "core/sha256.h"

/* Keep exit codes local to avoid include cycles with tablinum.c */
enum {
    TBLX_EXIT_OK = 0,
    TBLX_EXIT_USAGE = 2,
    TBLX_EXIT_NOTFOUND = 3,
    TBLX_EXIT_IO = 4,
    TBLX_EXIT_INTEGRITY = 5,
    TBLX_EXIT_SCHEMA = 6
};

static void tbl_pkgv_seterr(char *err, size_t errsz, const char *msg)
{
    if (!err || errsz == 0) return;
    err[0] = '\0';
    if (!msg) msg = "error";
    (void)tbl_strlcpy(err, msg, errsz);
}

static int tbl_pkgv_has_cr(const char *path, char *err, size_t errsz)
{
    FILE *fp;
    int c;

    fp = fopen(path, "rb");
    if (!fp) {
        tbl_pkgv_seterr(err, errsz, "cannot open file");
        return TBLX_EXIT_IO;
    }

    while ((c = fgetc(fp)) != EOF) {
        if (c == '\r') {
            fclose(fp);
            tbl_pkgv_seterr(err, errsz, "CR found (LF-only required)");
            return TBLX_EXIT_SCHEMA;
        }
    }

    if (ferror(fp)) {
        fclose(fp);
        tbl_pkgv_seterr(err, errsz, "read error");
        return TBLX_EXIT_IO;
    }

    fclose(fp);
    return TBLX_EXIT_OK;
}

static int tbl_pkgv_hash_file_hex(const char *path, char out_hex[65], char *err, size_t errsz)
{
    FILE *fp;
    unsigned char buf[4096];
    size_t n;
    tbl_sha256_t s;
    unsigned char digest[32];

    if (!path || !path[0] || !out_hex) {
        tbl_pkgv_seterr(err, errsz, "invalid args");
        return TBLX_EXIT_SCHEMA;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        tbl_pkgv_seterr(err, errsz, "cannot open file");
        return TBLX_EXIT_IO;
    }

    tbl_sha256_init(&s);
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        tbl_sha256_update(&s, buf, n);
    }
    if (ferror(fp)) {
        fclose(fp);
        tbl_pkgv_seterr(err, errsz, "read error");
        return TBLX_EXIT_IO;
    }
    fclose(fp);

    tbl_sha256_final(&s, digest);
    /* tbl_sha256_hex_ok() returns 1 on success, 0 on failure. */
    if (tbl_sha256_hex_ok(digest, out_hex, 65) == 0) {
        tbl_pkgv_seterr(err, errsz, "hex encode failed");
        return TBLX_EXIT_IO;
    }

    return TBLX_EXIT_OK;
}

static int tbl_pkgv_is_safe_relpath(const char *rel)
{
    /* Must be relative, use '/', no backslashes, no drive letters.
     * Must not contain '..' segments.
     */
    size_t i;

    if (!rel || !rel[0]) return 0;
    if (tbl_path_is_abs(rel)) return 0;

    for (i = 0; rel[i] != '\0'; ++i) {
        if (rel[i] == '\\') return 0;
    }

    /* reject "/../" or leading "../" or trailing "/.." */
    if (strncmp(rel, "../", 3) == 0) return 0;
    if (strcmp(rel, "..") == 0) return 0;
    if (strstr(rel, "/../") != NULL) return 0;
    {
        size_t n = strlen(rel);
        if (n >= 3 && strcmp(rel + (n - 3), "/..") == 0) return 0;
    }

    return 1;
}

typedef struct tbl_pkgv_pkgini_s {
    int saw_section;
    int saw_schema_version;
    int saw_kind;
    int saw_jobid;
    int saw_created_utc;
    int saw_tool_version;
    int saw_tool_commit;
    int saw_events_source;

    unsigned long schema_version;
    char kind[16];
    char jobid[128];
    char events_source[16];
} tbl_pkgv_pkgini_t;

static int tbl_pkgv_pkgini_cb(void *ud, const char *section, const char *key, const char *value, int line_no)
{
    tbl_pkgv_pkgini_t *p = (tbl_pkgv_pkgini_t*)ud;
    (void)line_no;

    /* NOTE: tbl_ini_parse_* expects callback return 0 on ACCEPT, non-zero on REJECT. */
    if (!p || !section || !key || !value) return 1;

    if (strcmp(section, "package") != 0) {
        return 1; /* unknown section => reject */
    }

    p->saw_section = 1;

    if (strcmp(key, "schema_version") == 0) {
        if (!tbl_parse_u32_ok(value, &p->schema_version)) return 1;
        p->saw_schema_version = 1;
        return 0;
    }

    if (strcmp(key, "kind") == 0) {
        if (tbl_strlcpy(p->kind, value, sizeof(p->kind)) >= sizeof(p->kind)) return 1;
        p->saw_kind = 1;
        return 0;
    }

    if (strcmp(key, "jobid") == 0) {
        if (tbl_strlcpy(p->jobid, value, sizeof(p->jobid)) >= sizeof(p->jobid)) return 1;
        p->saw_jobid = 1;
        return 0;
    }

    if (strcmp(key, "created_utc") == 0) {
        unsigned long tmp;
        if (!tbl_parse_u32_ok(value, &tmp)) return 1;
        p->saw_created_utc = 1;
        return 0;
    }


    if (strcmp(key, "events_source") == 0) {
        if (tbl_strlcpy(p->events_source, value, sizeof(p->events_source)) >= sizeof(p->events_source)) return 1;
        /* optional, but if present it must be job|legacy */
        if (!(strcmp(p->events_source, "job") == 0 || strcmp(p->events_source, "legacy") == 0)) return 1;
        p->saw_events_source = 1;
        return 0;
    }

    if (strcmp(key, "tool_version") == 0) {
        p->saw_tool_version = 1;
        return 0;
    }

    if (strcmp(key, "tool_commit") == 0) {
        p->saw_tool_commit = 1;
        return 0;
    }

    /* unknown key */
    return 1;
}

static int tbl_pkgv_parse_package_ini(const char *path, tbl_pkgv_pkgini_t *out, char *err, size_t errsz)
{
    char inierr[256];
    int rc;
    tbl_pkgv_pkgini_t tmp;

    if (!path || !out) {
        tbl_pkgv_seterr(err, errsz, "invalid args");
        return TBLX_EXIT_SCHEMA;
    }

    memset(&tmp, 0, sizeof(tmp));
    tmp.schema_version = 0UL;
    tmp.kind[0] = '\0';
    tmp.jobid[0] = '\0';
    tmp.events_source[0] = '\0';

    inierr[0] = '\0';
    rc = tbl_ini_parse_file(path, tbl_pkgv_pkgini_cb, &tmp, inierr, sizeof(inierr));
    if (rc == TBL_INI_EIO) {
        tbl_pkgv_seterr(err, errsz, "cannot read package.ini");
        return TBLX_EXIT_IO;
    }
    if (rc != TBL_INI_OK) {
        /* includes callback failure (unknown key/section) */
        if (inierr[0]) tbl_pkgv_seterr(err, errsz, inierr);
        else tbl_pkgv_seterr(err, errsz, "package.ini schema violation");
        return TBLX_EXIT_SCHEMA;
    }

    /* Required schema core (Reference v1):
     *   [package]
     *   schema_version, kind, jobid, created_utc, tool_version
     * Optional (but strict if present):
     *   tool_commit, events_source
     */
    if (!tmp.saw_section ||
        !tmp.saw_schema_version ||
        !tmp.saw_kind ||
        !tmp.saw_jobid ||
        !tmp.saw_created_utc ||
        !tmp.saw_tool_version) {
        tbl_pkgv_seterr(err, errsz, "package.ini missing required keys");
        return TBLX_EXIT_SCHEMA;
    }

    if (tmp.schema_version != 1UL) {
        tbl_pkgv_seterr(err, errsz, "unsupported schema_version");
        return TBLX_EXIT_SCHEMA;
    }

    /* kind must be aip|sip */
    if (!(strcmp(tmp.kind, "aip") == 0 || strcmp(tmp.kind, "sip") == 0)) {
        tbl_pkgv_seterr(err, errsz, "invalid kind");
        return TBLX_EXIT_SCHEMA;
    }

    *out = tmp;
    return TBLX_EXIT_OK;
}

static int tbl_pkgv_parse_manifest_line(const char *line, char out_hex[65], char out_rel[512])
{
    size_t i;
    size_t n;

    if (!line || !out_hex || !out_rel) return 0;

    /* format: 64 hex, two spaces, relpath */
    n = strlen(line);
    if (n < 64 + 2 + 1) return 0;

    for (i = 0; i < 64; ++i) {
        char c = line[i];
        int is_hex = ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
        if (!is_hex) return 0;
        out_hex[i] = c;
    }
    out_hex[64] = '\0';

    if (line[64] != ' ' || line[65] != ' ') return 0;

    /* trim trailing LF */
    {
        const char *rp = line + 66;
        size_t rpn = strlen(rp);
        while (rpn > 0 && (rp[rpn-1] == '\n')) rpn--;
        if (rpn == 0) return 0;
        if (rpn >= 512) return 0;
        memcpy(out_rel, rp, rpn);
        out_rel[rpn] = '\0';
    }

    return 1;
}

static int tbl_pkgv_read_line(FILE *fp, char *buf, size_t bufsz)
{
    if (!fp || !buf || bufsz == 0) return 0;
    if (!fgets(buf, (int)bufsz, fp)) return 0;
    return 1;
}

int tbl_verify_package_dir(const char *pkg_dir, char *err, size_t errsz)
{
    char meta_dir[1024];
    char rep_dir[1024];
    char rep_data_dir[1024];
    char path_record[1024];
    char path_package[1024];
    char path_events[1024];
    char path_manifest[1024];

    int ex = 0;
    int isdir = 0;

    tbl_pkgv_pkgini_t pkgini;
    tbl_record_t rec;

    /* expected manifest entries */
    const char *exp_rel[4];
    char exp_sha[4][65];
    char line[2048];
    int line_count;

    if (!pkg_dir || !pkg_dir[0]) {
        tbl_pkgv_seterr(err, errsz, "missing PKGDIR");
        return TBLX_EXIT_USAGE;
    }

    (void)tbl_fs_exists(pkg_dir, &ex);
    if (!ex) {
        tbl_pkgv_seterr(err, errsz, "package dir not found");
        return TBLX_EXIT_NOTFOUND;
    }
    (void)tbl_fs_is_dir(pkg_dir, &isdir);
    if (!isdir) {
        tbl_pkgv_seterr(err, errsz, "package path is not a directory");
        return TBLX_EXIT_SCHEMA;
    }

    if (!tbl_path_join2(meta_dir, sizeof(meta_dir), pkg_dir, "metadata")) {
        tbl_pkgv_seterr(err, errsz, "path too long");
        return TBLX_EXIT_SCHEMA;
    }
    if (!tbl_path_join2(rep_dir, sizeof(rep_dir), pkg_dir, "representations")) {
        tbl_pkgv_seterr(err, errsz, "path too long");
        return TBLX_EXIT_SCHEMA;
    }
    if (!tbl_path_join2(rep_data_dir, sizeof(rep_data_dir), rep_dir, "rep0/data")) {
        tbl_pkgv_seterr(err, errsz, "path too long");
        return TBLX_EXIT_SCHEMA;
    }

    /* strict: required directories exist */
    (void)tbl_fs_exists(meta_dir, &ex);
    if (!ex) { tbl_pkgv_seterr(err, errsz, "missing metadata dir"); return TBLX_EXIT_NOTFOUND; }
    (void)tbl_fs_is_dir(meta_dir, &isdir);
    if (!isdir) { tbl_pkgv_seterr(err, errsz, "metadata is not a dir"); return TBLX_EXIT_SCHEMA; }

    (void)tbl_fs_exists(rep_data_dir, &ex);
    if (!ex) { tbl_pkgv_seterr(err, errsz, "missing representations/rep0/data"); return TBLX_EXIT_NOTFOUND; }
    (void)tbl_fs_is_dir(rep_data_dir, &isdir);
    if (!isdir) { tbl_pkgv_seterr(err, errsz, "rep0/data is not a dir"); return TBLX_EXIT_SCHEMA; }

    /* required files */
    if (!tbl_path_join2(path_record, sizeof(path_record), meta_dir, "record.ini")) { tbl_pkgv_seterr(err, errsz, "path too long"); return TBLX_EXIT_SCHEMA; }
    if (!tbl_path_join2(path_package, sizeof(path_package), meta_dir, "package.ini")) { tbl_pkgv_seterr(err, errsz, "path too long"); return TBLX_EXIT_SCHEMA; }
    if (!tbl_path_join2(path_events, sizeof(path_events), meta_dir, "events.log")) { tbl_pkgv_seterr(err, errsz, "path too long"); return TBLX_EXIT_SCHEMA; }
    if (!tbl_path_join2(path_manifest, sizeof(path_manifest), meta_dir, "manifest-sha256.txt")) { tbl_pkgv_seterr(err, errsz, "path too long"); return TBLX_EXIT_SCHEMA; }

    (void)tbl_fs_exists(path_record, &ex); if (!ex) { tbl_pkgv_seterr(err, errsz, "missing metadata/record.ini"); return TBLX_EXIT_NOTFOUND; }
    (void)tbl_fs_exists(path_package, &ex); if (!ex) { tbl_pkgv_seterr(err, errsz, "missing metadata/package.ini"); return TBLX_EXIT_NOTFOUND; }
    (void)tbl_fs_exists(path_events, &ex); if (!ex) { tbl_pkgv_seterr(err, errsz, "missing metadata/events.log"); return TBLX_EXIT_NOTFOUND; }
    (void)tbl_fs_exists(path_manifest, &ex); if (!ex) { tbl_pkgv_seterr(err, errsz, "missing metadata/manifest-sha256.txt"); return TBLX_EXIT_NOTFOUND; }

    /* LF-only for metadata files */
    {
        int rc;
        rc = tbl_pkgv_has_cr(path_record, err, errsz); if (rc) return rc;
        rc = tbl_pkgv_has_cr(path_package, err, errsz); if (rc) return rc;
        rc = tbl_pkgv_has_cr(path_events, err, errsz); if (rc) return rc;
        rc = tbl_pkgv_has_cr(path_manifest, err, errsz); if (rc) return rc;
    }

    /* parse package.ini schema strictly */
    {
        int rc = tbl_pkgv_parse_package_ini(path_package, &pkgini, err, errsz);
        if (rc) return rc;
    }

    /* parse record.ini */
    memset(&rec, 0, sizeof(rec));
    {
        int rc = tbl_record_read_file(path_record, &rec, err, errsz);
        if (rc != 0) {
            if (err && err[0]) return TBLX_EXIT_SCHEMA;
            tbl_pkgv_seterr(err, errsz, "record.ini parse failed");
            return TBLX_EXIT_SCHEMA;
        }
    }

    /* jobid must match */
    if (rec.job[0] == '\0' || strcmp(rec.job, pkgini.jobid) != 0) {
        tbl_pkgv_seterr(err, errsz, "jobid mismatch");
        return TBLX_EXIT_INTEGRITY;
    }

    /* payload file name must match record.payload and exist */
    {
        char payload_path[1024];
        if (!rec.payload[0]) {
            tbl_pkgv_seterr(err, errsz, "record.payload missing");
            return TBLX_EXIT_SCHEMA;
        }
        if (strchr(rec.payload, '/') || strchr(rec.payload, '\\') || strstr(rec.payload, "..")) {
            tbl_pkgv_seterr(err, errsz, "unsafe payload name");
            return TBLX_EXIT_SCHEMA;
        }
        if (!tbl_path_join2(payload_path, sizeof(payload_path), rep_data_dir, rec.payload)) {
            tbl_pkgv_seterr(err, errsz, "payload path too long");
            return TBLX_EXIT_SCHEMA;
        }
        (void)tbl_fs_exists(payload_path, &ex);
        if (!ex) {
            tbl_pkgv_seterr(err, errsz, "payload not found");
            return TBLX_EXIT_NOTFOUND;
        }

        /* compute expected hashes */
        exp_rel[0] = "representations/rep0/data"; /* placeholder, replaced below */

        /* payload */
        if (tbl_pkgv_hash_file_hex(payload_path, exp_sha[0], err, errsz) != 0) return TBLX_EXIT_IO;
        /* must match record sha */
        if (strcmp(exp_sha[0], rec.sha256) != 0) {
            tbl_pkgv_seterr(err, errsz, "payload sha256 mismatch (vs record.ini)");
            return TBLX_EXIT_INTEGRITY;
        }

        /* other hashes */
        if (tbl_pkgv_hash_file_hex(path_record, exp_sha[1], err, errsz) != 0) return TBLX_EXIT_IO;
        if (tbl_pkgv_hash_file_hex(path_package, exp_sha[2], err, errsz) != 0) return TBLX_EXIT_IO;
        if (tbl_pkgv_hash_file_hex(path_events, exp_sha[3], err, errsz) != 0) return TBLX_EXIT_IO;

        exp_rel[0] = "representations/rep0/data"; /* not used */

        /* expected relpaths per spec */
        exp_rel[0] = "representations/rep0/data"; /* keep compiler quiet */
    }

    /* expected relpaths */
    {
        char tmp_payload_rel[512];
        (void)tbl_strlcpy(tmp_payload_rel, "representations/rep0/data/", sizeof(tmp_payload_rel));
        (void)tbl_strlcat(tmp_payload_rel, rec.payload, sizeof(tmp_payload_rel));

        exp_rel[0] = tmp_payload_rel; /* note: local var, so we copy below */

        /* We can't keep pointer to tmp on stack across blocks; copy into stable buffers. */
        {
            static char rel0[512];
            static char rel1[64];
            static char rel2[64];
            static char rel3[64];
            (void)tbl_strlcpy(rel0, tmp_payload_rel, sizeof(rel0));
            (void)tbl_strlcpy(rel1, "metadata/record.ini", sizeof(rel1));
            (void)tbl_strlcpy(rel2, "metadata/package.ini", sizeof(rel2));
            (void)tbl_strlcpy(rel3, "metadata/events.log", sizeof(rel3));
            exp_rel[0] = rel0;
            exp_rel[1] = rel1;
            exp_rel[2] = rel2;
            exp_rel[3] = rel3;
        }
    }

    /* parse manifest lines (strict: exactly 4 lines, deterministic order) */
    {
        FILE *fp = fopen(path_manifest, "rb");
        if (!fp) { tbl_pkgv_seterr(err, errsz, "cannot open manifest"); return TBLX_EXIT_IO; }

        line_count = 0;
        while (tbl_pkgv_read_line(fp, line, sizeof(line))) {
            char sha[65];
            char rel[512];

            if (line[0] == '\0') { fclose(fp); tbl_pkgv_seterr(err, errsz, "empty manifest line"); return TBLX_EXIT_INTEGRITY; }
            if (!tbl_pkgv_parse_manifest_line(line, sha, rel)) { fclose(fp); tbl_pkgv_seterr(err, errsz, "invalid manifest line"); return TBLX_EXIT_INTEGRITY; }
            if (!tbl_pkgv_is_safe_relpath(rel)) { fclose(fp); tbl_pkgv_seterr(err, errsz, "unsafe manifest path"); return TBLX_EXIT_INTEGRITY; }

            if (line_count >= 4) { fclose(fp); tbl_pkgv_seterr(err, errsz, "manifest has extra lines"); return TBLX_EXIT_INTEGRITY; }

            if (strcmp(rel, exp_rel[line_count]) != 0) { fclose(fp); tbl_pkgv_seterr(err, errsz, "manifest order/path mismatch"); return TBLX_EXIT_INTEGRITY; }
            if (strcmp(sha, exp_sha[line_count]) != 0) { fclose(fp); tbl_pkgv_seterr(err, errsz, "manifest hash mismatch"); return TBLX_EXIT_INTEGRITY; }

            line_count++;
        }

        if (ferror(fp)) { fclose(fp); tbl_pkgv_seterr(err, errsz, "manifest read error"); return TBLX_EXIT_IO; }
        fclose(fp);

        if (line_count != 4) { tbl_pkgv_seterr(err, errsz, "manifest line count mismatch"); return TBLX_EXIT_INTEGRITY; }
    }

    return TBLX_EXIT_OK;
}

int tbl_ingest_package_dir(const char *repo_root, const char *pkg_dir, char *err, size_t errsz)
{
    char meta_dir[1024];
    char rep_data_dir[1024];
    char path_record[1024];
    tbl_record_t rec;
    int rc;

    if (!repo_root || !repo_root[0]) {
        tbl_pkgv_seterr(err, errsz, "missing repo_root");
        return TBLX_EXIT_USAGE;
    }

    rc = tbl_verify_package_dir(pkg_dir, err, errsz);
    if (rc != 0) return rc;

    if (!tbl_path_join2(meta_dir, sizeof(meta_dir), pkg_dir, "metadata")) { tbl_pkgv_seterr(err, errsz, "path too long"); return TBLX_EXIT_SCHEMA; }
    if (!tbl_path_join2(rep_data_dir, sizeof(rep_data_dir), pkg_dir, "representations/rep0/data")) { tbl_pkgv_seterr(err, errsz, "path too long"); return TBLX_EXIT_SCHEMA; }
    if (!tbl_path_join2(path_record, sizeof(path_record), meta_dir, "record.ini")) { tbl_pkgv_seterr(err, errsz, "path too long"); return TBLX_EXIT_SCHEMA; }

    memset(&rec, 0, sizeof(rec));
    {
        int ex = 0;
        (void)tbl_fs_exists(path_record, &ex);
        if (!ex) {
            tbl_pkgv_seterr(err, errsz, "missing metadata/record.ini");
            return TBLX_EXIT_NOTFOUND;
        }
        if (tbl_record_read_file(path_record, &rec, err, errsz) != 0) {
            /* Preserve record parser's error message if it set one. */
            if (!err || !err[0]) {
                tbl_pkgv_seterr(err, errsz, "record.ini parse failed");
            }
            return TBLX_EXIT_SCHEMA;
        }
    }

    /* Put payload into CAS */
    {
        char payload_path[1024];
        char sha[65];
        if (!tbl_path_join2(payload_path, sizeof(payload_path), rep_data_dir, rec.payload)) {
            tbl_pkgv_seterr(err, errsz, "payload path too long");
            return TBLX_EXIT_SCHEMA;
        }

        sha[0] = '\0';
        if (tbl_cas_put_file(repo_root, payload_path, sha, sizeof(sha), err, errsz) != 0) {
            /* cas.h already sets err */
            return TBLX_EXIT_IO;
        }

        /* Ensure sha equals record sha */
        if (strcmp(sha, rec.sha256) != 0) {
            tbl_pkgv_seterr(err, errsz, "CAS sha mismatch (unexpected)");
            return TBLX_EXIT_INTEGRITY;
        }
    }

    /* Write record into repo */
    if (tbl_record_write_repo(repo_root, &rec, err, errsz) != 0) {
        return TBLX_EXIT_IO;
    }

    /* Append event */
    (void)tbl_events_append(repo_root, "ingest-package.ok", rec.job, rec.status, rec.sha256, "", 0, 0);

    return TBLX_EXIT_OK;
}

#endif /* TBL_PKGVERIFY_IMPLEMENTATION */

#endif /* TBL_CORE_PKGVERIFY_H */
