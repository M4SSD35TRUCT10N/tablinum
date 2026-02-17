#ifndef TBL_CORE_EVENTS_H
#define TBL_CORE_EVENTS_H

#include <stddef.h>

/* Append-only audit log:
   Path: <repo_root>/events.log

   Lines are "key=value" pairs separated by spaces (no quoting).
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

static void tbl_events_seterr(char *err, size_t errsz, const char *msg)
{
    if (!err || errsz == 0) return;
    err[0] = '\0';
    if (!msg) msg = "events error";
    (void)tbl_strlcpy(err, msg, errsz);
}

static void tbl_events_sanitize(char *dst, size_t dstsz, const char *src)
{
    size_t i, j;
    if (!dst || dstsz == 0) return;
    dst[0] = '\0';
    if (!src) return;

    j = 0;
    for (i = 0; src[i] && j + 1 < dstsz; ++i) {
        unsigned char c = (unsigned char)src[i];
        if (c <= 0x20 || c == '=') c = '_'; /* spaces, control, '=' -> '_' */
        dst[j++] = (char)c;
    }
    dst[j] = '\0';
}

int tbl_events_append(const char *repo_root,
                      const char *event,
                      const char *jobid,
                      const char *status,
                      const char *sha256_or_empty,
                      const char *reason_or_empty,
                      char *err, size_t errsz)
{
    char path[1024];
    FILE *fp;
    unsigned long ts;
    char ev[64], jb[256], st[32], sh[80], rs[256];

    if (err && errsz) err[0] = '\0';
    if (!repo_root || !repo_root[0] || !event || !event[0]) {
        tbl_events_seterr(err, errsz, "invalid args");
        return 2;
    }

    if (!tbl_path_join2(path, sizeof(path), repo_root, "events.log")) {
        tbl_events_seterr(err, errsz, "events path too long");
        return 2;
    }

    fp = fopen(path, "ab");
    if (!fp) {
        tbl_events_seterr(err, errsz, "cannot open events.log");
        return 2;
    }

    ts = (unsigned long)time(0);

    tbl_events_sanitize(ev, sizeof(ev), event);
    tbl_events_sanitize(jb, sizeof(jb), jobid ? jobid : "");
    tbl_events_sanitize(st, sizeof(st), status ? status : "");
    tbl_events_sanitize(sh, sizeof(sh), sha256_or_empty ? sha256_or_empty : "");
    tbl_events_sanitize(rs, sizeof(rs), reason_or_empty ? reason_or_empty : "");

    /* key=value pairs (space separated) */
    (void)fprintf(fp, "ts=%lu event=%s", ts, ev);
    if (jb[0]) (void)fprintf(fp, " job=%s", jb);
    if (st[0]) (void)fprintf(fp, " status=%s", st);
    if (sh[0]) (void)fprintf(fp, " sha256=%s", sh);
    if (rs[0]) (void)fprintf(fp, " reason=%s", rs);
    (void)fprintf(fp, "\n");

    fclose(fp);
    return 0;
}

#endif /* TBL_EVENTS_IMPLEMENTATION */

#endif /* TBL_CORE_EVENTS_H */
