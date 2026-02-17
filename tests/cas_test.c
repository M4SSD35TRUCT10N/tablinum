#include <stdio.h>

#define TBL_SAFE_IMPLEMENTATION
#include "core/safe.h"

#define TBL_STR_IMPLEMENTATION
#include "core/str.h"

#define TBL_PATH_IMPLEMENTATION
#include "core/path.h"

#define TBL_FS_IMPLEMENTATION
#include "os/fs.h"

#define TBL_SHA256_IMPLEMENTATION
#include "core/sha256.h"

#define TBL_CAS_IMPLEMENTATION
#include "core/cas.h"

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
    char repo[512];
    char src[512];
    char err[256];
    char sha[65];
    char obj[1024];
    int ex;

    T_ASSERT(mk_tmp_base(base_dir, sizeof(base_dir)) == 1);

    (void)tbl_fs_rm_rf(base_dir);
    T_ASSERT(tbl_fs_mkdir_p(base_dir) == 0);

    T_ASSERT(tbl_path_join2(repo, sizeof(repo), base_dir, "repo") == 1);
    T_ASSERT(tbl_path_join2(src, sizeof(src), base_dir, "in.bin") == 1);

    T_ASSERT(tbl_fs_write_file(src, "abc", 3) == 0);

    err[0] = '\0';
    sha[0] = '\0';
    T_ASSERT(tbl_cas_put_file(repo, src, sha, sizeof(sha), err, sizeof(err)) == 0);
    T_ASSERT(tbl_strlen(sha) == 64);

    T_ASSERT(tbl_cas_object_path(repo, sha, obj, sizeof(obj)) == 1);

    ex = 0;
    (void)tbl_fs_exists(obj, &ex);
    T_ASSERT(ex == 1);

    /* second put should be idempotent */
    err[0] = '\0';
    T_ASSERT(tbl_cas_put_file(repo, src, sha, sizeof(sha), err, sizeof(err)) == 0);

    (void)tbl_fs_rm_rf(base_dir);

    printf("OK\n");
    return 0;
}
