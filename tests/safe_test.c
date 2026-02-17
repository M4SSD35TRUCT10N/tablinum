#include <string.h>
#define T_TESTNAME "safe_test"
#include "test.h"


#define TBL_SAFE_IMPLEMENTATION
#include "core/safe.h"

int main(void)
{
    char buf[8];
    unsigned long v;

    buf[0] = '\0';
    (void)tbl_strlcpy(buf, "abcdef", sizeof(buf));
    T_ASSERT(strcmp(buf, "abcdef") == 0);

    (void)tbl_strlcpy(buf, "0123456789", sizeof(buf));
    T_ASSERT(strlen(buf) == (sizeof(buf) - 1));

    (void)tbl_strlcpy(buf, "aa", sizeof(buf));
    (void)tbl_strlcat(buf, "bb", sizeof(buf));
    T_ASSERT(strcmp(buf, "aabb") == 0);

    T_ASSERT(tbl_parse_u32("4294967295", &v) == 1);
    T_ASSERT(v == 4294967295UL);
    T_ASSERT(tbl_parse_u32("4294967296", &v) == 0);
    T_ASSERT(tbl_parse_u32("12x", &v) == 0);
    T_ASSERT(tbl_parse_u32("", &v) == 0);

        T_OK();
}
