#ifndef TBL_CORE_ARGS_H
#define TBL_CORE_ARGS_H

#include "tablinum.h"

/* Strict CLI parsing (C89).
   - Unknown arguments are errors.
   - Supports both: role-based execution and subcommands for specific actions.
       verify JOBID
       export JOBID OUTDIR
   - Returns:
       0 = parsed OK
       1 = handled (help/version printed)
       2 = error (message already printed)
*/
int tbl_args_parse(int argc, char **argv, tbl_app_config_t *cfg);

#ifdef TBL_ARGS_IMPLEMENTATION

#include <stdio.h>
#include <string.h>

#include "core/str.h"

static void tbl_usage(const char *prog)
{
    if (!prog || !prog[0]) prog = TBL_NAME;

    printf("%s %s\n", TBL_NAME, TBL_VERSION);
    printf("Strict C89 single-binary document archive engine (paperless-style).\n");
    printf("Usage:\n");
    printf("  %s [--config FILE] [--role ROLE]\n", prog);
    printf("  %s verify  JOBID [--config FILE]\n", prog);
    printf("  %s export  JOBID OUTDIR [--config FILE]\n", prog);
    printf("  %s package JOBID OUTDIR [--format aip|sip] [--config FILE]\n", prog);
    printf("\n");
    printf("Roles:\n");
    printf("  all | serve | ingest | index | worker | verify | export | package\n");
    printf("\n");
    printf("Options:\n");
    printf("  --config FILE        Path to INI config (default: tablinum.ini)\n");
    printf("  --role ROLE          Role to run (default: all)\n");
    printf("  --format KIND        Packaging kind for 'package' (aip|sip)\n");
    printf("  --version            Print version\n");
    printf("  -h, --help           This help\n");
}

/* role names are case-sensitive on purpose (strict). */
static int tbl_role_from_str(const char *s, tbl_role_t *out)
{
    if (!s || !out) return 0;

    if (tbl_streq(s, "all"))    { *out = TBL_ROLE_ALL; return 1; }
    if (tbl_streq(s, "serve"))  { *out = TBL_ROLE_SERVE; return 1; }
    if (tbl_streq(s, "ingest")) { *out = TBL_ROLE_INGEST; return 1; }
    if (tbl_streq(s, "index"))  { *out = TBL_ROLE_INDEX; return 1; }
    if (tbl_streq(s, "worker")) { *out = TBL_ROLE_WORKER; return 1; }
    if (tbl_streq(s, "verify")) { *out = TBL_ROLE_VERIFY; return 1; }
    if (tbl_streq(s, "export")) { *out = TBL_ROLE_EXPORT; return 1; }
    if (tbl_streq(s, "package")) { *out = TBL_ROLE_PACKAGE; return 1; }

    return 0;
}

static int tbl_pkg_from_str(const char *s, tbl_pkg_kind_t *out)
{
    if (!s || !out) return 0;
    if (tbl_streq(s, "aip")) { *out = TBL_PKG_AIP; return 1; }
    if (tbl_streq(s, "sip")) { *out = TBL_PKG_SIP; return 1; }
    return 0;
}

/* Accept --key=value form (C89). Returns pointer to value or NULL if not matching. */
static const char *tbl_match_kv(const char *arg, const char *key)
{
    size_t klen;

    if (!arg || !key) return NULL;

    klen = tbl_strlen(key);
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
        /* Programmer error: hard fail mentality, but return error here to stay standalone. */
        fprintf(stderr, "tablinum: internal error: cfg is NULL\n");
        return 2;
    }

    cfg->config_path = "tablinum.ini";
    cfg->role = TBL_ROLE_ALL;
    cfg->jobid = NULL;
    cfg->out_dir = NULL;
    cfg->pkg_kind = TBL_PKG_AIP;
    cfg->pkg_kind_set = 0;

    got_subcmd = 0;
    prog = (argc > 0 && argv && argv[0]) ? argv[0] : TBL_NAME;

    /* No arguments -> show help (avoid surprising defaults). */
    if (argc <= 1) {
        tbl_usage(prog);
        return 1; /* handled */
    }

    for (i = 1; i < argc; ++i) {
        const char *a;
        const char *v;

        a = argv[i];
        if (!a) continue;

        if (tbl_streq(a, "--version")) {
            printf("%s %s\n", TBL_NAME, TBL_VERSION);
            return 1; /* handled */
        }

        if (tbl_streq(a, "-h") || tbl_streq(a, "--help")) {
            tbl_usage(prog);
            return 1; /* handled */
        }

        /* end of options marker: remaining args are positional/subcommand */
        if (tbl_streq(a, "--")) {
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

        /* --format=KIND (for 'package') */
        v = tbl_match_kv(a, "--format");
        if (v) {
            if (!v[0]) {
                fprintf(stderr, "error: --format needs a value\n");
                return 2;
            }
            if (!tbl_pkg_from_str(v, &cfg->pkg_kind)) {
                fprintf(stderr, "error: unknown format: %s\n", v);
                fprintf(stderr, "hint: use --help\n");
                return 2;
            }
            cfg->pkg_kind_set = 1;
            continue;
        }

        /* --config FILE */
        if (tbl_streq(a, "--config")) {
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
        if (tbl_streq(a, "--role")) {
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

        /* --format KIND */
        if (tbl_streq(a, "--format")) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: --format needs a value\n");
                return 2;
            }
            i++;
            if (!argv[i] || !argv[i][0]) {
                fprintf(stderr, "error: --format needs a value\n");
                return 2;
            }
            if (!tbl_pkg_from_str(argv[i], &cfg->pkg_kind)) {
                fprintf(stderr, "error: unknown format: %s\n", argv[i]);
                fprintf(stderr, "hint: use --help\n");
                return 2;
            }
            cfg->pkg_kind_set = 1;
            continue;
        }

        /* Non-option token (subcommand/positional). */
        if (!tbl_is_option(a)) {
            /* allow "verify" / "export" as subcommand */
            if (!got_subcmd && (tbl_streq(a, "verify") || tbl_streq(a, "export") || tbl_streq(a, "package"))) {
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
            } else if (cfg->role == TBL_ROLE_PACKAGE) {
                if (!cfg->jobid) { cfg->jobid = a; continue; }
                if (!cfg->out_dir) { cfg->out_dir = a; continue; }
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

        if (!got_subcmd && (tbl_streq(a, "verify") || tbl_streq(a, "export") || tbl_streq(a, "package"))) {
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
        } else if (cfg->role == TBL_ROLE_PACKAGE) {
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

    if (cfg->role == TBL_ROLE_PACKAGE) {
        if (!cfg->jobid || !cfg->jobid[0] || !cfg->out_dir || !cfg->out_dir[0]) {
            fprintf(stderr, "error: package needs JOBID and OUTDIR\n");
            fprintf(stderr, "hint: %s package JOBID OUTDIR\n", prog);
            return 2;
        }
    }

    if (cfg->pkg_kind_set && cfg->role != TBL_ROLE_PACKAGE) {
        fprintf(stderr, "error: --format is only valid with 'package'\n");
        fprintf(stderr, "hint: %s package JOBID OUTDIR --format aip|sip\n", prog);
        return 2;
    }

    return 0;
}

#endif /* TBL_ARGS_IMPLEMENTATION */

#endif /* TBL_CORE_ARGS_H */
