#include <string.h>
#define T_TESTNAME "ingest_jobdir_test"
#include "test.h"


#define TBL_SAFE_IMPLEMENTATION
#include "core/safe.h"

#define TBL_STR_IMPLEMENTATION
#include "core/str.h"

#define TBL_PATH_IMPLEMENTATION
#include "core/path.h"

#define TBL_FS_IMPLEMENTATION
#include "os/fs.h"

#define TBL_TIME_IMPLEMENTATION
#include "os/time.h"

#define TBL_SHA256_IMPLEMENTATION
#include "core/sha256.h"

#define TBL_CAS_IMPLEMENTATION
#include "core/cas.h"

#define TBL_SPOOL_IMPLEMENTATION
#include "core/spool.h"

#define TBL_INGEST_IMPLEMENTATION
#include "core/ingest.h"

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
    tbl_cfg_t cfg;
    char base[256];
    char inbox[512];
    char job_ok[512];
    char job_bad[512];
    char payload_ok[512];
    char out_ok[512];
    char fail_bad[512];
    char repo_root[512];
    char obj[1024];
    char err[256];
    int ex;

    /* expected sha256("abc") */
    const char *sha_abc = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";

    T_ASSERT(mk_tmp_base(base, sizeof(base)) == 1);

    (void)tbl_fs_rm_rf(base);
    T_ASSERT(tbl_fs_mkdir_p(base) == 0);

    /* cfg */
    (void)memset(&cfg, 0, sizeof(cfg));
    T_ASSERT(tbl_strlcpy(cfg.root, base, sizeof(cfg.root)) < sizeof(cfg.root));
    T_ASSERT(tbl_strlcpy(cfg.spool, "spool", sizeof(cfg.spool)) < sizeof(cfg.spool));
    T_ASSERT(tbl_strlcpy(cfg.repo, "repo", sizeof(cfg.repo)) < sizeof(cfg.repo));
    cfg.ingest_once = 1UL;
    cfg.ingest_poll_seconds = 0UL;
    cfg.ingest_max_jobs = 0UL;

    /* create jobdirs in inbox */
    T_ASSERT(tbl_path_join2(inbox, sizeof(inbox), base, "spool/inbox") == 1);
    T_ASSERT(tbl_fs_mkdir_p(inbox) == 0);

    T_ASSERT(tbl_path_join2(job_ok, sizeof(job_ok), inbox, "jobOK") == 1);
    T_ASSERT(tbl_fs_mkdir_p(job_ok) == 0);
    T_ASSERT(tbl_path_join2(payload_ok, sizeof(payload_ok), job_ok, "payload.bin") == 1);
    T_ASSERT(tbl_fs_write_file(payload_ok, "abc", 3) == 0);

    T_ASSERT(tbl_path_join2(job_bad, sizeof(job_bad), inbox, "jobBAD") == 1);
    T_ASSERT(tbl_fs_mkdir_p(job_bad) == 0);
    /* no payload.bin -> should fail */

    err[0] = '\0';
    T_ASSERT(tbl_ingest_run(&cfg, err, sizeof(err)) == 0);

    /* verify out and fail */
    T_ASSERT(tbl_path_join2(out_ok, sizeof(out_ok), base, "spool/out/jobOK") == 1);
    ex = 0; (void)tbl_fs_exists(out_ok, &ex); T_ASSERT(ex == 1);

    T_ASSERT(tbl_path_join2(fail_bad, sizeof(fail_bad), base, "spool/fail/jobBAD") == 1);
    ex = 0; (void)tbl_fs_exists(fail_bad, &ex); T_ASSERT(ex == 1);

    /* verify CAS object exists */
    T_ASSERT(tbl_path_join2(repo_root, sizeof(repo_root), base, "repo") == 1);
    T_ASSERT(tbl_cas_object_path(repo_root, sha_abc, obj, sizeof(obj)) == 1);
    ex = 0; (void)tbl_fs_exists(obj, &ex); T_ASSERT(ex == 1);

    (void)tbl_fs_rm_rf(base);

        T_OK();
}
