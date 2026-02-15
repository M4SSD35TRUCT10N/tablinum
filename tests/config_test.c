#include <stdio.h>
#include <string.h>

#define TBL_SAFE_IMPLEMENTATION
#include "core/safe.h"

#define TBL_INI_IMPLEMENTATION
#include "core/ini.h"

#define TBL_CONFIG_IMPLEMENTATION
#include "core/config.h"

#define T_ASSERT(COND) do { \
    if (!(COND)) { \
        fprintf(stderr, "assert failed: %s (line %d)\n", #COND, __LINE__); \
        return 1; \
    } \
} while (0)

int main(void)
{
    const char *ini;
    char err[256];
    tbl_cfg_t cfg;
    int rc;

    ini =
        "[core]\n"
        "root = /srv/tab\n"
        "spool = /srv/tab/spool\n"
        "repo = /srv/tab/repo\n"
        "db = /srv/tab/db/tablinum.sqlite\n"
        "\n"
        "[http]\n"
        "listen = 127.0.0.1:8080\n"
        "\n"
        "[ingest]\n"
        "poll_seconds = 2\n";

    err[0] = '\0';
    rc = tbl_cfg_load_buf(&cfg, ini, (size_t)strlen(ini), err, sizeof(err));
    T_ASSERT(rc == 0);
    T_ASSERT(strcmp(cfg.root, "/srv/tab") == 0);
    T_ASSERT(strcmp(cfg.http_listen, "127.0.0.1:8080") == 0);
    T_ASSERT(cfg.ingest_poll_seconds == 2UL);

    /* unknown key must fail */
    ini =
        "[core]\n"
        "nope = 1\n";
    err[0] = '\0';
    rc = tbl_cfg_load_buf(&cfg, ini, (size_t)strlen(ini), err, sizeof(err));
    T_ASSERT(rc != 0);
    T_ASSERT(err[0] != '\0');

    /* invalid poll_seconds must fail */
    ini =
        "[ingest]\n"
        "poll_seconds = x\n";
    err[0] = '\0';
    rc = tbl_cfg_load_buf(&cfg, ini, (size_t)strlen(ini), err, sizeof(err));
    T_ASSERT(rc != 0);
    T_ASSERT(err[0] != '\0');

    printf("OK\n");
    return 0;
}
