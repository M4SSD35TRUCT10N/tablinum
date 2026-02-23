#ifndef TBL_VERSION_H
#define TBL_VERSION_H

#define TBL_NAME "tablinum"

/* SemVer-ish (0.y.z): bump PATCH for incremental features/bugfixes,
   bump MINOR for larger milestones, bump MAJOR for breaking changes. */
#define TBL_VERSION_MAJOR 0
#define TBL_VERSION_MINOR 2
#define TBL_VERSION_PATCH 0

/* Pre-release tag (set to "" for release builds).
   You may override from the build system via -DTBL_VERSION_SUFFIX="\"-dev\"" etc. */
#ifndef TBL_VERSION_SUFFIX
#define TBL_VERSION_SUFFIX "-dev"
#endif

/* Optional build metadata (SemVer build: "+..."), e.g. "+g<hash>".
   Override via -DTBL_BUILD_META="\"+gabcdef0\"" or leave empty. */
#ifndef TBL_BUILD_META
#define TBL_BUILD_META ""
#endif

/* stringify helpers */
#define TBL__STR(x) #x
#define TBL__XSTR(x) TBL__STR(x)

/* Convenience strings */
#define TBL_VERSION_BASE TBL__XSTR(TBL_VERSION_MAJOR) "." TBL__XSTR(TBL_VERSION_MINOR) "." TBL__XSTR(TBL_VERSION_PATCH)
#define TBL_VERSION      TBL_VERSION_BASE TBL_VERSION_SUFFIX TBL_BUILD_META

#endif /* TBL_VERSION_H */
