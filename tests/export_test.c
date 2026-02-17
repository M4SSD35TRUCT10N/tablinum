#include <string.h>

#define T_TESTNAME "export_test"
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

#define TBL_RECORD_IMPLEMENTATION
#include "core/record.h"

#define TBL_EVENTS_IMPLEMENTATION
#include "core/events.h"

#define TBL_INGEST_IMPLEMENTATION
#include "core/ingest.h"

#define TBL_EXPORT_IMPLEMENTATION
#include "core/export.h"

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

static int read3(const char *path, char out3[4])
{
    FILE *fp;
    size_t n;

    out3[0] = out3[1] = out3[2] = '\0';
    out3[3] = '\0';

    fp = fopen(path, "rb");
    if (!fp) return 0;
    n = fread(out3, 1, 3, fp);
    fclose(fp);
    if (n != 3) return 0;
    out3[3] = '\0';
    return 1;
}

int main(void)
{
    tbl_cfg_t cfg;
    char base[256];
    char inbox[512];
    char job_ok[512];
    char payload_ok[512];
    char outdir[512];
    char outpayload[1024];
    char outrecord[1024];
    char err[256];
    char repo_root[512];
    int ex;
    char got[4];

    T_ASSERT(mk_tmp_base(base, sizeof(base)) == 1);

    (void)tbl_fs_rm_rf(base);
    T_ASSERT(tbl_fs_mkdir_p(base) == 0);

    (void)memset(&cfg, 0, sizeof(cfg));
    T_ASSERT(tbl_strlcpy(cfg.root, base, sizeof(cfg.root)) < sizeof(cfg.root));
    T_ASSERT(tbl_strlcpy(cfg.spool, "spool", sizeof(cfg.spool)) < sizeof(cfg.spool));
    T_ASSERT(tbl_strlcpy(cfg.repo, "repo", sizeof(cfg.repo)) < sizeof(cfg.repo));
    cfg.ingest_once = 1UL;
    cfg.ingest_poll_seconds = 0UL;
    cfg.ingest_max_jobs = 0UL;

    /* create jobOK with payload.bin */
    T_ASSERT(tbl_path_join2(inbox, sizeof(inbox), base, "spool/inbox") == 1);
    T_ASSERT(tbl_fs_mkdir_p(inbox) == 0);

    T_ASSERT(tbl_path_join2(job_ok, sizeof(job_ok), inbox, "jobOK") == 1);
    T_ASSERT(tbl_fs_mkdir_p(job_ok) == 0);
    T_ASSERT(tbl_path_join2(payload_ok, sizeof(payload_ok), job_ok, "payload.bin") == 1);
    T_ASSERT(tbl_fs_write_file(payload_ok, "abc", 3) == 0);

    err[0] = '\0';
    T_ASSERT(tbl_ingest_run(&cfg, err, sizeof(err)) == 0);

    /* export */
    T_ASSERT(tbl_path_join2(repo_root, sizeof(repo_root), base, "repo") == 1);

    T_ASSERT(tbl_path_join2(outdir, sizeof(outdir), base, "export") == 1);
    err[0] = '\0';
    T_ASSERT(tbl_export_job(repo_root, "jobOK", outdir, err, sizeof(err)) == 0);

    /* verify payload + record exist */
    T_ASSERT(tbl_path_join2(outpayload, sizeof(outpayload), outdir, "payload.bin") == 1);
    ex = 0; (void)tbl_fs_exists(outpayload, &ex); T_ASSERT(ex == 1);

    T_ASSERT(read3(outpayload, got) == 1);
    T_ASSERT_STREQ(got, "abc");

    T_ASSERT(tbl_path_join2(outrecord, sizeof(outrecord), outdir, "record.ini") == 1);
    ex = 0; (void)tbl_fs_exists(outrecord, &ex); T_ASSERT(ex == 1);

    (void)tbl_fs_rm_rf(base);

    T_OK();
}
