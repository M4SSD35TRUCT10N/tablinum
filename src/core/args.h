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
#include "core/safe.h"

static void tbl_usage(const char *prog)
{
    if (!prog || !prog[0]) prog = TBL_NAME;

    (void)tbl_fputs4_ok(stdout, TBL_NAME, " ", TBL_VERSION, "\n");
    (void)tbl_fputs_ok(stdout, "Strict C89 single-binary document archive engine (paperless-style).\n");
    (void)tbl_fputs_ok(stdout, "Usage:\n");
    (void)tbl_fputs3_ok(stdout, "  ", prog, " [--config FILE] [--role ROLE]\n");
    (void)tbl_fputs3_ok(stdout, "  ", prog, " verify  JOBID [--config FILE]\n");
    (void)tbl_fputs3_ok(stdout, "  ", prog, " export  JOBID OUTDIR [--config FILE]\n");
    (void)tbl_fputs3_ok(stdout, "  ", prog, " verify-package PKGDIR\n");
    (void)tbl_fputs3_ok(stdout, "  ", prog, " ingest-package PKGDIR [--config FILE]\n");
    (void)tbl_fputs3_ok(stdout, "  ", prog, " verify-audit [--config FILE]\n");
    (void)tbl_fputs3_ok(stdout, "  ", prog, " package JOBID OUTDIR [--format aip|sip] [--config FILE]\n");
    (void)tbl_fputs_ok(stdout, "\n");
    (void)tbl_fputs_ok(stdout, "Roles:\n");
    (void)tbl_fputs_ok(stdout, "  all | serve | ingest | index | worker | verify | export | package | verify-package | ingest-package | verify-audit\n");
    (void)tbl_fputs_ok(stdout, "\n");
    (void)tbl_fputs_ok(stdout, "Options:\n");
    (void)tbl_fputs_ok(stdout, "  --config FILE        Path to INI config (default: tablinum.ini)\n");
    (void)tbl_fputs_ok(stdout, "  --role ROLE          Role to run (default: all)\n");
    (void)tbl_fputs_ok(stdout, "  --format KIND        Packaging kind for 'package' (aip|sip)\n");
    (void)tbl_fputs_ok(stdout, "  --version            Print version\n");
    (void)tbl_fputs_ok(stdout, "  -h, --help           This help\n");
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
    if (tbl_streq(s, "verify-package")) { *out = TBL_ROLE_VERIFY_PACKAGE; return 1; }
    if (tbl_streq(s, "ingest-package")) { *out = TBL_ROLE_INGEST_PACKAGE; return 1; }
    if (tbl_streq(s, "verify-audit")) { *out = TBL_ROLE_VERIFY_AUDIT; return 1; }
    if (tbl_streq(s, "audit-verify")) { *out = TBL_ROLE_VERIFY_AUDIT; return 1; } /* alias */

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
        (void)tbl_fputs_ok(stderr, "tablinum: internal error: cfg is NULL\n");
        return 2;
    }

    cfg->config_path = "tablinum.ini";
    cfg->role = TBL_ROLE_ALL;
    cfg->jobid = NULL;
    cfg->out_dir = NULL;
    cfg->pkg_dir = NULL;
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
            (void)tbl_fputs4_ok(stdout, TBL_NAME, " ", TBL_VERSION, "\n");
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
                (void)tbl_fputs_ok(stderr, "error: --config needs a file\n");
                return 2;
            }
            cfg->config_path = v;
            continue;
        }

        /* --role=ROLE */
        v = tbl_match_kv(a, "--role");
        if (v) {
            if (!v[0]) {
                (void)tbl_fputs_ok(stderr, "error: --role needs a value\n");
                return 2;
            }
            if (!tbl_role_from_str(v, &cfg->role)) {
                (void)tbl_fputs3_ok(stderr, "error: unknown role: ", v, "\n");
                (void)tbl_fputs_ok(stderr, "hint: use --help\n");
                return 2;
            }
            continue;
        }

        /* --format=KIND (for 'package') */
        v = tbl_match_kv(a, "--format");
        if (v) {
            if (!v[0]) {
                (void)tbl_fputs_ok(stderr, "error: --format needs a value\n");
                return 2;
            }
            if (!tbl_pkg_from_str(v, &cfg->pkg_kind)) {
                (void)tbl_fputs3_ok(stderr, "error: unknown format: ", v, "\n");
                (void)tbl_fputs_ok(stderr, "hint: use --help\n");
                return 2;
            }
            cfg->pkg_kind_set = 1;
            continue;
        }

        /* --config FILE */
        if (tbl_streq(a, "--config")) {
            if (i + 1 >= argc) {
                (void)tbl_fputs_ok(stderr, "error: --config needs a file\n");
                return 2;
            }
            i++;
            if (!argv[i] || !argv[i][0]) {
                (void)tbl_fputs_ok(stderr, "error: --config needs a file\n");
                return 2;
            }
            cfg->config_path = argv[i];
            continue;
        }

        /* --role ROLE */
        if (tbl_streq(a, "--role")) {
            if (i + 1 >= argc) {
                (void)tbl_fputs_ok(stderr, "error: --role needs a value\n");
                return 2;
            }
            i++;
            if (!argv[i] || !argv[i][0]) {
                (void)tbl_fputs_ok(stderr, "error: --role needs a value\n");
                return 2;
            }
            if (!tbl_role_from_str(argv[i], &cfg->role)) {
                (void)tbl_fputs3_ok(stderr, "error: unknown role: ", argv[i], "\n");
                (void)tbl_fputs_ok(stderr, "hint: use --help\n");
                return 2;
            }
            continue;
        }

        /* --format KIND */
        if (tbl_streq(a, "--format")) {
            if (i + 1 >= argc) {
                (void)tbl_fputs_ok(stderr, "error: --format needs a value\n");
                return 2;
            }
            i++;
            if (!argv[i] || !argv[i][0]) {
                (void)tbl_fputs_ok(stderr, "error: --format needs a value\n");
                return 2;
            }
            if (!tbl_pkg_from_str(argv[i], &cfg->pkg_kind)) {
                (void)tbl_fputs3_ok(stderr, "error: unknown format: ", argv[i], "\n");
                (void)tbl_fputs_ok(stderr, "hint: use --help\n");
                return 2;
            }
            cfg->pkg_kind_set = 1;
            continue;
        }

        /* Non-option token (subcommand/positional). */
        if (!tbl_is_option(a)) {
            /* allow "verify" / "export" as subcommand */
            if (!got_subcmd && (tbl_streq(a, "verify") || tbl_streq(a, "export") || tbl_streq(a, "package") || tbl_streq(a, "verify-package") || tbl_streq(a, "ingest-package") || tbl_streq(a, "verify-audit") || tbl_streq(a, "audit-verify"))) {
                got_subcmd = 1;
                if (!tbl_role_from_str(a, &cfg->role)) {
                    (void)tbl_fputs3_ok(stderr, "error: unknown subcommand: ", a, "\n");
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
            } else if (cfg->role == TBL_ROLE_VERIFY_PACKAGE) {
                if (!cfg->pkg_dir) { cfg->pkg_dir = a; continue; }
            } else if (cfg->role == TBL_ROLE_INGEST_PACKAGE) {
                if (!cfg->pkg_dir) { cfg->pkg_dir = a; continue; }
            }

            /* anything else is an error (strict) */
            (void)tbl_fputs3_ok(stderr, "error: unexpected positional argument: ", a, "\n");
                (void)tbl_fputs_ok(stderr, "hint: use --help\n");
            return 2;
        }

        /* Strict: no unknown arguments allowed */
        (void)tbl_fputs3_ok(stderr, "error: unknown argument: ", a, "\n");
                (void)tbl_fputs_ok(stderr, "hint: use --help\n");
        return 2;
    }

    /* If we broke out via "--", parse remaining as pure positional */
    for (; i < argc; ++i) {
        const char *a = argv[i];
        if (!a || !a[0]) continue;

        if (!got_subcmd && (tbl_streq(a, "verify") || tbl_streq(a, "export") || tbl_streq(a, "package") || tbl_streq(a, "verify-package") || tbl_streq(a, "ingest-package") || tbl_streq(a, "verify-audit") || tbl_streq(a, "audit-verify"))) {
            got_subcmd = 1;
            if (!tbl_role_from_str(a, &cfg->role)) {
                (void)tbl_fputs3_ok(stderr, "error: unknown subcommand: ", a, "\n");
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
        } else if (cfg->role == TBL_ROLE_VERIFY_PACKAGE) {
            if (!cfg->pkg_dir) { cfg->pkg_dir = a; continue; }
        } else if (cfg->role == TBL_ROLE_INGEST_PACKAGE) {
            if (!cfg->pkg_dir) { cfg->pkg_dir = a; continue; }
        }

        (void)tbl_fputs3_ok(stderr, "error: unexpected positional argument: ", a, "\n");
                (void)tbl_fputs_ok(stderr, "hint: use --help\n");
        return 2;
    }

    /* Validate required positional args */
    if (cfg->role == TBL_ROLE_VERIFY) {
        if (!cfg->jobid || !cfg->jobid[0]) {
            (void)tbl_fputs_ok(stderr, "error: verify needs JOBID\n");
            (void)tbl_fputs3_ok(stderr, "hint: ", prog, " verify JOBID\n");
            return 2;
        }
    }
    if (cfg->role == TBL_ROLE_EXPORT) {
        if (!cfg->jobid || !cfg->jobid[0] || !cfg->out_dir || !cfg->out_dir[0]) {
            (void)tbl_fputs_ok(stderr, "error: export needs JOBID and OUTDIR\n");
            (void)tbl_fputs3_ok(stderr, "hint: ", prog, " export JOBID OUTDIR\n");
            return 2;
        }
    }

    if (cfg->role == TBL_ROLE_PACKAGE) {
        if (!cfg->jobid || !cfg->jobid[0] || !cfg->out_dir || !cfg->out_dir[0]) {
            (void)tbl_fputs_ok(stderr, "error: package needs JOBID and OUTDIR\n");
            (void)tbl_fputs3_ok(stderr, "hint: ", prog, " package JOBID OUTDIR\n");
            return 2;
        }
    }

    if (cfg->role == TBL_ROLE_VERIFY_PACKAGE) {
        if (!cfg->pkg_dir || !cfg->pkg_dir[0]) {
            (void)tbl_fputs_ok(stderr, "error: verify-package needs PKGDIR\n");
            (void)tbl_fputs3_ok(stderr, "hint: ", prog, " verify-package PKGDIR\n");
            return 2;
        }
    }

    if (cfg->role == TBL_ROLE_INGEST_PACKAGE) {
        if (!cfg->pkg_dir || !cfg->pkg_dir[0]) {
            (void)tbl_fputs_ok(stderr, "error: ingest-package needs PKGDIR\n");
            (void)tbl_fputs3_ok(stderr, "hint: ", prog, " ingest-package PKGDIR --config PATH\n");
            return 2;
        }
    }

    if (cfg->role == TBL_ROLE_VERIFY_AUDIT) {
        /* no positional args */
    }

    if (cfg->pkg_kind_set && cfg->role != TBL_ROLE_PACKAGE) {
        (void)tbl_fputs_ok(stderr, "error: --format is only valid with 'package'\n");
        (void)tbl_fputs3_ok(stderr, "hint: ", prog, " package JOBID OUTDIR --format aip|sip\n");
            return 2;
    }

    return 0;
}

#endif /* TBL_ARGS_IMPLEMENTATION */

#endif /* TBL_CORE_ARGS_H */
