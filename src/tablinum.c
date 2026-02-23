/* src/tablinum.c - Tablinum entrypoint (strict C89, fail-fast, tack-typisch) */
#include "tablinum.h"

#include "core/args.h"
#include "core/audit.h"
#include "core/config.h"
#include "core/export.h"
#include "core/package.h"
#include "core/pkgverify.h"
#include "core/record.h"
#include "core/ingest.h"
#include "core/ini.h"
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

    if (cfg->repo[0] == '\0') return 0;

    if (tbl_path_is_abs(cfg->repo)) {
        return (tbl_strlcpy(out, cfg->repo, outsz) < outsz) ? 1 : 0;
    }
    return tbl_path_join2(out, outsz, cfg->root, cfg->repo);
}

static int run_verify(const tbl_app_config_t *app, const tbl_cfg_t *cfg)
{
    char repo_root[1024];
    char err[256];
    int rc;

    if (!app || !cfg) return TBL_EXIT_USAGE;
    if (!app->jobid || !app->jobid[0]) {
        tbl_logf(TBL_LOG_ERROR, "[verify] missing JOBID");
        return TBL_EXIT_USAGE;
    }

    if (!resolve_repo_root(repo_root, sizeof(repo_root), cfg)) {
        tbl_logf(TBL_LOG_ERROR, "[verify] repo path resolve failed");
        return TBL_EXIT_SCHEMA;
    }

    err[0] = '\0';
    rc = tbl_verify_job(repo_root, app->jobid, err, sizeof(err));

    if (rc == 0) {
        tbl_logf(TBL_LOG_INFO, "[verify] OK %s", app->jobid);
        return TBL_EXIT_OK;
    }

    /* rc==1 means "soft" failure/skip in the legacy verifier.
       Map into stable exit codes: SKIP => 0, mismatch => integrity. */
    if (rc == 1) {
        if (tbl_str_starts_with(err, "no record") ||
            tbl_str_starts_with(err, "record status is not ok")) {
            tbl_logf(TBL_LOG_INFO, "[verify] SKIP %s: %s", app->jobid, err[0] ? err : "skip");
            return TBL_EXIT_OK;
        }

        tbl_logf(TBL_LOG_ERROR, "[verify] INTEGRITY %s: %s", app->jobid, err[0] ? err : "integrity failure");
        return TBL_EXIT_INTEGRITY;
    }

    if (err[0] != '\0') {
        tbl_logf(TBL_LOG_ERROR, "[verify] ERROR %s: %s", app->jobid, err);
    } else {
        tbl_logf(TBL_LOG_ERROR, "[verify] ERROR %s", app->jobid);
    }

    if (tbl_str_starts_with(err, "unsafe") || tbl_str_starts_with(err, "invalid")) {
        return TBL_EXIT_SCHEMA;
    }
    if (tbl_str_starts_with(err, "object missing")) {
        return TBL_EXIT_INTEGRITY;
    }

    return TBL_EXIT_IO;
}

static int run_export(const tbl_app_config_t *app, const tbl_cfg_t *cfg)
{
    char repo_root[1024];
    char err[256];
    int rc;

    if (!app || !cfg) return TBL_EXIT_USAGE;
    if (!app->jobid || !app->jobid[0] || !app->out_dir || !app->out_dir[0]) {
        tbl_logf(TBL_LOG_ERROR, "[export] needs JOBID and OUTDIR");
        return TBL_EXIT_USAGE;
    }

    if (!resolve_repo_root(repo_root, sizeof(repo_root), cfg)) {
        tbl_logf(TBL_LOG_ERROR, "[export] repo path resolve failed");
        return TBL_EXIT_SCHEMA;
    }

    err[0] = '\0';
    rc = tbl_export_job(repo_root, app->jobid, app->out_dir, err, sizeof(err));
    if (rc == 0) {
        tbl_logf(TBL_LOG_INFO, "[export] OK %s -> %s", app->jobid, app->out_dir);
        return TBL_EXIT_OK;
    }

    if (err[0] != '\0') {
        tbl_logf(TBL_LOG_ERROR, "[export] FAIL %s: %s", app->jobid, err);
    } else {
        tbl_logf(TBL_LOG_ERROR, "[export] FAIL %s", app->jobid);
    }

    /* try to distinguish missing record from other I/O */
    {
        char recpath[1024];
        int ex = 0;
        if (tbl_record_path(repo_root, app->jobid, recpath, sizeof(recpath))) {
            (void)tbl_fs_exists(recpath, &ex);
            if (!ex) return TBL_EXIT_NOTFOUND;
        }
    }

    if (tbl_str_starts_with(err, "unsafe") || tbl_str_starts_with(err, "invalid")) {
        return TBL_EXIT_SCHEMA;
    }

    return TBL_EXIT_IO;
}

static int run_package(const tbl_app_config_t *app, const tbl_cfg_t *cfg)
{
    char repo_root[1024];
    char err[256];
    int rc;

    if (!app || !cfg) return TBL_EXIT_USAGE;
    if (!app->jobid || !app->jobid[0] || !app->out_dir || !app->out_dir[0]) {
        tbl_logf(TBL_LOG_ERROR, "[package] needs JOBID and OUTDIR");
        return TBL_EXIT_USAGE;
    }

    if (!resolve_repo_root(repo_root, sizeof(repo_root), cfg)) {
        tbl_logf(TBL_LOG_ERROR, "[package] repo path resolve failed");
        return TBL_EXIT_SCHEMA;
    }

    err[0] = '\0';
    rc = tbl_package_job(repo_root, app->jobid, app->out_dir, app->pkg_kind, err, sizeof(err));
    if (rc == 0) {
        tbl_logf(TBL_LOG_INFO, "[package] OK %s -> %s", app->jobid, app->out_dir);
        return TBL_EXIT_OK;
    }

    if (err[0] != '\0') {
        tbl_logf(TBL_LOG_ERROR, "[package] FAIL %s: %s", app->jobid, err);
    } else {
        tbl_logf(TBL_LOG_ERROR, "[package] FAIL %s", app->jobid);
    }

    /* missing record => notfound */
    {
        char recpath[1024];
        int ex = 0;
        if (tbl_record_path(repo_root, app->jobid, recpath, sizeof(recpath))) {
            (void)tbl_fs_exists(recpath, &ex);
            if (!ex) return TBL_EXIT_NOTFOUND;
        }
    }

    if (tbl_str_starts_with(err, "record status") ||
        tbl_str_starts_with(err, "sha256") ||
        tbl_str_starts_with(err, "object") ) {
        return TBL_EXIT_INTEGRITY;
    }

    if (tbl_str_starts_with(err, "unsafe") || tbl_str_starts_with(err, "invalid")) {
        return TBL_EXIT_SCHEMA;
    }

    return TBL_EXIT_IO;
}

static int run_verify_package(const tbl_app_config_t *app)
{
    char err[256];
    int rc;

    if (!app) return 2;
    if (!app->pkg_dir || !app->pkg_dir[0]) {
        tbl_logf(TBL_LOG_ERROR, "[verify-package] needs PKGDIR");
        return 2;
    }

    err[0] = '\0';
    rc = tbl_verify_package_dir(app->pkg_dir, err, sizeof(err));
    if (rc == 0) {
        tbl_logf(TBL_LOG_INFO, "[verify-package] OK %s", app->pkg_dir);
        return 0;
    }

    if (err[0] != '\0') {
        tbl_logf(TBL_LOG_ERROR, "[verify-package] FAIL %s: %s", app->pkg_dir, err);
    } else {
        tbl_logf(TBL_LOG_ERROR, "[verify-package] FAIL %s", app->pkg_dir);
    }

    return rc;
}

static int run_ingest_package(const tbl_app_config_t *app, const tbl_cfg_t *cfg)
{
    char repo_root[1024];
    char err[256];
    int rc;

    if (!app || !cfg) return 2;
    if (!app->pkg_dir || !app->pkg_dir[0]) {
        tbl_logf(TBL_LOG_ERROR, "[ingest-package] needs PKGDIR");
        return 2;
    }

    if (!resolve_repo_root(repo_root, sizeof(repo_root), cfg)) {
        tbl_logf(TBL_LOG_ERROR, "[ingest-package] repo path resolve failed");
        return 2;
    }

    err[0] = '\0';
    rc = tbl_ingest_package_dir(repo_root, app->pkg_dir, err, sizeof(err));
    if (rc == 0) {
        tbl_logf(TBL_LOG_INFO, "[ingest-package] OK %s", app->pkg_dir);
        return 0;
    }

    if (err[0] != '\0') {
        tbl_logf(TBL_LOG_ERROR, "[ingest-package] FAIL %s: %s", app->pkg_dir, err);
    } else {
        tbl_logf(TBL_LOG_ERROR, "[ingest-package] FAIL %s", app->pkg_dir);
    }

    return rc;
}

static int run_verify_audit(const tbl_app_config_t *app, const tbl_cfg_t *cfg)
{
    char repo_root[1024];
    char err[256];
    int rc;

    (void)app;

    if (!cfg) return TBL_EXIT_USAGE;
    if (!resolve_repo_root(repo_root, sizeof(repo_root), cfg)) {
        tbl_logf(TBL_LOG_ERROR, "[verify-audit] repo path resolve failed");
        return TBL_EXIT_NOTFOUND;
    }

    err[0] = '\0';
    rc = tbl_audit_verify_ops(repo_root, err, sizeof(err));
    if (rc == 0) {
        tbl_logf(TBL_LOG_INFO, "[verify-audit] OK %s/audit/ops.log", repo_root);
        return TBL_EXIT_OK;
    }

    tbl_logf(TBL_LOG_ERROR, "[verify-audit] FAIL: %s", err[0] ? err : "audit verify failed");
    return rc;
}
static int run_all(const tbl_app_config_t *app, const tbl_cfg_t *cfg)
{
    (void)app;
    (void)cfg;

    tbl_logf(TBL_LOG_ERROR, "[all] not implemented yet");
    return 2;
}

static int run_serve(const tbl_app_config_t *app, const tbl_cfg_t *cfg)
{
    (void)app;
    (void)cfg;

    tbl_logf(TBL_LOG_ERROR, "[serve] not implemented yet (listen=%s)", cfg->http_listen);
    return 2;
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

static int run_index(const tbl_app_config_t *app, const tbl_cfg_t *cfg)
{
    (void)app;
    (void)cfg;

    tbl_logf(TBL_LOG_ERROR, "[index] not implemented yet (db=%s)", cfg->db);
    return 2;
}

static int run_worker(const tbl_app_config_t *app, const tbl_cfg_t *cfg)
{
    (void)app;
    (void)cfg;

    tbl_logf(TBL_LOG_ERROR, "[worker] not implemented yet");
    return 2;
}

int main(int argc, char **argv)
{
        int rc;
    int ex;
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
        return TBL_EXIT_OK;
    }
    if (rc != 0) {
        /* strict: args parser already printed an error */
        return TBL_EXIT_USAGE;
    }

    /* verify-package is fully self-contained (no repo config needed) */
    if (app.role == TBL_ROLE_VERIFY_PACKAGE) {
        return run_verify_package(&app);
    }

    /* load INI config (fail-fast, strict schema) */
    ex = 0;
    (void)tbl_fs_exists(app.config_path, &ex);
    if (!ex) {
        tbl_logf(TBL_LOG_ERROR, "config not found: %s", app.config_path);
        return TBL_EXIT_NOTFOUND;
    }

    err[0] = '\0';
    rc = tbl_cfg_load(&cfg, app.config_path, err, sizeof(err));
    if (rc != 0) {
        if (err[0] != '\0') {
            tbl_logf(TBL_LOG_ERROR, "%s", err);
        } else {
            tbl_logf(TBL_LOG_ERROR, "config load failed: %s", app.config_path);
        }

        if (rc == TBL_INI_EIO) return TBL_EXIT_IO;
        return TBL_EXIT_SCHEMA;
    }

    switch (app.role) {
        case TBL_ROLE_ALL:    return run_all(&app, &cfg);
        case TBL_ROLE_SERVE:  return run_serve(&app, &cfg);
        case TBL_ROLE_INGEST: return run_ingest(&app, &cfg);
        case TBL_ROLE_INDEX:  return run_index(&app, &cfg);
        case TBL_ROLE_WORKER: return run_worker(&app, &cfg);
        case TBL_ROLE_VERIFY: return run_verify(&app, &cfg);
        case TBL_ROLE_EXPORT: return run_export(&app, &cfg);
        case TBL_ROLE_PACKAGE:       return run_package(&app, &cfg);
        case TBL_ROLE_INGEST_PACKAGE:return run_ingest_package(&app, &cfg);
        case TBL_ROLE_VERIFY_PACKAGE:return run_verify_package(&app);
        case TBL_ROLE_VERIFY_AUDIT:  return run_verify_audit(&app, &cfg);
        default: break;
    }

    return TBL_EXIT_USAGE;
}
