#include <string.h>
#define T_TESTNAME "spool_test"
#include "test.h"


#define TBL_SAFE_IMPLEMENTATION
#include "core/safe.h"

#define TBL_PATH_IMPLEMENTATION
#include "core/path.h"

#define TBL_FS_IMPLEMENTATION
#include "os/fs.h"

#define TBL_SPOOL_IMPLEMENTATION
#include "core/spool.h"

static int mk_tmp_base(char *out, size_t outsz)
{
    char num[16];

    if (!out || outsz == 0) return 0;
    out[0] = '\0';

    if (!tbl_u32_to_dec(tbl_fs_pid_u32(), num, sizeof(num))) return 0;

    if (tbl_strlcpy(out, "tests_tmp_tablinum_", outsz) >= outsz) return 0;
    if (tbl_strlcat(out, num, outsz) >= outsz) return 0;
    return 1;
}

int main(void)
{
    char base[256];
    char spool_root[512];
    char jobpath[512];
    char moved[512];
    char err[256];
    char name[256];
    tbl_spool_t sp;
    int rc;
    int ex;

    T_ASSERT(mk_tmp_base(base, sizeof(base)) == 1);

    /* cleanup from previous run (best effort) */
    (void)tbl_fs_rm_rf(base);

    /* base dir */
    T_ASSERT(tbl_fs_mkdir_p(base) == 0);

    /* spool root under base */
    T_ASSERT(tbl_path_join2(spool_root, sizeof(spool_root), base, "spool") == 1);

    err[0] = '\0';
    rc = tbl_spool_init(&sp, spool_root, err, sizeof(err));
    if (rc != TBL_SPOOL_OK) {
        fprintf(stderr, "spool_init failed: %s\n", err);
    }
    T_ASSERT(rc == TBL_SPOOL_OK);

    /* drop a job into inbox */
    T_ASSERT(tbl_path_join2(jobpath, sizeof(jobpath), sp.inbox, "job1.txt") == 1);
    T_ASSERT(tbl_fs_write_file(jobpath, "x", 1) == 0);

    /* claim it */
    err[0] = '\0';
    name[0] = '\0';
    rc = tbl_spool_claim_next(&sp, name, sizeof(name), err, sizeof(err));
    if (rc != TBL_SPOOL_OK) {
        fprintf(stderr, "claim_next failed: %s\n", err);
    }
    T_ASSERT(rc == TBL_SPOOL_OK);
    T_ASSERT(strcmp(name, "job1.txt") == 0);

    /* verify moved inbox -> claim */
    ex = 0;
    (void)tbl_fs_exists(jobpath, &ex);
    T_ASSERT(ex == 0);

    T_ASSERT(tbl_path_join2(moved, sizeof(moved), sp.claim, "job1.txt") == 1);
    ex = 0;
    (void)tbl_fs_exists(moved, &ex);
    T_ASSERT(ex == 1);

    /* commit to out */
    err[0] = '\0';
    rc = tbl_spool_commit_out(&sp, "job1.txt", err, sizeof(err));
    if (rc != TBL_SPOOL_OK) {
        fprintf(stderr, "commit_out failed: %s\n", err);
    }
    T_ASSERT(rc == TBL_SPOOL_OK);

    ex = 0;
    (void)tbl_fs_exists(moved, &ex);
    T_ASSERT(ex == 0);

    T_ASSERT(tbl_path_join2(moved, sizeof(moved), sp.out, "job1.txt") == 1);
    ex = 0;
    (void)tbl_fs_exists(moved, &ex);
    T_ASSERT(ex == 1);

    /* drop a second job and fail it */
    T_ASSERT(tbl_path_join2(jobpath, sizeof(jobpath), sp.inbox, "job2.txt") == 1);
    T_ASSERT(tbl_fs_write_file(jobpath, "y", 1) == 0);

    err[0] = '\0';
    name[0] = '\0';
    rc = tbl_spool_claim_next(&sp, name, sizeof(name), err, sizeof(err));
    T_ASSERT(rc == TBL_SPOOL_OK);
    T_ASSERT(strcmp(name, "job2.txt") == 0);

    err[0] = '\0';
    rc = tbl_spool_commit_fail(&sp, "job2.txt", err, sizeof(err));
    T_ASSERT(rc == TBL_SPOOL_OK);

    T_ASSERT(tbl_path_join2(moved, sizeof(moved), sp.fail, "job2.txt") == 1);
    ex = 0;
    (void)tbl_fs_exists(moved, &ex);
    T_ASSERT(ex == 1);

    /* no more jobs */
    err[0] = '\0';
    rc = tbl_spool_claim_next(&sp, name, sizeof(name), err, sizeof(err));
    T_ASSERT(rc == TBL_SPOOL_ENOJOB);

    /* cleanup */
    (void)tbl_fs_rm_rf(base);

        T_OK();
}
