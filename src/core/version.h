#ifndef TBL_VERSION_H
#define TBL_VERSION_H

#define TBL_NAME "tablinum"

/* SemVer-ish (0.y.z): bump PATCH for incremental features/bugfixes,
   bump MINOR for larger milestones, bump MAJOR for breaking changes. */
#define TBL_VERSION_MAJOR 0
#define TBL_VERSION_MINOR 1
#define TBL_VERSION_PATCH 4

/* Pre-release tag (set to "" for release builds) */
#define TBL_VERSION_SUFFIX "-dev"

/* Convenience strings */
#define TBL_VERSION_BASE "0.1.4"
#define TBL_VERSION      TBL_VERSION_BASE TBL_VERSION_SUFFIX

#endif /* TBL_VERSION_H */
