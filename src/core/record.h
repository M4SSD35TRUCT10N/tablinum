#ifndef TBL_CORE_RECORD_H
#define TBL_CORE_RECORD_H

#include <stddef.h>

/* Repo record (INI-ish key=value lines).
   Path: <repo_root>/records/<jobid>.ini

   This is the durable "truth" (AIP-light metadata) independent of spool/out retention.
*/

typedef struct tbl_record_s {
    char job[256];        /* job id (directory name) */
    char status[16];      /* "ok" or "fail" */
    char payload[64];     /* e.g. "payload.bin" */
    char sha256[65];      /* 64 hex + NUL */
    unsigned long bytes;  /* payload size (best effort) */
    unsigned long stored_at; /* unix epoch seconds (best effort) */
    char reason[256];     /* optional error reason */
} tbl_record_t;

/* Safe job id: no path separators, no "..", no control chars. */
int tbl_record_is_safe_id(const char *jobid);

/* Compute <repo_root>/records/<jobid>.ini (no filesystem access). Returns 1/0. */
int tbl_record_path(const char *repo_root, const char *jobid, char *out_path, size_t out_path_sz);

/* Write record to repo path, creating <repo_root>/records as needed. Returns 0 on success. */
int tbl_record_write_repo(const char *repo_root, const tbl_record_t *rec, char *err, size_t errsz);

/* Read record from repo path. Returns 0 on success. */
int tbl_record_read_repo(const char *repo_root, const char *jobid, tbl_record_t *out_rec, char *err, size_t errsz);

/* Read record from an explicit file path (used by package verify/ingest). Returns 0 on success. */
int tbl_record_read_file(const char *path, tbl_record_t *out_rec, char *err, size_t errsz);

#ifdef TBL_RECORD_IMPLEMENTATION

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "core/safe.h"
#include "core/str.h"
#include "core/path.h"
#include "os/fs.h"

static void tbl_record_seterr(char *err, size_t errsz, const char *msg)
{
    if (!err || errsz == 0) return;
    err[0] = '\0';
    if (!msg) msg = "record error";
    (void)tbl_strlcpy(err, msg, errsz);
}

int tbl_record_is_safe_id(const char *jobid)
{
    size_t i;

    if (!jobid || !jobid[0]) return 0;

    /* forbid ".." anywhere */
    if (strstr(jobid, "..") != 0) return 0;

    for (i = 0; jobid[i]; ++i) {
        unsigned char c = (unsigned char)jobid[i];

        if (c < 0x20) return 0;            /* control chars */
        if (c == '/' || c == '\\') return 0; /* path separators */
        if (c == ':') return 0;            /* Windows drive separator */
    }
    return 1;
}

int tbl_record_path(const char *repo_root, const char *jobid, char *out_path, size_t out_path_sz)
{
    char records_dir[1024];
    char fname[300];

    if (!repo_root || !repo_root[0] || !out_path || out_path_sz == 0) return 0;
    if (!tbl_record_is_safe_id(jobid)) return 0;

    if (!tbl_path_join2(records_dir, sizeof(records_dir), repo_root, "records")) return 0;

    fname[0] = '\0';
    if (tbl_strlcpy(fname, jobid, sizeof(fname)) >= sizeof(fname)) return 0;
    if (tbl_strlcat(fname, ".ini", sizeof(fname)) >= sizeof(fname)) return 0;

    if (!tbl_path_join2(out_path, out_path_sz, records_dir, fname)) return 0;
    return 1;
}

static int tbl_record_write_path(const char *path, const tbl_record_t *rec, char *err, size_t errsz)
{
    char buf[2048];
    char num[32];

    if (!path || !path[0] || !rec) {
        tbl_record_seterr(err, errsz, "invalid args");
        return 2;
    }

    buf[0] = '\0';

    if (tbl_strlcat(buf, "status=", sizeof(buf)) >= sizeof(buf) ||
        tbl_strlcat(buf, rec->status[0] ? rec->status : "unknown", sizeof(buf)) >= sizeof(buf) ||
        tbl_strlcat(buf, "\n", sizeof(buf)) >= sizeof(buf)) {
        tbl_record_seterr(err, errsz, "record buffer too small");
        return 2;
    }

    if (rec->job[0]) {
        if (tbl_strlcat(buf, "job=", sizeof(buf)) >= sizeof(buf) ||
            tbl_strlcat(buf, rec->job, sizeof(buf)) >= sizeof(buf) ||
            tbl_strlcat(buf, "\n", sizeof(buf)) >= sizeof(buf)) {
            tbl_record_seterr(err, errsz, "record buffer too small");
            return 2;
        }
    }

    if (rec->payload[0]) {
        if (tbl_strlcat(buf, "payload=", sizeof(buf)) >= sizeof(buf) ||
            tbl_strlcat(buf, rec->payload, sizeof(buf)) >= sizeof(buf) ||
            tbl_strlcat(buf, "\n", sizeof(buf)) >= sizeof(buf)) {
            tbl_record_seterr(err, errsz, "record buffer too small");
            return 2;
        }
    }

    if (rec->sha256[0]) {
        if (tbl_strlcat(buf, "sha256=", sizeof(buf)) >= sizeof(buf) ||
            tbl_strlcat(buf, rec->sha256, sizeof(buf)) >= sizeof(buf) ||
            tbl_strlcat(buf, "\n", sizeof(buf)) >= sizeof(buf)) {
            tbl_record_seterr(err, errsz, "record buffer too small");
            return 2;
        }
    }

    if (!tbl_u32_to_dec_ok((unsigned long)rec->bytes, num, sizeof(num))) {
        tbl_record_seterr(err, errsz, "bytes conv failed");
        return 2;
    }
    if (tbl_strlcat(buf, "bytes=", sizeof(buf)) >= sizeof(buf) ||
        tbl_strlcat(buf, num, sizeof(buf)) >= sizeof(buf) ||
        tbl_strlcat(buf, "\n", sizeof(buf)) >= sizeof(buf)) {
        tbl_record_seterr(err, errsz, "record buffer too small");
        return 2;
    }

    if (!tbl_u32_to_dec_ok((unsigned long)rec->stored_at, num, sizeof(num))) {
        tbl_record_seterr(err, errsz, "stored_at conv failed");
        return 2;
    }
    if (tbl_strlcat(buf, "stored_at=", sizeof(buf)) >= sizeof(buf) ||
        tbl_strlcat(buf, num, sizeof(buf)) >= sizeof(buf) ||
        tbl_strlcat(buf, "\n", sizeof(buf)) >= sizeof(buf)) {
        tbl_record_seterr(err, errsz, "record buffer too small");
        return 2;
    }

    if (rec->reason[0]) {
        if (tbl_strlcat(buf, "reason=", sizeof(buf)) >= sizeof(buf) ||
            tbl_strlcat(buf, rec->reason, sizeof(buf)) >= sizeof(buf) ||
            tbl_strlcat(buf, "\n", sizeof(buf)) >= sizeof(buf)) {
            tbl_record_seterr(err, errsz, "record buffer too small");
            return 2;
        }
    }

    if (tbl_fs_write_file(path, buf, (size_t)tbl_strlen(buf)) != 0) {
        tbl_record_seterr(err, errsz, "cannot write record file");
        return 2;
    }

    return 0;
}

int tbl_record_write_repo(const char *repo_root, const tbl_record_t *rec, char *err, size_t errsz)
{
    char records_dir[1024];
    char path[1024];

    if (err && errsz) err[0] = '\0';
    if (!repo_root || !repo_root[0] || !rec) {
        tbl_record_seterr(err, errsz, "invalid args");
        return 2;
    }
    if (!tbl_record_is_safe_id(rec->job)) {
        tbl_record_seterr(err, errsz, "unsafe job id");
        return 2;
    }

    if (!tbl_path_join2(records_dir, sizeof(records_dir), repo_root, "records")) {
        tbl_record_seterr(err, errsz, "records dir path too long");
        return 2;
    }
    if (tbl_fs_mkdir_p(records_dir) != 0) {
        tbl_record_seterr(err, errsz, "cannot create records dir");
        return 2;
    }

    if (!tbl_record_path(repo_root, rec->job, path, sizeof(path))) {
        tbl_record_seterr(err, errsz, "record path too long");
        return 2;
    }

    return tbl_record_write_path(path, rec, err, errsz);
}

static void tbl_record_init(tbl_record_t *r)
{
    if (!r) return;
    (void)memset(r, 0, sizeof(*r));
    r->bytes = 0UL;
    r->stored_at = 0UL;
}

static void tbl_record_trim(char *s)
{
    size_t n;
    if (!s) return;
    n = strlen(s);
    while (n > 0 && (s[n-1] == '\r' || s[n-1] == '\n' || s[n-1] == ' ' || s[n-1] == '\t')) {
        s[n-1] = '\0';
        n--;
    }
}

static int tbl_record_parse_ul(const char *s, unsigned long *out)
{
    char *end = 0;
    unsigned long v;

    if (!s || !out) return 0;
    if (!s[0]) return 0;

    v = strtoul(s, &end, 10);
    if (!end || *end != '\0') return 0;
    *out = v;
    return 1;
}

int tbl_record_read_repo(const char *repo_root, const char *jobid, tbl_record_t *out_rec, char *err, size_t errsz)
{
    char path[1024];
    FILE *fp;
    char line[512];

    if (err && errsz) err[0] = '\0';
    if (!repo_root || !repo_root[0] || !out_rec) {
        tbl_record_seterr(err, errsz, "invalid args");
        return 2;
    }
    if (!tbl_record_is_safe_id(jobid)) {
        tbl_record_seterr(err, errsz, "unsafe job id");
        return 2;
    }
    if (!tbl_record_path(repo_root, jobid, path, sizeof(path))) {
        tbl_record_seterr(err, errsz, "record path too long");
        return 2;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        tbl_record_seterr(err, errsz, "cannot open record");
        return 2;
    }

    tbl_record_init(out_rec);
    (void)tbl_strlcpy(out_rec->job, jobid, sizeof(out_rec->job));

    while (fgets(line, (int)sizeof(line), fp)) {
        char *eq;
        char *key;
        char *val;

        tbl_record_trim(line);
        if (line[0] == '\0') continue;
        if (line[0] == '#') continue;

        eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        key = line;
        val = eq + 1;

        if (strcmp(key, "status") == 0) {
            (void)tbl_strlcpy(out_rec->status, val, sizeof(out_rec->status));
        } else if (strcmp(key, "job") == 0) {
            (void)tbl_strlcpy(out_rec->job, val, sizeof(out_rec->job));
        } else if (strcmp(key, "payload") == 0) {
            (void)tbl_strlcpy(out_rec->payload, val, sizeof(out_rec->payload));
        } else if (strcmp(key, "sha256") == 0) {
            (void)tbl_strlcpy(out_rec->sha256, val, sizeof(out_rec->sha256));
        } else if (strcmp(key, "bytes") == 0) {
            (void)tbl_record_parse_ul(val, &out_rec->bytes);
        } else if (strcmp(key, "stored_at") == 0) {
            (void)tbl_record_parse_ul(val, &out_rec->stored_at);
        } else if (strcmp(key, "reason") == 0) {
            (void)tbl_strlcpy(out_rec->reason, val, sizeof(out_rec->reason));
        }
    }

    fclose(fp);

    /* Minimal validation */
    if (out_rec->status[0] == '\0') (void)tbl_strlcpy(out_rec->status, "unknown", sizeof(out_rec->status));
    return 0;
}

int tbl_record_read_file(const char *path, tbl_record_t *out_rec, char *err, size_t errsz)
{
    FILE *fp;
    char line[512];

    if (err && errsz) err[0] = '\0';
    if (!path || !path[0] || !out_rec) {
        tbl_record_seterr(err, errsz, "invalid args");
        return 2;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        tbl_record_seterr(err, errsz, "cannot open record");
        return 2;
    }

    tbl_record_init(out_rec);

    while (fgets(line, (int)sizeof(line), fp)) {
        char *eq;
        char *key;
        char *val;

        tbl_record_trim(line);
        if (line[0] == '\0') continue;
        if (line[0] == '#') continue;

        eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        key = line;
        val = eq + 1;

        if (strcmp(key, "status") == 0) {
            (void)tbl_strlcpy(out_rec->status, val, sizeof(out_rec->status));
        } else if (strcmp(key, "job") == 0) {
            (void)tbl_strlcpy(out_rec->job, val, sizeof(out_rec->job));
        } else if (strcmp(key, "payload") == 0) {
            (void)tbl_strlcpy(out_rec->payload, val, sizeof(out_rec->payload));
        } else if (strcmp(key, "sha256") == 0) {
            (void)tbl_strlcpy(out_rec->sha256, val, sizeof(out_rec->sha256));
        } else if (strcmp(key, "bytes") == 0) {
            (void)tbl_record_parse_ul(val, &out_rec->bytes);
        } else if (strcmp(key, "stored_at") == 0) {
            (void)tbl_record_parse_ul(val, &out_rec->stored_at);
        } else if (strcmp(key, "reason") == 0) {
            (void)tbl_strlcpy(out_rec->reason, val, sizeof(out_rec->reason));
        }
    }

    fclose(fp);

    if (out_rec->status[0] == '\0') (void)tbl_strlcpy(out_rec->status, "unknown", sizeof(out_rec->status));
    return 0;
}

#endif /* TBL_RECORD_IMPLEMENTATION */

#endif /* TBL_CORE_RECORD_H */
