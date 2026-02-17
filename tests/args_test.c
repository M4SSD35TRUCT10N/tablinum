#include <string.h>

/* tack-style test output harness */
#define T_TESTNAME "args_test"
#include "test.h"

/* args implementation depends on core/str (tbl_streq/tbl_strlen). */
#define TBL_STR_IMPLEMENTATION
#include "core/str.h"

#define TBL_ARGS_IMPLEMENTATION
#include "core/args.h"

static int test_defaults(void)
{
    tbl_app_config_t app;
    char *argv0[] = { (char*)"tablinum" };
    int rc = tbl_args_parse(1, argv0, &app);

    /* No args -> help handled (behavioral polish). */
    T_ASSERT_EQ_INT(rc, 1);
    return 0;
}

static int test_explicit_all_role(void)
{
    tbl_app_config_t app;
    char *argv0[] = { (char*)"tablinum", (char*)"--role", (char*)"all" };
    int rc = tbl_args_parse(3, argv0, &app);

    T_ASSERT_EQ_INT(rc, 0);
    T_ASSERT_STREQ(app.config_path, "tablinum.ini");
    T_ASSERT_EQ_INT(app.role, TBL_ROLE_ALL);
    T_ASSERT(app.jobid == NULL);
    T_ASSERT(app.out_dir == NULL);
    return 0;
}

static int test_role_and_config(void)
{
    tbl_app_config_t app;
    char *argv1[] = {
        (char*)"tablinum",
        (char*)"--config", (char*)"x.ini",
        (char*)"--role", (char*)"ingest"
    };
    int rc = tbl_args_parse(5, argv1, &app);

    T_ASSERT_EQ_INT(rc, 0);
    T_ASSERT_STREQ(app.config_path, "x.ini");
    T_ASSERT_EQ_INT(app.role, TBL_ROLE_INGEST);
    T_ASSERT(app.jobid == NULL);
    T_ASSERT(app.out_dir == NULL);
    return 0;
}

static int test_verify_subcmd(void)
{
    tbl_app_config_t app;
    char *argv2[] = { (char*)"tablinum", (char*)"verify", (char*)"jobOK" };
    int rc = tbl_args_parse(3, argv2, &app);

    T_ASSERT_EQ_INT(rc, 0);
    T_ASSERT_EQ_INT(app.role, TBL_ROLE_VERIFY);
    T_ASSERT(app.jobid != NULL);
    T_ASSERT_STREQ(app.jobid, "jobOK");
    T_ASSERT(app.out_dir == NULL);
    return 0;
}

static int test_export_subcmd(void)
{
    tbl_app_config_t app;
    char *argv3[] = { (char*)"tablinum", (char*)"export", (char*)"jobOK", (char*)"outdir" };
    int rc = tbl_args_parse(4, argv3, &app);

    T_ASSERT_EQ_INT(rc, 0);
    T_ASSERT_EQ_INT(app.role, TBL_ROLE_EXPORT);
    T_ASSERT(app.jobid != NULL);
    T_ASSERT_STREQ(app.jobid, "jobOK");
    T_ASSERT(app.out_dir != NULL);
    T_ASSERT_STREQ(app.out_dir, "outdir");
    return 0;
}

static int test_package_subcmd_default_format(void)
{
    tbl_app_config_t app;
    char *argv4[] = { (char*)"tablinum", (char*)"package", (char*)"jobOK", (char*)"outpkg" };
    int rc = tbl_args_parse(4, argv4, &app);

    T_ASSERT_EQ_INT(rc, 0);
    T_ASSERT_EQ_INT(app.role, TBL_ROLE_PACKAGE);
    T_ASSERT(app.jobid != NULL);
    T_ASSERT_STREQ(app.jobid, "jobOK");
    T_ASSERT(app.out_dir != NULL);
    T_ASSERT_STREQ(app.out_dir, "outpkg");
    T_ASSERT_EQ_INT(app.pkg_kind, TBL_PKG_AIP);
    return 0;
}

static int test_package_subcmd_format_sip(void)
{
    tbl_app_config_t app;
    char *argv5[] = { (char*)"tablinum", (char*)"package", (char*)"jobOK", (char*)"outpkg", (char*)"--format", (char*)"sip" };
    int rc = tbl_args_parse(6, argv5, &app);

    T_ASSERT_EQ_INT(rc, 0);
    T_ASSERT_EQ_INT(app.role, TBL_ROLE_PACKAGE);
    T_ASSERT_EQ_INT(app.pkg_kind, TBL_PKG_SIP);
    return 0;
}

int main(void)
{
    T_ASSERT(test_defaults() == 0);
    T_ASSERT(test_explicit_all_role() == 0);
    T_ASSERT(test_role_and_config() == 0);
    T_ASSERT(test_verify_subcmd() == 0);
    T_ASSERT(test_export_subcmd() == 0);
    T_ASSERT(test_package_subcmd_default_format() == 0);
    T_ASSERT(test_package_subcmd_format_sip() == 0);
    T_OK();
}
