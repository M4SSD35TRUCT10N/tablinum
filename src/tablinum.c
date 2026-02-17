/* src/tablinum.c - Tablinum entrypoint (strict C89, fail-fast, tack-typisch) */
#include "tablinum.h"

#include "core/args.h"
#include "core/config.h"
#include "core/export.h"
#include "core/ingest.h"
#include "core/log.h"
#include "core/path.h"
#include "core/safe.h"
#include "core/spool.h"
#include "core/str.h"
#include "core/verify.h"
#include "os/time.h"
#include "os/fs.h"

static int resolve_repo_root(char *out, size_t outsz, const tbl_cfg_t *cfg)
{
    if (!out || outsz == 0 || !cfg) return 0;
    out[0] = '\0';

    if (tbl_path_is_abs(cfg->repo)) {
        return (tbl_strlcpy(out, cfg->repo, outsz) < outsz) ? 1 : 0;
    }
    return tbl_path_join2(out, outsz, cfg->root, cfg->repo);
}

static int run_all(const tbl_app_config_t *app, const tbl_cfg_t *cfg)
{
    (void)app;
    (void)cfg;

    tbl_logf(TBL_LOG_INFO, "[all] TODO");
    return 0;
}

static int run_serve(const tbl_app_config_t *app, const tbl_cfg_t *cfg)
{
    (void)app;
    (void)cfg;

    tbl_logf(TBL_LOG_INFO, "[serve] TODO (listen=%s)", cfg->http_listen);
    return 0;
}

static int run_ingest(const tbl_app_config_t *app, const tbl_cfg_t *cfg)
{
    char err[256];
    unsigned long jobs_done;

    (void)app;

    err[0] = '\0';
    jobs_done = 0UL;

    tbl_logf(TBL_LOG_INFO, "[ingest] running (spool=%s, poll=%lu s, once=%lu, max_jobs=%lu)",
             cfg->spool, cfg->ingest_poll_seconds, cfg->ingest_once, cfg->ingest_max_jobs);

    if (tbl_ingest_run_ex(cfg, &jobs_done, err, sizeof(err)) != 0) {
        tbl_logf(TBL_LOG_ERROR, "%s", err[0] ? err : "ingest failed");
        return 2;
    }

    tbl_logf(TBL_LOG_INFO, "[ingest] done (%lu job(s))", jobs_done);
    return 0;
}

static int run_verify(const tbl_app_config_t *app, const tbl_cfg_t *cfg)
{
    char err[256];
    char repo_root[1024];
        int rc;

    if (!app || !app->jobid || !app->jobid[0]) {
        tbl_logf(TBL_LOG_ERROR, "verify requires JOBID");
        return 2;
    }

    if (!resolve_repo_root(repo_root, sizeof(repo_root), cfg)) {
        tbl_logf(TBL_LOG_ERROR, "repo path too long");
        return 2;
    }

    err[0] = '\0';
    rc = tbl_verify_job(repo_root, app->jobid, err, sizeof(err));
    if (rc == 0) {
        tbl_logf(TBL_LOG_INFO, "[verify] OK (%s)", app->jobid);
        return 0;
    }

    tbl_logf(TBL_LOG_ERROR, "[verify] FAIL (%s): %s", app->jobid, err[0] ? err : "verify failed");
    return (rc == 1) ? 1 : 2;
}

static int run_export(const tbl_app_config_t *app, const tbl_cfg_t *cfg)
{
    char err[256];
    char repo_root[1024];
    int rc;

    if (!app || !app->jobid || !app->jobid[0] || !app->out_dir || !app->out_dir[0]) {
        tbl_logf(TBL_LOG_ERROR, "export requires JOBID and OUTDIR");
        return 2;
    }

    if (!resolve_repo_root(repo_root, sizeof(repo_root), cfg)) {
        tbl_logf(TBL_LOG_ERROR, "repo path too long");
        return 2;
    }

    err[0] = '\0';
    rc = tbl_export_job(repo_root, app->jobid, app->out_dir, err, sizeof(err));
    if (rc != 0) {
        tbl_logf(TBL_LOG_ERROR, "[export] FAIL (%s): %s", app->jobid, err[0] ? err : "export failed");
        return 2;
    }

    tbl_logf(TBL_LOG_INFO, "[export] OK (%s -> %s)", app->jobid, app->out_dir);
    return 0;
}

static int run_index(const tbl_app_config_t *app, const tbl_cfg_t *cfg)
{
    (void)app;
    (void)cfg;

    tbl_logf(TBL_LOG_INFO, "[index] TODO (db=%s)", cfg->db);
    return 0;
}

static int run_worker(const tbl_app_config_t *app, const tbl_cfg_t *cfg)
{
    (void)app;
    (void)cfg;

    tbl_logf(TBL_LOG_INFO, "[worker] TODO");
    return 0;
}

int main(int argc, char **argv)
{
    int rc;
    tbl_app_config_t app;
    tbl_cfg_t cfg;
    char err[256];
    tbl_log_level_t lvl;

    lvl = TBL_LOG_INFO;
#if defined(TBL_DEBUG) && (TBL_DEBUG)
    lvl = TBL_LOG_DEBUG;
#endif
    tbl_log_set_level(lvl);

    rc = tbl_args_parse(argc, argv, &app);
    if (rc == 1) {
        /* --help / --version already handled */
        return 0;
    }
    if (rc != 0) {
        /* strict: args parser already printed an error */
        return rc;
    }

    /* load INI config (fail-fast on invalid config) */
    err[0] = '\0';
    rc = tbl_cfg_load(&cfg, app.config_path, err, sizeof(err));
    if (rc != 0) {
        if (err[0] != '\0') {
            tbl_logf(TBL_LOG_ERROR, "%s", err);
        } else {
            tbl_logf(TBL_LOG_ERROR, "config load failed: %s", app.config_path);
        }
        return 2;
    }

    switch (app.role) {
        case TBL_ROLE_ALL:    return run_all(&app, &cfg);
        case TBL_ROLE_SERVE:  return run_serve(&app, &cfg);
        case TBL_ROLE_INGEST: return run_ingest(&app, &cfg);
        case TBL_ROLE_VERIFY: return run_verify(&app, &cfg);
        case TBL_ROLE_EXPORT: return run_export(&app, &cfg);
        case TBL_ROLE_INDEX:  return run_index(&app, &cfg);
        case TBL_ROLE_WORKER: return run_worker(&app, &cfg);
        default: break;
    }

    return 2;
}
