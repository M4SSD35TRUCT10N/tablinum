#ifndef TBL_CORE_INGEST_H
#define TBL_CORE_INGEST_H

#include <stddef.h>
#include "core/config.h"

/* Ingest role (jobdir + CAS, SIP-light -> AIP-light):
   - claims next DIRECTORY from spool/inbox -> spool/claim
   - requires: <jobdir>/payload.bin
   - stores payload in repo CAS (sha256)
   - writes <jobdir>/job.meta (moves with directory)
   - commits jobdir to spool/out or spool/fail

   jobs_done counts ok + fail.
*/

int tbl_ingest_run_ex(const tbl_cfg_t *cfg,
                      unsigned long *out_jobs_done,
                      char *err, size_t errsz);

/* Convenience wrapper (ignore jobs_done). */
int tbl_ingest_run(const tbl_cfg_t *cfg, char *err, size_t errsz);

#ifdef TBL_INGEST_IMPLEMENTATION

#include <string.h>

#include "core/safe.h"
#include "core/str.h"
#include "core/path.h"
#include "core/spool.h"
#include "core/cas.h"
#include "os/fs.h"
#include "os/time.h"

static void tbl_ingest_seterr(char *err, size_t errsz, const char *msg)
{
    if (!err || errsz == 0) return;
    err[0] = '\0';
    if (!msg) msg = "ingest error";
    (void)tbl_strlcpy(err, msg, errsz);
}

static int tbl_ingest_resolve_root(char *out, size_t outsz, const char *root, const char *path_rel_or_abs)
{
    if (!out || outsz == 0 || !root || !root[0] || !path_rel_or_abs || !path_rel_or_abs[0]) return 0;
    out[0] = '\0';
    if (tbl_path_is_abs(path_rel_or_abs)) {
        if (tbl_strlcpy(out, path_rel_or_abs, outsz) >= outsz) return 0;
        return 1;
    }
    return tbl_path_join2(out, outsz, root, path_rel_or_abs);
}

static int tbl_ingest_write_job_meta(const char *jobdir,
                                    const char *status,
                                    const char *jobid,
                                    const char *payload_name,
                                    const char *sha256hex_or_empty,
                                    const char *reason_or_empty,
                                   char *err, size_t errsz)
{
    char meta_path[1024];
    char buf[1536];

    if (!jobdir || !jobdir[0]) {
        tbl_ingest_seterr(err, errsz, "invalid jobdir");
        return 2;
    }

    if (!tbl_path_join2(meta_path, sizeof(meta_path), jobdir, "job.meta")) {
        tbl_ingest_seterr(err, errsz, "meta path too long");
        return 2;
    }

    buf[0] = '\0';

    if (tbl_strlcat(buf, "status=", sizeof(buf)) >= sizeof(buf) ||
        tbl_strlcat(buf, status ? status : "unknown", sizeof(buf)) >= sizeof(buf) ||
        tbl_strlcat(buf, "\n", sizeof(buf)) >= sizeof(buf)) {
        tbl_ingest_seterr(err, errsz, "meta buffer too small");
        return 2;
    }

    if (jobid && jobid[0]) {
        if (tbl_strlcat(buf, "job=", sizeof(buf)) >= sizeof(buf) ||
            tbl_strlcat(buf, jobid, sizeof(buf)) >= sizeof(buf) ||
            tbl_strlcat(buf, "\n", sizeof(buf)) >= sizeof(buf)) {
            tbl_ingest_seterr(err, errsz, "meta buffer too small");
            return 2;
        }
    }

    if (payload_name && payload_name[0]) {
        if (tbl_strlcat(buf, "payload=", sizeof(buf)) >= sizeof(buf) ||
            tbl_strlcat(buf, payload_name, sizeof(buf)) >= sizeof(buf) ||
            tbl_strlcat(buf, "\n", sizeof(buf)) >= sizeof(buf)) {
            tbl_ingest_seterr(err, errsz, "meta buffer too small");
            return 2;
        }
    }

    if (sha256hex_or_empty && sha256hex_or_empty[0]) {
        if (tbl_strlcat(buf, "sha256=", sizeof(buf)) >= sizeof(buf) ||
            tbl_strlcat(buf, sha256hex_or_empty, sizeof(buf)) >= sizeof(buf) ||
            tbl_strlcat(buf, "\n", sizeof(buf)) >= sizeof(buf)) {
            tbl_ingest_seterr(err, errsz, "meta buffer too small");
            return 2;
        }
    }

    if (reason_or_empty && reason_or_empty[0]) {
        if (tbl_strlcat(buf, "reason=", sizeof(buf)) >= sizeof(buf) ||
            tbl_strlcat(buf, reason_or_empty, sizeof(buf)) >= sizeof(buf) ||
            tbl_strlcat(buf, "\n", sizeof(buf)) >= sizeof(buf)) {
            tbl_ingest_seterr(err, errsz, "meta buffer too small");
            return 2;
        }
    }

    if (tbl_fs_write_file(meta_path, buf, (size_t)tbl_strlen(buf)) != 0) {
        tbl_ingest_seterr(err, errsz, "cannot write job.meta");
        return 2;
    }

    return 0;
}

static int tbl_ingest_commit_fail(tbl_spool_t *sp, const char *jobid, char *err, size_t errsz)
{
    int rc;
    rc = tbl_spool_commit_fail(sp, jobid, err, errsz);
    if (rc != TBL_SPOOL_OK) {
        if (err && errsz && err[0] == '\0') tbl_ingest_seterr(err, errsz, "commit_fail failed");
        return 2;
    }
    return 0;
}

int tbl_ingest_run_ex(const tbl_cfg_t *cfg,
                      unsigned long *out_jobs_done,
                      char *err, size_t errsz)
{
    tbl_spool_t sp;
    char spool_root[1024];
    char repo_root[1024];
    char name[256];
    char jobdir[1024];
    char payload[1024];
    unsigned long poll_ms;
    unsigned long jobs_done;
    int once;
    unsigned long max_jobs;

    if (err && errsz) err[0] = '\0';
    if (out_jobs_done) *out_jobs_done = 0UL;

    if (!cfg) {
        tbl_ingest_seterr(err, errsz, "cfg is NULL");
        return 2;
    }

    jobs_done = 0UL;
    once = (cfg->ingest_once != 0UL) ? 1 : 0;
    max_jobs = cfg->ingest_max_jobs;

    if (!tbl_ingest_resolve_root(spool_root, sizeof(spool_root), cfg->root, cfg->spool)) {
        tbl_ingest_seterr(err, errsz, "spool path resolve failed");
        return 2;
    }
    if (!tbl_ingest_resolve_root(repo_root, sizeof(repo_root), cfg->root, cfg->repo)) {
        tbl_ingest_seterr(err, errsz, "repo path resolve failed");
        return 2;
    }

    if (tbl_spool_init(&sp, spool_root, err, errsz) != TBL_SPOOL_OK) {
        if (err && errsz && err[0] == '\0') tbl_ingest_seterr(err, errsz, "spool init failed");
        return 2;
    }

    /* poll interval (seconds -> ms), clamp to avoid overflow */
    if (cfg->ingest_poll_seconds == 0UL) {
        poll_ms = 2000UL;
    } else if (cfg->ingest_poll_seconds > (0xFFFFFFFFUL / 1000UL)) {
        poll_ms = 0xFFFFFFFFUL;
    } else {
        poll_ms = cfg->ingest_poll_seconds * 1000UL;
    }

    for (;;) {
        int rc;
        int ex;
        char sha[65];

        if (err && errsz) err[0] = '\0';
        name[0] = '\0';

        /* IMPORTANT: claim DIRECTORY jobs (jobid is directory name) */
        rc = tbl_spool_claim_next_dir(&sp, name, sizeof(name), err, errsz);
        if (rc == TBL_SPOOL_ENOJOB) {
            if (once) break;
            tbl_sleep_ms(poll_ms);
            continue;
        }
        if (rc != TBL_SPOOL_OK) {
            if (err && errsz && err[0] == '\0') tbl_ingest_seterr(err, errsz, "claim failed");
            return 2;
        }

        /* jobdir = <sp.claim>/<jobid> */
        if (!tbl_path_join2(jobdir, sizeof(jobdir), sp.claim, name)) {
            tbl_ingest_seterr(err, errsz, "jobdir path too long");
            return 2;
        }

        /* payload = <jobdir>/payload.bin */
        if (!tbl_path_join2(payload, sizeof(payload), jobdir, "payload.bin")) {
            tbl_ingest_seterr(err, errsz, "payload path too long");
            return 2;
        }

        ex = 0;
        (void)tbl_fs_exists(payload, &ex);
        if (!ex) {
            /* missing payload -> fail */
            (void)tbl_ingest_write_job_meta(jobdir, "fail", name, "payload.bin", "", "missing payload.bin", err, errsz);
            if (tbl_ingest_commit_fail(&sp, name, err, errsz) != 0) return 2;

            jobs_done++;
            if (max_jobs > 0UL && jobs_done >= max_jobs) break;
            continue;
        }

        sha[0] = '\0';
        if (tbl_cas_put_file(repo_root, payload, sha, sizeof(sha), err, errsz) != 0) {
            (void)tbl_ingest_write_job_meta(jobdir, "fail", name, "payload.bin", "", err && err[0] ? err : "cas put failed", err, errsz);
            if (tbl_ingest_commit_fail(&sp, name, err, errsz) != 0) return 2;

            jobs_done++;
            if (max_jobs > 0UL && jobs_done >= max_jobs) break;
            continue;
        }

        if (tbl_ingest_write_job_meta(jobdir, "ok", name, "payload.bin", sha, "", err, errsz) != 0) {
            /* try to move to fail to avoid clogging claim */
            (void)tbl_ingest_commit_fail(&sp, name, err, errsz);
            return 2;
        }

        rc = tbl_spool_commit_out(&sp, name, err, errsz);
        if (rc != TBL_SPOOL_OK) {
            if (err && errsz && err[0] == '\0') tbl_ingest_seterr(err, errsz, "commit_out failed");
            return 2;
        }

        jobs_done++;
        if (max_jobs > 0UL && jobs_done >= max_jobs) break;
    }

    if (out_jobs_done) *out_jobs_done = jobs_done;
    return 0;
}

int tbl_ingest_run(const tbl_cfg_t *cfg, char *err, size_t errsz)
{
    return tbl_ingest_run_ex(cfg, 0, err, errsz);
}

#endif /* TBL_INGEST_IMPLEMENTATION */

#endif /* TBL_CORE_INGEST_H */
