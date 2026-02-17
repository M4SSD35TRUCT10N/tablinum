/* src/core/args.c - strict C89, fail-fast on programmer errors, strict CLI parsing */

#include "core/args.h"

#include <stdio.h>
#include <string.h>

static void tbl_usage(const char *prog)
{
    if (!prog || !prog[0]) prog = TBL_NAME;

    printf("%s %s\n", TBL_NAME, TBL_VERSION);
    printf("Usage:\n");
    printf("  %s [--config FILE] [--role ROLE]\n", prog);
    printf("  %s verify  JOBID [--config FILE]\n", prog);
    printf("  %s export  JOBID OUTDIR [--config FILE]\n", prog);
    printf("\n");
    printf("Roles:\n");
    printf("  all | serve | ingest | index | worker | verify | export\n");
    printf("\n");
    printf("Options:\n");
    printf("  --config FILE        Path to INI config (default: tablinum.ini)\n");
    printf("  --role ROLE          Role to run (default: all)\n");
    printf("  --version            Print version\n");
    printf("  -h, --help           This help\n");
}

/* role names are case-sensitive on purpose (strict). */
static int tbl_role_from_str(const char *s, tbl_role_t *out)
{
    if (!s || !out) return 0;

    if (strcmp(s, "all") == 0)    { *out = TBL_ROLE_ALL; return 1; }
    if (strcmp(s, "serve") == 0)  { *out = TBL_ROLE_SERVE; return 1; }
    if (strcmp(s, "ingest") == 0) { *out = TBL_ROLE_INGEST; return 1; }
    if (strcmp(s, "index") == 0)  { *out = TBL_ROLE_INDEX; return 1; }
    if (strcmp(s, "worker") == 0) { *out = TBL_ROLE_WORKER; return 1; }
    if (strcmp(s, "verify") == 0) { *out = TBL_ROLE_VERIFY; return 1; }
    if (strcmp(s, "export") == 0) { *out = TBL_ROLE_EXPORT; return 1; }

    return 0;
}

/* Accept --key=value form (C89). Returns pointer to value or NULL if not matching. */
static const char *tbl_match_kv(const char *arg, const char *key)
{
    size_t klen;

    if (!arg || !key) return NULL;

    klen = strlen(key);
    if (strncmp(arg, key, klen) != 0) return NULL;
    if (arg[klen] != '=') return NULL;

    return arg + klen + 1;
}

static int tbl_is_option(const char *a)
{
    if (!a || !a[0]) return 0;
    return (a[0] == '-');
}

int tbl_args_parse(int argc, char **argv, tbl_app_config_t *cfg)
{
    int i;
    const char *prog;
    int got_subcmd;

    if (!cfg) {
        /* Programmer error: hard fail in debug mentality, but return error here to stay standalone. */
        fprintf(stderr, "tablinum: internal error: cfg is NULL\n");
        return 2;
    }

    cfg->config_path = "tablinum.ini";
    cfg->role = TBL_ROLE_ALL;
    cfg->jobid = NULL;
    cfg->out_dir = NULL;

    got_subcmd = 0;

    prog = (argc > 0 && argv && argv[0]) ? argv[0] : TBL_NAME;

    for (i = 1; i < argc; ++i) {
        const char *a;
        const char *v;

        a = argv[i];
        if (!a) continue;

        if (strcmp(a, "--version") == 0) {
            printf("%s %s\n", TBL_NAME, TBL_VERSION);
            return 1; /* handled */
        }

        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            tbl_usage(prog);
            return 1; /* handled */
        }

        /* end of options marker: remaining args are positional/subcommand */
        if (strcmp(a, "--") == 0) {
            i++;
            break;
        }

        /* --config=FILE */
        v = tbl_match_kv(a, "--config");
        if (v) {
            if (!v[0]) {
                fprintf(stderr, "error: --config needs a file\n");
                return 2;
            }
            cfg->config_path = v;
            continue;
        }

        /* --role=ROLE */
        v = tbl_match_kv(a, "--role");
        if (v) {
            if (!v[0]) {
                fprintf(stderr, "error: --role needs a value\n");
                return 2;
            }
            if (!tbl_role_from_str(v, &cfg->role)) {
                fprintf(stderr, "error: unknown role: %s\n", v);
                fprintf(stderr, "hint: use --help\n");
                return 2;
            }
            continue;
        }

        /* --config FILE */
        if (strcmp(a, "--config") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: --config needs a file\n");
                return 2;
            }
            i++;
            if (!argv[i] || !argv[i][0]) {
                fprintf(stderr, "error: --config needs a file\n");
                return 2;
            }
            cfg->config_path = argv[i];
            continue;
        }

        /* --role ROLE */
        if (strcmp(a, "--role") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: --role needs a value\n");
                return 2;
            }
            i++;
            if (!argv[i] || !argv[i][0]) {
                fprintf(stderr, "error: --role needs a value\n");
                return 2;
            }
            if (!tbl_role_from_str(argv[i], &cfg->role)) {
                fprintf(stderr, "error: unknown role: %s\n", argv[i]);
                fprintf(stderr, "hint: use --help\n");
                return 2;
            }
            continue;
        }

        /* Non-option token (subcommand/positional). */
        if (!tbl_is_option(a)) {
            /* allow "verify" / "export" as subcommand */
            if (!got_subcmd && (strcmp(a, "verify") == 0 || strcmp(a, "export") == 0)) {
                got_subcmd = 1;
                if (!tbl_role_from_str(a, &cfg->role)) {
                    fprintf(stderr, "error: unknown subcommand: %s\n", a);
                    return 2;
                }
                continue;
            }

            /* positional args for verify/export */
            if (cfg->role == TBL_ROLE_VERIFY) {
                if (!cfg->jobid) {
                    cfg->jobid = a;
                    continue;
                }
            } else if (cfg->role == TBL_ROLE_EXPORT) {
                if (!cfg->jobid) {
                    cfg->jobid = a;
                    continue;
                }
                if (!cfg->out_dir) {
                    cfg->out_dir = a;
                    continue;
                }
            }

            /* anything else is an error (strict) */
            fprintf(stderr, "error: unexpected positional argument: %s\n", a);
            fprintf(stderr, "hint: use --help\n");
            return 2;
        }

        /* Strict: no unknown arguments allowed */
        fprintf(stderr, "error: unknown argument: %s\n", a);
        fprintf(stderr, "hint: use --help\n");
        return 2;
    }

    /* If we broke out via "--", parse remaining as pure positional */
    for (; i < argc; ++i) {
        const char *a = argv[i];
        if (!a || !a[0]) continue;

        if (!got_subcmd && (strcmp(a, "verify") == 0 || strcmp(a, "export") == 0)) {
            got_subcmd = 1;
            if (!tbl_role_from_str(a, &cfg->role)) {
                fprintf(stderr, "error: unknown subcommand: %s\n", a);
                return 2;
            }
            continue;
        }

        if (cfg->role == TBL_ROLE_VERIFY) {
            if (!cfg->jobid) { cfg->jobid = a; continue; }
        } else if (cfg->role == TBL_ROLE_EXPORT) {
            if (!cfg->jobid) { cfg->jobid = a; continue; }
            if (!cfg->out_dir) { cfg->out_dir = a; continue; }
        }

        fprintf(stderr, "error: unexpected positional argument: %s\n", a);
        fprintf(stderr, "hint: use --help\n");
        return 2;
    }

    /* Validate required positional args */
    if (cfg->role == TBL_ROLE_VERIFY) {
        if (!cfg->jobid || !cfg->jobid[0]) {
            fprintf(stderr, "error: verify needs JOBID\n");
            fprintf(stderr, "hint: %s verify JOBID\n", prog);
            return 2;
        }
    }
    if (cfg->role == TBL_ROLE_EXPORT) {
        if (!cfg->jobid || !cfg->jobid[0] || !cfg->out_dir || !cfg->out_dir[0]) {
            fprintf(stderr, "error: export needs JOBID and OUTDIR\n");
            fprintf(stderr, "hint: %s export JOBID OUTDIR\n", prog);
            return 2;
        }
    }

    return 0;
}
