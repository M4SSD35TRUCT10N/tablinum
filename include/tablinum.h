#ifndef TABLINUM_H
#define TABLINUM_H

/* Public umbrella header (lives in ./include) */

#include "core/version.h"

/* Stable exit codes (OpenBSD-style). */
#define TBL_EXIT_OK        0
#define TBL_EXIT_USAGE     2
#define TBL_EXIT_NOTFOUND  3
#define TBL_EXIT_IO        4
#define TBL_EXIT_INTEGRITY 5
#define TBL_EXIT_SCHEMA    6

/* Roles */
typedef enum tbl_role_e {
    TBL_ROLE_ALL = 0,
    TBL_ROLE_SERVE,
    TBL_ROLE_INGEST,
    TBL_ROLE_INDEX,
    TBL_ROLE_WORKER,
    TBL_ROLE_VERIFY,
    TBL_ROLE_EXPORT,
    TBL_ROLE_PACKAGE,
    TBL_ROLE_VERIFY_PACKAGE,
    TBL_ROLE_INGEST_PACKAGE,
    TBL_ROLE_VERIFY_AUDIT
} tbl_role_t;

/* Packaging kinds (E-ARK inspired, OAIS-light). */
typedef enum tbl_pkg_kind_e {
    TBL_PKG_AIP = 0,
    TBL_PKG_SIP
} tbl_pkg_kind_t;

/* CLI/Runtime config (loaded by args.c, then INI via core/config.c) */
typedef struct tbl_app_config_s {
    const char *config_path; /* INI path */
    tbl_role_t role;         /* selected role */

    /* Optional positional args */
    const char *jobid;       /* verify/export/package: job id (job directory name) */
    const char *out_dir;     /* export/package: output directory */
    const char *pkg_dir;     /* verify-package/ingest-package: package directory */

    /* package: AIP/SIP kind */
    tbl_pkg_kind_t pkg_kind;
    int pkg_kind_set; /* strict: if set but role!=package => error */
} tbl_app_config_t;

#endif /* TABLINUM_H */
