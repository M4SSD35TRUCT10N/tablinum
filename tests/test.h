#ifndef TBL_TEST_H
#define TBL_TEST_H

#include <stdio.h>
#include <string.h>

/* Define T_TESTNAME before including this header, otherwise it falls back to __FILE__. */
#ifndef T_TESTNAME
#define T_TESTNAME __FILE__
#endif

/* --- output helpers --- */

#define T_TRACE(MSG) do { \
    fprintf(stderr, "%s: %s\n", T_TESTNAME, (MSG)); \
} while (0)

#define T_FAIL(MSG) do { \
    fprintf(stderr, "%s: %s\n", T_TESTNAME, (MSG)); \
    return 1; \
} while (0)

/* --- assertions --- */

#define T_ASSERT(COND) do { \
    if (!(COND)) { \
        fprintf(stderr, "%s: assert failed: %s (line %d)\n", T_TESTNAME, #COND, __LINE__); \
        return 1; \
    } \
} while (0)

#define T_ASSERT_EQ_INT(A,B) do { \
    int _a = (A); \
    int _b = (B); \
    if (_a != _b) { \
        fprintf(stderr, "%s: assert failed: %s == %s (got %d, expected %d) (line %d)\n", \
                T_TESTNAME, #A, #B, _a, _b, __LINE__); \
        return 1; \
    } \
} while (0)

#define T_ASSERT_EQ_ULONG(A,B) do { \
    unsigned long _a = (unsigned long)(A); \
    unsigned long _b = (unsigned long)(B); \
    if (_a != _b) { \
        fprintf(stderr, "%s: assert failed: %s == %s (got %lu, expected %lu) (line %d)\n", \
                T_TESTNAME, #A, #B, _a, _b, __LINE__); \
        return 1; \
    } \
} while (0)

#define T_ASSERT_STREQ(A,B) do { \
    const char *_a = (A); \
    const char *_b = (B); \
    if (!_a) _a = "(null)"; \
    if (!_b) _b = "(null)"; \
    if (strcmp(_a, _b) != 0) { \
        fprintf(stderr, "%s: assert failed: \"%s\" == \"%s\" (line %d)\n", \
                T_TESTNAME, _a, _b, __LINE__); \
        return 1; \
    } \
} while (0)

/* Use at end of main() */
#define T_OK() do { \
    printf("OK %s\n", T_TESTNAME); \
    return 0; \
} while (0)

#endif /* TBL_TEST_H */
