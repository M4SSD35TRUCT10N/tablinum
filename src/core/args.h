#ifndef TBL_CORE_ARGS_H
#define TBL_CORE_ARGS_H

#include "tablinum.h"

/* returns 0 on success, nonzero on error; may print usage on --help */
int tbl_args_parse(int argc, char **argv, tbl_app_config_t *cfg);

#endif /* TBL_CORE_ARGS_H */
