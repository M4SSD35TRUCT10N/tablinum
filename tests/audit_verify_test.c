#include <stdio.h>
#include <string.h>

#define T_TESTNAME "audit_verify_test"
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

#define TBL_AUDIT_IMPLEMENTATION
#include "core/audit.h"

static void zero64(char out64[65])
{
    int i;
    for (i = 0; i < 64; ++i) out64[i] = '0';
    out64[64] = '\0';
}

static void hash_chain(const char prev64[65], const char *canonical, char out64[65])
{
    tbl_sha256_t st;
    unsigned char dig[32];

    tbl_sha256_init(&st);
    tbl_sha256_update(&st, prev64, 64);
    tbl_sha256_update(&st, "\n", 1);
    tbl_sha256_update(&st, canonical, strlen(canonical));
    tbl_sha256_final(&st, dig);
    (void)tbl_sha256_hex(dig, out64, 65);
}

int main(void)
{
    char root[256];
    char audit_dir[512];
    char ops_path[512];
    char err[256];

    char prev0[65];
    char h1[65];
    char h2[65];

    const char *c1;
    const char *c2;

    char buf[4096];
    size_t n;
    int rc;

    /* temp repo root */
    {
        char nbuf[32];
        unsigned long pid;
        pid = (unsigned long)tbl_fs_pid_u32();
        if (!tbl_ul_to_dec_ok(pid, nbuf, sizeof(nbuf))) {
            (void)tbl_strlcpy(nbuf, "0", sizeof(nbuf));
        }
        root[0] = '\0';
        (void)tbl_strlcpy(root, "tbl_test_audit_", sizeof(root));
        (void)tbl_strlcat(root, nbuf, sizeof(root));
    }
T_ASSERT(tbl_fs_mkdir_p(root) == 0);

    T_ASSERT(tbl_path_join2(audit_dir, sizeof(audit_dir), root, "audit"));
    T_ASSERT(tbl_fs_mkdir_p(audit_dir) == 0);
    T_ASSERT(tbl_path_join2(ops_path, sizeof(ops_path), audit_dir, "ops.log"));

    /* build two valid entries */
    zero64(prev0);

    c1 = "ts=1 event=test job=J status=ok";
    hash_chain(prev0, c1, h1);

    c2 = "ts=2 event=test job=J status=ok";
    hash_chain(h1, c2, h2);

    buf[0] = '\0';
    (void)tbl_strlcpy(buf, "prev=", sizeof(buf));
    (void)tbl_strlcat(buf, prev0, sizeof(buf));
    (void)tbl_strlcat(buf, " hash=", sizeof(buf));
    (void)tbl_strlcat(buf, h1, sizeof(buf));
    (void)tbl_strlcat(buf, " ", sizeof(buf));
    (void)tbl_strlcat(buf, c1, sizeof(buf));
    (void)tbl_strlcat(buf, "\n", sizeof(buf));

    (void)tbl_strlcat(buf, "prev=", sizeof(buf));
    (void)tbl_strlcat(buf, h1, sizeof(buf));
    (void)tbl_strlcat(buf, " hash=", sizeof(buf));
    (void)tbl_strlcat(buf, h2, sizeof(buf));
    (void)tbl_strlcat(buf, " ", sizeof(buf));
    (void)tbl_strlcat(buf, c2, sizeof(buf));
    (void)tbl_strlcat(buf, "\n", sizeof(buf));

    n = (size_t)tbl_strlen(buf);
    T_ASSERT(tbl_fs_write_file(ops_path, buf, n) == 0);

    err[0] = '\0';
    rc = tbl_audit_verify_ops(root, err, sizeof(err));
    T_ASSERT_EQ_INT(rc, 0);

    /* tamper with second canonical part, keep hash => integrity failure */
    buf[0] = '\0';
    (void)tbl_strlcpy(buf, "prev=", sizeof(buf));
    (void)tbl_strlcat(buf, prev0, sizeof(buf));
    (void)tbl_strlcat(buf, " hash=", sizeof(buf));
    (void)tbl_strlcat(buf, h1, sizeof(buf));
    (void)tbl_strlcat(buf, " ", sizeof(buf));
    (void)tbl_strlcat(buf, c1, sizeof(buf));
    (void)tbl_strlcat(buf, "\n", sizeof(buf));

    (void)tbl_strlcat(buf, "prev=", sizeof(buf));
    (void)tbl_strlcat(buf, h1, sizeof(buf));
    (void)tbl_strlcat(buf, " hash=", sizeof(buf));
    (void)tbl_strlcat(buf, h2, sizeof(buf));
    (void)tbl_strlcat(buf, " ", sizeof(buf));
    (void)tbl_strlcat(buf, "ts=2 event=test job=J status=BAD", sizeof(buf));
    (void)tbl_strlcat(buf, "\n", sizeof(buf));

    n = (size_t)tbl_strlen(buf);
    T_ASSERT(tbl_fs_write_file(ops_path, buf, n) == 0);

    err[0] = '\0';
    rc = tbl_audit_verify_ops(root, err, sizeof(err));
    T_ASSERT_EQ_INT(rc, 5);
    T_ASSERT(err[0] != '\0');

    (void)tbl_fs_rm_rf(root);

    T_OK();
}
