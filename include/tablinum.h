#ifndef TABLINUM_H
#define TABLINUM_H

/* Public umbrella header (lives in ./include) */

#include "core/version.h"

/* Roles */
typedef enum tbl_role_e {
    TBL_ROLE_ALL = 0,
    TBL_ROLE_SERVE,
    TBL_ROLE_INGEST,
    TBL_ROLE_INDEX,
    TBL_ROLE_WORKER,
    TBL_ROLE_VERIFY,
    TBL_ROLE_EXPORT,
    TBL_ROLE_PACKAGE
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

    /* Optional positional args (used by verify/export roles) */
    const char *jobid;       /* verify/export: job id (job directory name) */
    const char *out_dir;     /* export: output directory */

    /* package: AIP/SIP kind */
    tbl_pkg_kind_t pkg_kind;
    int pkg_kind_set; /* strict: if set but role!=package => error */
} tbl_app_config_t;

#endif /* TABLINUM_H */
