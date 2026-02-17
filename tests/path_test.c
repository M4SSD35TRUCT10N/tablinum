#include <string.h>
#define T_TESTNAME "path_test"
#include "test.h"


#define TBL_SAFE_IMPLEMENTATION
#include "core/safe.h"

#define TBL_PATH_IMPLEMENTATION
#include "core/path.h"

static void make_expected_ab(char *out, size_t outsz, char sep)
{
    /* "a<sep>b" */
    if (!out || outsz < 4) return;
    out[0] = 'a';
    out[1] = sep;
    out[2] = 'b';
    out[3] = '\0';
}

int main(void)
{
    char p[64];
    char exp[8];
    char sep;

    sep = tbl_path_sep();
    make_expected_ab(exp, sizeof(exp), sep);

    T_ASSERT(tbl_path_join2(p, sizeof(p), "a", "b") == 1);
    T_ASSERT(strcmp(p, exp) == 0);

    T_ASSERT(tbl_path_join2(p, sizeof(p), "a/", "/b") == 1);
    T_ASSERT(strcmp(p, exp) == 0);

    T_ASSERT(tbl_path_join2(p, sizeof(p), "a\\", "\\b") == 1);
    T_ASSERT(strcmp(p, exp) == 0);

#ifdef _WIN32
    T_ASSERT(tbl_path_is_abs("C:\\x") == 1);
    T_ASSERT(tbl_path_is_abs("\\x") == 1);
    T_ASSERT(tbl_path_is_abs("/x") == 1);
    T_ASSERT(tbl_path_is_abs("x\\y") == 0);
#else
    T_ASSERT(tbl_path_is_abs("/x") == 1);
    T_ASSERT(tbl_path_is_abs("x/y") == 0);
#endif

        T_OK();
}
