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

    if (!tbl_u32_to_dec_ok(tbl_fs_pid_u32(), num, sizeof(num))) return 0;

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

static int hash_file_hex(const char *path, char out_hex[65])
{
    FILE *fp;
    unsigned char buf[16384];
    size_t rd;
    tbl_sha256_t st;
    unsigned char dig[32];

    if (!path || !path[0] || !out_hex) return 0;
    out_hex[0] = '\0';

    fp = fopen(path, "rb");
    if (!fp) return 0;

    tbl_sha256_init(&st);
    for (;;) {
        rd = fread(buf, 1, sizeof(buf), fp);
        if (rd > 0) tbl_sha256_update(&st, buf, rd);
        if (rd < sizeof(buf)) {
            if (ferror(fp)) {
                fclose(fp);
                return 0;
            }
            break;
        }
    }
    fclose(fp);
    tbl_sha256_final(&st, dig);
    if (!tbl_sha256_hex_ok(dig, out_hex, 65)) return 0;
    return 1;
}

static int read_all(const char *path, char *buf, size_t bufsz)
{
    FILE *fp;
    size_t n;

    if (!path || !path[0] || !buf || bufsz == 0) return 0;
    buf[0] = '\0';

    fp = fopen(path, "rb");
    if (!fp) return 0;

    n = fread(buf, 1, bufsz - 1, fp);
    fclose(fp);
    buf[n] = '\0';
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
    char outmanifest[1024];
    char err[256];
    char repo_root[512];
    int ex;
    char got[4];
    char sha_payload[65];
    char sha_record[65];
    char manifest[2048];
    char needle[256];

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

    /* verify manifest exists and matches exported files */
    T_ASSERT(tbl_path_join2(outmanifest, sizeof(outmanifest), outdir, "manifest-sha256.txt") == 1);
    ex = 0; (void)tbl_fs_exists(outmanifest, &ex); T_ASSERT(ex == 1);

    T_ASSERT(hash_file_hex(outpayload, sha_payload) == 1);
    T_ASSERT(hash_file_hex(outrecord, sha_record) == 1);

    T_ASSERT(read_all(outmanifest, manifest, sizeof(manifest)) == 1);

    needle[0] = '\0';
    T_ASSERT(tbl_strlcpy(needle, sha_payload, sizeof(needle)) < sizeof(needle));
    T_ASSERT(tbl_strlcat(needle, "  payload.bin\n", sizeof(needle)) < sizeof(needle));
    T_ASSERT(strstr(manifest, needle) != 0);

    needle[0] = '\0';
    T_ASSERT(tbl_strlcpy(needle, sha_record, sizeof(needle)) < sizeof(needle));
    T_ASSERT(tbl_strlcat(needle, "  record.ini\n", sizeof(needle)) < sizeof(needle));
    T_ASSERT(strstr(manifest, needle) != 0);

    (void)tbl_fs_rm_rf(base);

    T_OK();
}
