#ifndef TBL_CORE_CONFIG_H
#define TBL_CORE_CONFIG_H

#include <stddef.h>
#include "tablinum.h"
#include "core/str.h"

#define TBL_CFG_PATH_MAX   1024
#define TBL_CFG_LISTEN_MAX 128

typedef struct tbl_cfg_s {
    char root[TBL_CFG_PATH_MAX];
    char spool[TBL_CFG_PATH_MAX];
    char repo[TBL_CFG_PATH_MAX];
    char db[TBL_CFG_PATH_MAX];

    char http_listen[TBL_CFG_LISTEN_MAX];

    /* ingest */
    unsigned long ingest_poll_seconds; /* exists already */
    unsigned long ingest_once;         /* 0|1 */
    unsigned long ingest_max_jobs;     /* 0 = unlimited */
} tbl_cfg_t;

void tbl_cfg_defaults(tbl_cfg_t *cfg);

int tbl_cfg_load(tbl_cfg_t *cfg, const char *path, char *err, size_t errsz);
int tbl_cfg_load_buf(tbl_cfg_t *cfg, const char *buf, size_t len, char *err, size_t errsz);

#ifdef TBL_CONFIG_IMPLEMENTATION

#include <string.h>

#include "core/ini.h"
#include "core/safe.h"

static void tbl_cfg_seterr(char *err, size_t errsz, const char *msg)
{
    if (!err || errsz == 0) return;
    err[0] = '\0';
    if (!msg) msg = "config error";
    (void)tbl_strlcpy(err, msg, errsz);
}

void tbl_cfg_defaults(tbl_cfg_t *cfg)
{
    if (!cfg) return;

    cfg->root[0] = '\0';
    cfg->spool[0] = '\0';
    cfg->repo[0] = '\0';
    cfg->db[0] = '\0';
    cfg->http_listen[0] = '\0';

    (void)tbl_strlcpy(cfg->root, ".", sizeof(cfg->root));
    (void)tbl_strlcpy(cfg->spool, "spool", sizeof(cfg->spool));
    (void)tbl_strlcpy(cfg->repo, "repo", sizeof(cfg->repo));
    (void)tbl_strlcpy(cfg->db, "db/tablinum.sqlite", sizeof(cfg->db));
    (void)tbl_strlcpy(cfg->http_listen, "0.0.0.0:8080", sizeof(cfg->http_listen));

    cfg->ingest_poll_seconds = 2UL;
    cfg->ingest_once = 0UL;
    cfg->ingest_max_jobs = 0UL;
}

typedef struct tbl_cfg_ctx_s {
    tbl_cfg_t *cfg;
    char *err;
    size_t errsz;
} tbl_cfg_ctx_t;

static int tbl_cfg_copy(char *dst, size_t dstsz, const char *src, tbl_cfg_ctx_t *ctx)
{
    if (tbl_strlcpy(dst, src ? src : "", dstsz) >= dstsz) {
        tbl_cfg_seterr(ctx->err, ctx->errsz, "value too long");
        return 0;
    }
    return 1;
}

static int tbl_cfg_on_kv(void *ud, const char *section, const char *key, const char *value, int line_no)
{
    tbl_cfg_ctx_t *ctx;
    (void)line_no;

    ctx = (tbl_cfg_ctx_t *)ud;
    if (!ctx || !ctx->cfg) return 1;

    if (!section || section[0] == '\0') {
        tbl_cfg_seterr(ctx->err, ctx->errsz, "global keys not allowed (missing [section])");
        return 1;
    }

    if (strcmp(section, "core") == 0) {
        if (strcmp(key, "root") == 0) {
            return tbl_cfg_copy(ctx->cfg->root, sizeof(ctx->cfg->root), value, ctx) ? 0 : 1;
        }
        if (strcmp(key, "spool") == 0) {
            return tbl_cfg_copy(ctx->cfg->spool, sizeof(ctx->cfg->spool), value, ctx) ? 0 : 1;
        }
        if (strcmp(key, "repo") == 0) {
            return tbl_cfg_copy(ctx->cfg->repo, sizeof(ctx->cfg->repo), value, ctx) ? 0 : 1;
        }
        if (strcmp(key, "db") == 0) {
            return tbl_cfg_copy(ctx->cfg->db, sizeof(ctx->cfg->db), value, ctx) ? 0 : 1;
        }

        tbl_cfg_seterr(ctx->err, ctx->errsz, "unknown key in [core]");
        return 1;
    }

    if (strcmp(section, "http") == 0) {
        if (strcmp(key, "listen") == 0) {
            return tbl_cfg_copy(ctx->cfg->http_listen, sizeof(ctx->cfg->http_listen), value, ctx) ? 0 : 1;
        }
        tbl_cfg_seterr(ctx->err, ctx->errsz, "unknown key in [http]");
        return 1;
    }

    if (strcmp(section, "ingest") == 0) {
        if (strcmp(key, "poll_seconds") == 0) {
            unsigned long v;
            if (!tbl_parse_u32_ok(value, &v)) {
                tbl_cfg_seterr(ctx->err, ctx->errsz, "invalid poll_seconds");
                return 1;
            }
            if (v == 0UL) {
                tbl_cfg_seterr(ctx->err, ctx->errsz, "poll_seconds must be > 0");
                return 1;
            }
            ctx->cfg->ingest_poll_seconds = v;
            return 0;
        }

        if (strcmp(key, "once") == 0) {
            unsigned long v;
            if (!tbl_parse_u32_ok(value, &v)) {
                tbl_cfg_seterr(ctx->err, ctx->errsz, "invalid once");
                return 1;
            }
            if (v > 1UL) {
                tbl_cfg_seterr(ctx->err, ctx->errsz, "once must be 0 or 1");
                return 1;
            }
            ctx->cfg->ingest_once = v;
            return 0;
        }

        if (strcmp(key, "max_jobs") == 0) {
            unsigned long v;
            if (!tbl_parse_u32_ok(value, &v)) {
                tbl_cfg_seterr(ctx->err, ctx->errsz, "invalid max_jobs");
                return 1;
            }
            ctx->cfg->ingest_max_jobs = v;
            return 0;
        }

        tbl_cfg_seterr(ctx->err, ctx->errsz, "unknown key in [ingest]");
        return 1;
    }

    tbl_cfg_seterr(ctx->err, ctx->errsz, "unknown section");
    return 1;
}

int tbl_cfg_load(tbl_cfg_t *cfg, const char *path, char *err, size_t errsz)
{
    tbl_cfg_ctx_t ctx;
    int rc;

    if (err && errsz) err[0] = '\0';
    if (!cfg) {
        tbl_cfg_seterr(err, errsz, "cfg is NULL");
        return 2;
    }

    tbl_cfg_defaults(cfg);

    ctx.cfg = cfg;
    ctx.err = err;
    ctx.errsz = errsz;

    rc = tbl_ini_parse_file(path, tbl_cfg_on_kv, &ctx, err, errsz);
    if (rc != TBL_INI_OK) {
        return rc;
    }

    return 0;
}

int tbl_cfg_load_buf(tbl_cfg_t *cfg, const char *buf, size_t len, char *err, size_t errsz)
{
    tbl_cfg_ctx_t ctx;
    int rc;

    if (err && errsz) err[0] = '\0';
    if (!cfg) {
        tbl_cfg_seterr(err, errsz, "cfg is NULL");
        return 2;
    }

    tbl_cfg_defaults(cfg);

    ctx.cfg = cfg;
    ctx.err = err;
    ctx.errsz = errsz;

    rc = tbl_ini_parse_buf(buf, len, tbl_cfg_on_kv, &ctx, err, errsz);
    if (rc != TBL_INI_OK) {
        return rc;
    }

    return 0;
}

#endif /* TBL_CONFIG_IMPLEMENTATION */

#endif /* TBL_CORE_CONFIG_H */
