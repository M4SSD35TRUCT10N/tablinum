#ifndef TBL_CORE_EVENTS_H
#define TBL_CORE_EVENTS_H

#include <stddef.h>
#include <time.h>

/* Events (Reference v1)
   - Legacy combined stream: <repo_root>/events.log
   - Exportable job stream:  <repo_root>/jobs/<jobid>/events.log
   - Ops audit (hash-chain): <repo_root>/audit/ops.log

   Lines are key=value pairs separated by spaces (no quoting).
   NOTE: This remains best-effort; primary operations must not fail solely
   because logging failed.
*/

int tbl_events_append(const char *repo_root,
                      const char *event,
                      const char *jobid,
                      const char *status,
                      const char *sha256_or_empty,
                      const char *reason_or_empty,
                      char *err, size_t errsz);

#ifdef TBL_EVENTS_IMPLEMENTATION

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "core/safe.h"
#include "core/str.h"
#include "core/path.h"
#include "core/sha256.h"
#include "os/fs.h"

static void tbl_events_seterr(char *err, size_t errsz, const char *msg)
{
    if (!err || errsz == 0) return;
    err[0] = '\0';
    if (!msg) msg = "events error";
    (void)tbl_strlcpy(err, msg, errsz);
}

static int tbl_events_is_hex64(const char *s)
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

static void tbl_events_zero64(char out64[65])
{
    int i;
    for (i = 0; i < 64; ++i) out64[i] = '0';
    out64[64] = '\0';
}

static int tbl_events_read_last_hash(const char *path, char out64[65])
{
    FILE *fp;
    long endpos;
    long startpos;
    unsigned char buf[4096];
    size_t n;
    int i;
    int end_i;
    int line_start;
    const char *p;

    tbl_events_zero64(out64);

    fp = fopen(path, "rb");
    if (!fp) return 0;

    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return 0; }
    endpos = ftell(fp);
    if (endpos <= 0) { fclose(fp); return 0; }

    startpos = endpos - (long)sizeof(buf);
    if (startpos < 0) startpos = 0;
    if (fseek(fp, startpos, SEEK_SET) != 0) { fclose(fp); return 0; }

    n = fread(buf, 1, sizeof(buf), fp);
    fclose(fp);
    if (n == 0) return 0;

    /* find end of last non-empty line */
    end_i = (int)n - 1;
    while (end_i >= 0 && (buf[end_i] == '\n' || buf[end_i] == '\r')) end_i--;

    /* find start of that line */
    line_start = 0;
    i = end_i;
    while (i >= 0) {
        if (buf[i] == '\n') { line_start = i + 1; break; }
        i--;
    }

    /* locate "hash=" in last line */
    p = (const char *)(buf + line_start);
    {
        const char *h = strstr(p, "hash=");
        if (h) {
            char tmp[65];
            int k;
            h += 5; /* after "hash=" */
            for (k = 0; k < 64; ++k) {
                char c = h[k];
                if (!c) break;
                tmp[k] = c;
            }
            tmp[64] = '\0';
            if (tbl_events_is_hex64(tmp)) {
                (void)tbl_strlcpy(out64, tmp, 65);
            }
        }
    }

    return 0;
}

static int tbl_events_append_line(const char *path, const char *line_with_nl)
{
    FILE *fp;
    fp = fopen(path, "ab");
    if (!fp) return 2;
    if (fputs(line_with_nl, fp) == EOF) { fclose(fp); return 2; }
    if (fclose(fp) != 0) return 2;
    return 0;
}

static void tbl_events_build_canonical(char out[2048],
                                       unsigned long ts,
                                       const char *ev,
                                       const char *jb,
                                       const char *st,
                                       const char *sh,
                                       const char *rs)
{
    char nbuf[32];

    out[0] = '\0';

    if (!tbl_ul_to_dec_ok(ts, nbuf, sizeof(nbuf))) {
        (void)tbl_strlcpy(out, "ts=0 event=", 2048);
    } else {
        (void)tbl_strlcpy(out, "ts=", 2048);
        (void)tbl_strlcat(out, nbuf, 2048);
        (void)tbl_strlcat(out, " event=", 2048);
    }

    (void)tbl_strlcat(out, ev ? ev : "", 2048);

    if (jb && jb[0]) { (void)tbl_strlcat(out, " job=", 2048); (void)tbl_strlcat(out, jb, 2048); }
    if (st && st[0]) { (void)tbl_strlcat(out, " status=", 2048); (void)tbl_strlcat(out, st, 2048); }
    if (sh && sh[0]) { (void)tbl_strlcat(out, " sha256=", 2048); (void)tbl_strlcat(out, sh, 2048); }
    if (rs && rs[0]) { (void)tbl_strlcat(out, " reason=", 2048); (void)tbl_strlcat(out, rs, 2048); }
}

static void tbl_events_hash_chain(const char prev64[65], const char *canonical, char out64[65])
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

int tbl_events_append(const char *repo_root,
                      const char *event,
                      const char *jobid,
                      const char *status,
                      const char *sha256_or_empty,
                      const char *reason_or_empty,
                      char *err, size_t errsz)
{
    char ev[128];
    char jb[128];
    char st[64];
    char sh[80];
    char rs[512];

    char legacy_path[1024];
    char jobs_dir[1024];
    char job_dir[1024];
    char job_path[1024];
    char audit_dir[1024];
    char audit_path[1024];

    unsigned long ts;
    char canonical[2048];
    char line[2100];

    int rc;

    if (!repo_root || !repo_root[0] || !event || !event[0]) {
        tbl_events_seterr(err, errsz, "invalid args");
        return 2;
    }

    ev[0] = '\0';
    jb[0] = '\0';
    st[0] = '\0';
    sh[0] = '\0';
    rs[0] = '\0';

    (void)tbl_strlcpy(ev, event, sizeof(ev));
    if (jobid) (void)tbl_strlcpy(jb, jobid, sizeof(jb));
    if (status) (void)tbl_strlcpy(st, status, sizeof(st));
    if (sha256_or_empty) (void)tbl_strlcpy(sh, sha256_or_empty, sizeof(sh));
    if (reason_or_empty) (void)tbl_strlcpy(rs, reason_or_empty, sizeof(rs));

    ts = (unsigned long)time(0);
    tbl_events_build_canonical(canonical, ts, ev, jb, st, sh, rs);

    /* legacy: <repo_root>/events.log (kept for compatibility) */
    if (!tbl_path_join2(legacy_path, sizeof(legacy_path), repo_root, "events.log")) {
        tbl_events_seterr(err, errsz, "events path too long");
        return 2;
    }
    (void)tbl_strlcpy(line, canonical, sizeof(line));
    (void)tbl_strlcat(line, "\n", sizeof(line));
    rc = tbl_events_append_line(legacy_path, line);
    if (rc != 0) {
        tbl_events_seterr(err, errsz, "cannot append legacy events");
        return 2;
    }

    /* exportable job stream: <repo_root>/jobs/<jobid>/events.log (best effort) */
    if (jb[0]) {
        if (tbl_path_join2(jobs_dir, sizeof(jobs_dir), repo_root, "jobs") &&
            tbl_path_join2(job_dir, sizeof(job_dir), jobs_dir, jb) &&
            tbl_path_join2(job_path, sizeof(job_path), job_dir, "events.log")) {
            (void)tbl_fs_mkdir_p(job_dir);
            (void)tbl_events_append_line(job_path, line);
        }
    }

    /* ops audit: <repo_root>/audit/ops.log (hash-chained, best effort) */
    if (tbl_path_join2(audit_dir, sizeof(audit_dir), repo_root, "audit") &&
        tbl_path_join2(audit_path, sizeof(audit_path), audit_dir, "ops.log")) {
        char prev[65];
        char cur[65];
        char audit_line[2200];

        (void)tbl_fs_mkdir_p(audit_dir);

        (void)tbl_events_read_last_hash(audit_path, prev);
        tbl_events_hash_chain(prev, canonical, cur);

        audit_line[0] = '\0';
        (void)tbl_strlcpy(audit_line, "prev=", sizeof(audit_line));
        (void)tbl_strlcat(audit_line, prev, sizeof(audit_line));
        (void)tbl_strlcat(audit_line, " hash=", sizeof(audit_line));
        (void)tbl_strlcat(audit_line, cur, sizeof(audit_line));
        (void)tbl_strlcat(audit_line, " ", sizeof(audit_line));
        (void)tbl_strlcat(audit_line, canonical, sizeof(audit_line));
        (void)tbl_strlcat(audit_line, "\n", sizeof(audit_line));

        (void)tbl_events_append_line(audit_path, audit_line);
    }

    return 0;
}


#endif /* TBL_EVENTS_IMPLEMENTATION */

#endif /* TBL_CORE_EVENTS_H */
