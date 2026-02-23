#define T_TESTNAME "sha256_test"
#include "test.h"


#define TBL_SAFE_IMPLEMENTATION
#include "core/safe.h"

#define TBL_STR_IMPLEMENTATION
#include "core/str.h"

#define TBL_SHA256_IMPLEMENTATION
#include "core/sha256.h"

static int hash_str(const char *s, char *hex, size_t hexsz)
{
    tbl_sha256_t st;
    unsigned char dig[32];

    tbl_sha256_init(&st);
    if (s) {
        tbl_sha256_update(&st, s, tbl_strlen(s));
    } else {
        tbl_sha256_update(&st, "", 0);
    }
    tbl_sha256_final(&st, dig);
    return tbl_sha256_hex_ok(dig, hex, hexsz);
}

int main(void)
{
    char hex[65];

    /* "" */
    T_ASSERT(hash_str("", hex, sizeof(hex)) == 1);
    T_ASSERT(tbl_streq(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == 1);

    /* "abc" */
    T_ASSERT(hash_str("abc", hex, sizeof(hex)) == 1);
    T_ASSERT(tbl_streq(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 1);

        T_OK();
}
