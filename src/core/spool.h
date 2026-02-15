#ifndef TBL_CORE_SPOOL_H
#define TBL_CORE_SPOOL_H

#include <stddef.h>

typedef struct tbl_spool_s {
    char root[1024];
    char inbox[1024];
    char claim[1024];
    char out[1024];
    char fail[1024];
} tbl_spool_t;

enum {
    TBL_SPOOL_OK = 0,
    TBL_SPOOL_ENOJOB = 1,
    TBL_SPOOL_EIO = 2,
    TBL_SPOOL_EINVAL = 3
};

/* Initialize spool dirs: root + {inbox,claim,out,fail}. Creates as needed. */
int tbl_spool_init(tbl_spool_t *sp, const char *root, char *err, size_t errsz);

/* Claim next job from inbox -> claim (atomic rename). Returns OK or ENOJOB. */
int tbl_spool_claim_next(tbl_spool_t *sp, char *out_name, size_t out_namesz,
                         char *err, size_t errsz);

/* Move claimed job to out/fail (claim -> out/fail). */
int tbl_spool_commit_out(tbl_spool_t *sp, const char *name, char *err, size_t errsz);
int tbl_spool_commit_fail(tbl_spool_t *sp, const char *name, char *err, size_t errsz);

#ifdef TBL_SPOOL_IMPLEMENTATION

#include <string.h>

#include "core/safe.h"
#include "core/path.h"
#include "os/fs.h"

static void tbl_spool_seterr(char *err, size_t errsz, const char *msg)
{
    if (!err || errsz == 0) return;
    err[0] = '\0';
    if (!msg) msg = "spool error";
    (void)tbl_strlcpy(err, msg, errsz);
}

static int tbl_spool_make_dirs(tbl_spool_t *sp, char *err, size_t errsz)
{
    if (tbl_fs_mkdir_p(sp->root) != 0) { tbl_spool_seterr(err, errsz, "cannot create spool root"); return TBL_SPOOL_EIO; }
    if (tbl_fs_mkdir_p(sp->inbox) != 0) { tbl_spool_seterr(err, errsz, "cannot create inbox"); return TBL_SPOOL_EIO; }
    if (tbl_fs_mkdir_p(sp->claim) != 0) { tbl_spool_seterr(err, errsz, "cannot create claim"); return TBL_SPOOL_EIO; }
    if (tbl_fs_mkdir_p(sp->out) != 0)   { tbl_spool_seterr(err, errsz, "cannot create out"); return TBL_SPOOL_EIO; }
    if (tbl_fs_mkdir_p(sp->fail) != 0)  { tbl_spool_seterr(err, errsz, "cannot create fail"); return TBL_SPOOL_EIO; }
    return TBL_SPOOL_OK;
}

int tbl_spool_init(tbl_spool_t *sp, const char *root, char *err, size_t errsz)
{
    if (err && errsz) err[0] = '\0';
    if (!sp || !root || !root[0]) { tbl_spool_seterr(err, errsz, "invalid args"); return TBL_SPOOL_EINVAL; }

    (void)memset(sp, 0, sizeof(*sp));

    if (tbl_strlcpy(sp->root, root, sizeof(sp->root)) >= sizeof(sp->root)) {
        tbl_spool_seterr(err, errsz, "root path too long");
        return TBL_SPOOL_EINVAL;
    }

    if (!tbl_path_join2(sp->inbox, sizeof(sp->inbox), sp->root, "inbox")) { tbl_spool_seterr(err, errsz, "path join failed"); return TBL_SPOOL_EINVAL; }
    if (!tbl_path_join2(sp->claim, sizeof(sp->claim), sp->root, "claim")) { tbl_spool_seterr(err, errsz, "path join failed"); return TBL_SPOOL_EINVAL; }
    if (!tbl_path_join2(sp->out,   sizeof(sp->out),   sp->root, "out"))   { tbl_spool_seterr(err, errsz, "path join failed"); return TBL_SPOOL_EINVAL; }
    if (!tbl_path_join2(sp->fail,  sizeof(sp->fail),  sp->root, "fail"))  { tbl_spool_seterr(err, errsz, "path join failed"); return TBL_SPOOL_EINVAL; }

    return tbl_spool_make_dirs(sp, err, errsz);
}

typedef struct tbl_spool_claim_ctx_s {
    tbl_spool_t *sp;
    char *out_name;
    size_t out_namesz;
    int claimed;
    char *err;
    size_t errsz;
} tbl_spool_claim_ctx_t;

static int tbl_spool_claim_cb(void *ud, const char *name, const char *fullpath, int is_dir)
{
    tbl_spool_claim_ctx_t *ctx;
    char dst[1024];

    ctx = (tbl_spool_claim_ctx_t *)ud;
    if (!ctx || !ctx->sp) return 1;

    if (ctx->claimed) return 1;
    if (is_dir) return 0;
    if (!name || !name[0]) return 0;

    if (!tbl_path_join2(dst, sizeof(dst), ctx->sp->claim, name)) {
        tbl_spool_seterr(ctx->err, ctx->errsz, "claim path too long");
        return 1;
    }

    /* Claim by atomic-ish rename */
    if (tbl_fs_rename_atomic(fullpath, dst, 0) == 0) {
        if (ctx->out_name && ctx->out_namesz) {
            if (tbl_strlcpy(ctx->out_name, name, ctx->out_namesz) >= ctx->out_namesz) {
                tbl_spool_seterr(ctx->err, ctx->errsz, "job name too long");
                return 1;
            }
        }
        ctx->claimed = 1;
        return 1; /* stop */
    }

    return 0; /* continue */
}

int tbl_spool_claim_next(tbl_spool_t *sp, char *out_name, size_t out_namesz,
                         char *err, size_t errsz)
{
    tbl_spool_claim_ctx_t ctx;

    if (err && errsz) err[0] = '\0';
    if (!sp) { tbl_spool_seterr(err, errsz, "invalid args"); return TBL_SPOOL_EINVAL; }

    ctx.sp = sp;
    ctx.out_name = out_name;
    ctx.out_namesz = out_namesz;
    ctx.claimed = 0;
    ctx.err = err;
    ctx.errsz = errsz;

    if (out_name && out_namesz) out_name[0] = '\0';

    if (tbl_fs_list_dir(sp->inbox, tbl_spool_claim_cb, &ctx) != 0) {
        if (err && errsz && err[0] == '\0') tbl_spool_seterr(err, errsz, "cannot list inbox");
        return TBL_SPOOL_EIO;
    }

    if (!ctx.claimed) {
        return TBL_SPOOL_ENOJOB;
    }

    return TBL_SPOOL_OK;
}

static int tbl_spool_move_claimed(tbl_spool_t *sp, const char *name, const char *dst_dir,
                                  char *err, size_t errsz)
{
    char src[1024];
    char dst[1024];

    if (!sp || !name || !name[0] || !dst_dir) {
        tbl_spool_seterr(err, errsz, "invalid args");
        return TBL_SPOOL_EINVAL;
    }

    if (!tbl_path_join2(src, sizeof(src), sp->claim, name)) { tbl_spool_seterr(err, errsz, "path too long"); return TBL_SPOOL_EINVAL; }
    if (!tbl_path_join2(dst, sizeof(dst), dst_dir, name))   { tbl_spool_seterr(err, errsz, "path too long"); return TBL_SPOOL_EINVAL; }

    if (tbl_fs_rename_atomic(src, dst, 0) != 0) {
        tbl_spool_seterr(err, errsz, "cannot move claimed job");
        return TBL_SPOOL_EIO;
    }

    return TBL_SPOOL_OK;
}

int tbl_spool_commit_out(tbl_spool_t *sp, const char *name, char *err, size_t errsz)
{
    if (err && errsz) err[0] = '\0';
    return tbl_spool_move_claimed(sp, name, sp->out, err, errsz);
}

int tbl_spool_commit_fail(tbl_spool_t *sp, const char *name, char *err, size_t errsz)
{
    if (err && errsz) err[0] = '\0';
    return tbl_spool_move_claimed(sp, name, sp->fail, err, errsz);
}

#endif /* TBL_SPOOL_IMPLEMENTATION */
#endif /* TBL_CORE_SPOOL_H */
