/* docs_bilingual_test.c - documentation bilingual + LF-only sanity check
 * Reference v1: documents MUST provide German + English sections and be LF-only.
 * Pillars: strict C89/ANSI-C, fail-fast, OpenBSD-safety, header-implementations.
 * This test avoids format-stdio; use safe helpers for output.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* tack-style test output harness */
#define T_TESTNAME "docs_bilingual_test"
#include "test.h"

#ifndef TBL_ARRAY_LEN
#define TBL_ARRAY_LEN(a) (sizeof(a)/sizeof((a)[0]))
#endif

struct tbl_doc_item {
    const char *name;      /* logical name for messages */
    const char *primary;   /* preferred path */
    const char *fallback;  /* optional fallback path (may be NULL) */
};

static int read_all(const char *path, char **out_buf, size_t *out_len)
{
    FILE *fp;
    long szl;
    size_t sz;
    char *buf;

    if (!path || !out_buf || !out_len) return 0;

    fp = fopen(path, "rb");
    if (!fp) return 0;

    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return 0; }
    szl = ftell(fp);
    if (szl < 0) { fclose(fp); return 0; }

    /* docs should be small; keep bounded */
    if (szl > (long)(2u * 1024u * 1024u)) { fclose(fp); return 0; }

    sz = (size_t)szl;
    if (fseek(fp, 0, SEEK_SET) != 0) { fclose(fp); return 0; }

    buf = (char*)malloc(sz + 1u);
    if (!buf) { fclose(fp); return 0; }

    if (sz > 0) {
        size_t got = fread(buf, 1u, sz, fp);
        if (got != sz) { free(buf); fclose(fp); return 0; }
    }

    buf[sz] = '\0';
    fclose(fp);

    *out_buf = buf;
    *out_len = sz;
    return 1;
}

static int has_cr(const char *buf, size_t len)
{
    size_t i;
    if (!buf) return 0;
    for (i = 0; i < len; ++i) if (buf[i] == '\r') return 1;
    return 0;
}

static int has_bilingual_markers(const char *buf)
{
    if (!buf) return 0;

    if (strstr(buf, "## Deutsch (DE)") && strstr(buf, "## English (EN)")) return 1;

    /* fallback inline markers */
    if (strstr(buf, "**DE:**") && strstr(buf, "**EN:**")) return 1;

    return 0;
}

static int de_before_en_if_headings(const char *buf)
{
    const char *de, *en;
    if (!buf) return 1;

    de = strstr(buf, "## Deutsch (DE)");
    en = strstr(buf, "## English (EN)");

    /* only enforce order if both headings exist */
    if (!de || !en) return 1;

    return (de < en) ? 1 : 0;
}

static int check_one(const struct tbl_doc_item *it)
{
    char *buf = 0;
    size_t len = 0;
    const char *path = 0;

    if (!it) return 0;

    if (read_all(it->primary, &buf, &len)) {
        path = it->primary;
    } else if (it->fallback && read_all(it->fallback, &buf, &len)) {
        path = it->fallback;
    } else {
        T_FAIL("missing file (see test list)");
    }

    if (has_cr(buf, len)) {
        t_puts(stderr, T_TESTNAME); t_puts(stderr, ": CR found (not LF-only): ");
        t_puts(stderr, path); t_putc(stderr, '\n');
        free(buf);
        return 1;
    }

    if (!has_bilingual_markers(buf)) {
        t_puts(stderr, T_TESTNAME); t_puts(stderr, ": missing DE/EN markers: ");
        t_puts(stderr, path); t_putc(stderr, '\n');
        free(buf);
        return 1;
    }

    if (!de_before_en_if_headings(buf)) {
        t_puts(stderr, T_TESTNAME); t_puts(stderr, ": DE must precede EN: ");
        t_puts(stderr, path); t_putc(stderr, '\n');
        free(buf);
        return 1;
    }

    free(buf);
    (void)path;
    return 0;
}

int main(void)
{
    size_t i;

    /* Keep this list small + explicit (portable across Windows/Linux/9front). */
    static const struct tbl_doc_item docs[] = {
        { "README.md",            "README.md",            0 },
        { "RELEASENOTES.md",      "RELEASENOTES.md",      0 },
        { "CHANGELOG.md",         "CHANGELOG.md",         0 },
        { "RELEASING.md",         "RELEASING.md",         "docs/RELEASING.md" },

        { "docs/AUDIT.md",        "docs/AUDIT.md",        "AUDIT.md" },
        { "docs/CODING.md",       "docs/CODING.md",       "CODING.md" },
        { "docs/PACKAGING.md",    "docs/PACKAGING.md",    "PACKAGING.md" },
        { "docs/REFERENCE.md",    "docs/REFERENCE.md",    "REFERENCE.md" }
    };

    for (i = 0; i < TBL_ARRAY_LEN(docs); ++i) {
        int rc = check_one(&docs[i]);
        if (rc != 0) return 1; /* fail-fast */
    }

    T_OK();
}
