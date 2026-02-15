#include <stdio.h>

#define TBL_STR_IMPLEMENTATION
#include "core/str.h"

#define T_ASSERT(COND) do { \
    if (!(COND)) { \
        fprintf(stderr, "assert failed: %s (line %d)\n", #COND, __LINE__); \
        return 1; \
    } \
} while (0)

int main(void)
{
    T_ASSERT(tbl_strlen(NULL) == 0);
    T_ASSERT(tbl_strlen("") == 0);
    T_ASSERT(tbl_strlen("a") == 1);

    T_ASSERT(tbl_streq(NULL, NULL) == 1);
    T_ASSERT(tbl_streq(NULL, "") == 1);
    T_ASSERT(tbl_streq("x", "x") == 1);
    T_ASSERT(tbl_streq("x", "y") == 0);

    T_ASSERT(tbl_strneq("abc", "abd", 2) == 1);
    T_ASSERT(tbl_strneq("abc", "abd", 3) == 0);

    T_ASSERT(tbl_str_starts_with("abcdef", "abc") == 1);
    T_ASSERT(tbl_str_starts_with("abcdef", "abd") == 0);
    T_ASSERT(tbl_str_starts_with(NULL, "") == 1);

    T_ASSERT(tbl_str_ends_with("abcdef", "def") == 1);
    T_ASSERT(tbl_str_ends_with("abcdef", "deg") == 0);
    T_ASSERT(tbl_str_ends_with("a", "aa") == 0);
    T_ASSERT(tbl_str_ends_with(NULL, "") == 1);

    printf("OK\n");
    return 0;
}
