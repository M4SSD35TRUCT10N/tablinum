#include <stdio.h>
#include <string.h>

#define TBL_SAFE_IMPLEMENTATION
#include "core/safe.h"

#define TBL_INI_IMPLEMENTATION
#include "core/ini.h"

#define T_ASSERT(COND) do { \
    if (!(COND)) { \
        fprintf(stderr, "assert failed: %s (line %d)\n", #COND, __LINE__); \
        return 1; \
    } \
} while (0)

typedef struct st_s {
    int step;
} st_t;

static int on_kv(void *ud, const char *section, const char *key, const char *value, int line_no)
{
    st_t *st;
    (void)line_no;
    st = (st_t *)ud;

    if (st->step == 0) {
        T_ASSERT(strcmp(section, "core") == 0);
        T_ASSERT(strcmp(key, "root") == 0);
        T_ASSERT(strcmp(value, "/srv/tablinum") == 0);
        st->step++;
        return 0;
    }
    if (st->step == 1) {
        T_ASSERT(strcmp(section, "target \"app\".debug") == 0);
        T_ASSERT(strcmp(key, "defines") == 0);
        T_ASSERT(strcmp(value, "X=1") == 0);
        st->step++;
        return 0;
    }

    return 1; /* unexpected extra kv */
}

int main(void)
{
    const char *ini;
    char err[256];
    st_t st;
    int rc;

    ini =
        "; comment\n"
        "[core]\n"
        "  root = /srv/tablinum   \n"
        "\n"
        "[target \"app\".debug]\n"
        "defines = X=1\n";

    st.step = 0;
    err[0] = '\0';

    rc = tbl_ini_parse_buf(ini, (size_t)strlen(ini), on_kv, &st, err, sizeof(err));
    T_ASSERT(rc == TBL_INI_OK);
    T_ASSERT(st.step == 2);

    /* invalid line */
    ini = "[core]\nnoequals\n";
    err[0] = '\0';
    rc = tbl_ini_parse_buf(ini, (size_t)strlen(ini), on_kv, &st, err, sizeof(err));
    T_ASSERT(rc != TBL_INI_OK);
    T_ASSERT(err[0] != '\0');

    printf("OK\n");
    return 0;
}
