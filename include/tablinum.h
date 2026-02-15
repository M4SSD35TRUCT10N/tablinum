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
    TBL_ROLE_WORKER
} tbl_role_t;

/* Runtime config (later: loaded from INI) */
typedef struct tbl_app_config_s {
    const char *config_path; /* INI path */
    tbl_role_t role;         /* selected role */
} tbl_app_config_t;

#endif /* TABLINUM_H */
