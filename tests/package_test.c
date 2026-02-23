#include <stdio.h>
#include <string.h>

#define T_TESTNAME "package_test"
#include "test.h"

#define TBL_SAFE_IMPLEMENTATION
#include "core/safe.h"

#define TBL_STR_IMPLEMENTATION
#include "core/str.h"

#define TBL_PATH_IMPLEMENTATION
#include "core/path.h"

#define TBL_SHA256_IMPLEMENTATION
#include "core/sha256.h"

#define TBL_FS_IMPLEMENTATION
#include "os/fs.h"

#define TBL_INI_IMPLEMENTATION
#include "core/ini.h"

#define TBL_RECORD_IMPLEMENTATION
#include "core/record.h"

#define TBL_CAS_IMPLEMENTATION
#include "core/cas.h"

#define TBL_EVENTS_IMPLEMENTATION
#include "core/events.h"

#define TBL_PACKAGE_IMPLEMENTATION
#include "core/package.h"

#define TBL_PKGVERIFY_IMPLEMENTATION
#include "core/pkgverify.h"

static int read_file(const char *path, char *buf, size_t bufsz)
{
    FILE *fp;
    size_t n;

    if (!path || !path[0] || !buf || bufsz == 0) return 1;
    buf[0] = '\0';

    fp = fopen(path, "rb");
    if (!fp) return 1;

    n = fread(buf, 1, bufsz - 1, fp);
    buf[n] = '\0';
    fclose(fp);
    return 0;
}

int main(void)
{
    char root[256];
    char repo_root[512];
    char pkg_dir[512];
    char payload_src[512];
    char meta_dir[512];
    char package_ini[768];
    char events_log[768];

    const unsigned char payload_bytes[] = "hello tablinum\n";
    char sha[65];
    char err[256];
    int rc;

    tbl_record_t rec;
    char buf[4096];

    {
        char nbuf[32];
        unsigned long pid;
        pid = (unsigned long)tbl_fs_pid_u32();
        if (!tbl_ul_to_dec_ok(pid, nbuf, sizeof(nbuf))) {
            (void)tbl_strlcpy(nbuf, "0", sizeof(nbuf));
        }
        root[0] = '\0';
        (void)tbl_strlcpy(root, "tbl_test_pkg_", sizeof(root));
        (void)tbl_strlcat(root, nbuf, sizeof(root));
    }
T_ASSERT(tbl_fs_mkdir_p(root) == 0);

    T_ASSERT(tbl_path_join2(repo_root, sizeof(repo_root), root, "repo"));
    T_ASSERT(tbl_path_join2(pkg_dir, sizeof(pkg_dir), root, "pkg"));
    T_ASSERT(tbl_path_join2(payload_src, sizeof(payload_src), root, "payload.bin"));

    T_ASSERT(tbl_fs_mkdir_p(repo_root) == 0);

    /* create payload source */
    T_ASSERT(tbl_fs_write_file(payload_src, payload_bytes, sizeof(payload_bytes) - 1) == 0);

    /* put into CAS */
    err[0] = '\0';
    sha[0] = '\0';
    rc = tbl_cas_put_file(repo_root, payload_src, sha, sizeof(sha), err, sizeof(err));
    T_ASSERT(rc == 0);
    T_ASSERT(tbl_strlen(sha) == 64);

    /* write durable record */
    memset(&rec, 0, sizeof(rec));
    (void)tbl_strlcpy(rec.job, "job1", sizeof(rec.job));
    (void)tbl_strlcpy(rec.status, "ok", sizeof(rec.status));
    (void)tbl_strlcpy(rec.payload, "payload.bin", sizeof(rec.payload));
    (void)tbl_strlcpy(rec.sha256, sha, sizeof(rec.sha256));
    rec.bytes = (unsigned long)(sizeof(payload_bytes) - 1);
    rec.stored_at = 1234UL;
    rec.reason[0] = '\0';

    err[0] = '\0';
    rc = tbl_record_write_repo(repo_root, &rec, err, sizeof(err));
    T_ASSERT(rc == 0);

    /* create an event (produces per-job events + ops audit as best effort) */
    err[0] = '\0';
    (void)tbl_events_append(repo_root, "ingest.ok", rec.job, "ok", rec.sha256, "", err, sizeof(err));

    /* build package */
    err[0] = '\0';
    rc = tbl_package_job(repo_root, rec.job, pkg_dir, TBL_PKG_AIP, err, sizeof(err));
    T_ASSERT(rc == 0);

    /* strict verify-package */
    err[0] = '\0';
    rc = tbl_verify_package_dir(pkg_dir, err, sizeof(err));
    T_ASSERT_EQ_INT(rc, 0);

    /* check package.ini contains events_source=job and tool_version */
    T_ASSERT(tbl_path_join2(meta_dir, sizeof(meta_dir), pkg_dir, "metadata"));
    T_ASSERT(tbl_path_join2(package_ini, sizeof(package_ini), meta_dir, "package.ini"));
    T_ASSERT(read_file(package_ini, buf, sizeof(buf)) == 0);
    T_ASSERT(strstr(buf, "events_source = job") != NULL);
    T_ASSERT(strstr(buf, "tool_version = ") != NULL);

    /* check events.log contains our event (job stream preferred) */
    T_ASSERT(tbl_path_join2(events_log, sizeof(events_log), meta_dir, "events.log"));
    T_ASSERT(read_file(events_log, buf, sizeof(buf)) == 0);
    T_ASSERT(strstr(buf, "event=ingest.ok") != NULL);

    (void)tbl_fs_rm_rf(root);

    T_OK();
}
