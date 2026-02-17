#include <stdio.h>

#define TBL_SAFE_IMPLEMENTATION
#include "core/safe.h"

#define TBL_STR_IMPLEMENTATION
#include "core/str.h"

#define TBL_PATH_IMPLEMENTATION
#include "core/path.h"

#define TBL_FS_IMPLEMENTATION
#include "os/fs.h"

#define TBL_SPOOL_IMPLEMENTATION
#include "core/spool.h"

#define T_ASSERT(COND) do { \
    if (!(COND)) { \
        fprintf(stderr, "assert failed: %s (line %d)\n", #COND, __LINE__); \
        return 1; \
    } \
} while (0)

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
    char base_dir[256];
    char spool_root[512];
    char jobdir[512];
    char inside[512];
    char moved[512];
    char err[256];
    char name[256];
    tbl_spool_t sp;
    int rc;
    int ex;

    T_ASSERT(mk_tmp_base(base_dir, sizeof(base_dir)) == 1);

    (void)tbl_fs_rm_rf(base_dir);
    T_ASSERT(tbl_fs_mkdir_p(base_dir) == 0);

    T_ASSERT(tbl_path_join2(spool_root, sizeof(spool_root), base_dir, "spool") == 1);

    err[0] = '\0';
    rc = tbl_spool_init(&sp, spool_root, err, sizeof(err));
    T_ASSERT(rc == TBL_SPOOL_OK);

    /* create inbox/jobA/ and a payload file inside */
    T_ASSERT(tbl_path_join2(jobdir, sizeof(jobdir), sp.inbox, "jobA") == 1);
    T_ASSERT(tbl_fs_mkdir_p(jobdir) == 0);
    T_ASSERT(tbl_path_join2(inside, sizeof(inside), jobdir, "payload.bin") == 1);
    T_ASSERT(tbl_fs_write_file(inside, "x", 1) == 0);

    /* claim directory */
    err[0] = '\0';
    name[0] = '\0';
    rc = tbl_spool_claim_next_dir(&sp, name, sizeof(name), err, sizeof(err));
    T_ASSERT(rc == TBL_SPOOL_OK);
    T_ASSERT(tbl_streq(name, "jobA") == 1);

    ex = 0;
    (void)tbl_fs_exists(jobdir, &ex);
    T_ASSERT(ex == 0);

    T_ASSERT(tbl_path_join2(moved, sizeof(moved), sp.claim, "jobA") == 1);
    ex = 0;
    (void)tbl_fs_exists(moved, &ex);
    T_ASSERT(ex == 1);

    /* commit to out */
    err[0] = '\0';
    rc = tbl_spool_commit_out(&sp, "jobA", err, sizeof(err));
    T_ASSERT(rc == TBL_SPOOL_OK);

    T_ASSERT(tbl_path_join2(moved, sizeof(moved), sp.out, "jobA") == 1);
    ex = 0;
    (void)tbl_fs_exists(moved, &ex);
    T_ASSERT(ex == 1);

    (void)tbl_fs_rm_rf(base_dir);

    printf("OK\n");
    return 0;
}
