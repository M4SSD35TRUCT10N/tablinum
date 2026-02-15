/* src/tablinum.c - Tablinum entrypoint (strict C89, fail-fast, tack-typisch) */
#include "tablinum.h"

#include "core/args.h"
#include "core/config.h"
#include "core/log.h"
#include "core/path.h"
#include "core/safe.h"
#include "core/spool.h"
#include "core/str.h"
#include "os/time.h"
#include "os/fs.h"

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
    tbl_spool_t sp;
    char spool_root[1024];
    char err[256];
    char name[256];
    unsigned long poll_ms;

    (void)app;

    /* resolve spool path */
    spool_root[0] = '\0';
    if (tbl_path_is_abs(cfg->spool)) {
        if (tbl_strlcpy(spool_root, cfg->spool, sizeof(spool_root)) >= sizeof(spool_root)) {
            tbl_logf(TBL_LOG_ERROR, "spool path too long");
            return 2;
        }
    } else {
        if (!tbl_path_join2(spool_root, sizeof(spool_root), cfg->root, cfg->spool)) {
            tbl_logf(TBL_LOG_ERROR, "spool path join failed");
            return 2;
        }
    }

    err[0] = '\0';
    if (tbl_spool_init(&sp, spool_root, err, sizeof(err)) != TBL_SPOOL_OK) {
        tbl_logf(TBL_LOG_ERROR, "%s", err[0] ? err : "spool init failed");
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

    tbl_logf(TBL_LOG_INFO, "[ingest] running (spool=%s, poll=%lu ms)", spool_root, poll_ms);

    for (;;) {
        int rc;
        int should_fail;
        size_t n;

        err[0] = '\0';
        name[0] = '\0';

        rc = tbl_spool_claim_next(&sp, name, sizeof(name), err, sizeof(err));
        if (rc == TBL_SPOOL_ENOJOB) {
            tbl_sleep_ms(poll_ms);
            continue;
        }
        if (rc != TBL_SPOOL_OK) {
            tbl_logf(TBL_LOG_ERROR, "%s", err[0] ? err : "claim failed");
            return 2;
        }

        /* demo failure rule: names ending with ".bad" -> fail */
        should_fail = 0;
        if (tbl_str_ends_with(name, ".bad")) {
            should_fail = 1;
        }

        if (should_fail) {
            err[0] = '\0';
            rc = tbl_spool_commit_fail(&sp, name, err, sizeof(err));
            if (rc != TBL_SPOOL_OK) {
                tbl_logf(TBL_LOG_ERROR, "%s", err[0] ? err : "commit_fail failed");
                return 2;
            }
            tbl_logf(TBL_LOG_WARN, "job failed: %s", name);
        } else {
            char meta_name[300];
            char meta_path[1024];
            const char *meta = "status=ok\n";

            err[0] = '\0';
            rc = tbl_spool_commit_out(&sp, name, err, sizeof(err));
            if (rc != TBL_SPOOL_OK) {
                tbl_logf(TBL_LOG_ERROR, "%s", err[0] ? err : "commit_out failed");
                return 2;
            }

            /* write a tiny sidecar into out: <name>.meta */
            meta_name[0] = '\0';
            if (tbl_strlcpy(meta_name, name, sizeof(meta_name)) >= sizeof(meta_name) ||
                tbl_strlcat(meta_name, ".meta", sizeof(meta_name)) >= sizeof(meta_name)) {
                tbl_logf(TBL_LOG_ERROR, "meta name too long for job: %s", name);
                return 2;
            }

            if (!tbl_path_join2(meta_path, sizeof(meta_path), sp.out, meta_name)) {
                tbl_logf(TBL_LOG_ERROR, "meta path too long for job: %s", name);
                return 2;
            }

            if (tbl_fs_write_file(meta_path, meta, (size_t)tbl_strlen(meta)) != 0) {
                tbl_logf(TBL_LOG_ERROR, "cannot write meta for job: %s", name);
                return 2;
            }

            tbl_logf(TBL_LOG_INFO, "job ok: %s", name);
        }
    }

    /* not reached */
    /* return 0; */
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
        case TBL_ROLE_INDEX:  return run_index(&app, &cfg);
        case TBL_ROLE_WORKER: return run_worker(&app, &cfg);
        default: break;
    }

    return 2;
}
