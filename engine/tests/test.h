/*
 * Minimal test runner for the engine, run on the host (no dependency).
 * Add a test: write `void test_x(void)` in a C file of this folder, then list it in LP_TESTS below.
 */
#ifndef LILPET_TEST_H
#define LILPET_TEST_H

#include <math.h>
#include <stdio.h>
#include <string.h>

/* Every test function, one per line */
#define LP_TESTS(X) X(test_version_matches_macros)

#define LP_DECLARE_TEST(fn) void fn(void);
LP_TESTS(LP_DECLARE_TEST)

extern int lp_test_failures;

#define LP_FAIL(...)                                                                                                   \
    do {                                                                                                               \
        fprintf(stderr, "%s:%d: ", __FILE__, __LINE__);                                                                \
        fprintf(stderr, __VA_ARGS__);                                                                                  \
        fputc('\n', stderr);                                                                                           \
        lp_test_failures++;                                                                                            \
    } while (0)

#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond))                                                                                                   \
            LP_FAIL("CHECK failed: %s", #cond);                                                                        \
    } while (0)

#define CHECK_STR_EQ(actual, expected)                                                                                 \
    do {                                                                                                               \
        const char *a_ = (actual);                                                                                     \
        const char *e_ = (expected);                                                                                   \
        if (strcmp(a_, e_) != 0)                                                                                       \
            LP_FAIL("%s is \"%s\", expected \"%s\"", #actual, a_, e_);                                                 \
    } while (0)

#define CHECK_NEAR(actual, expected, tolerance)                                                                        \
    do {                                                                                                               \
        float a_ = (actual), e_ = (expected);                                                                          \
        if (fabsf(a_ - e_) > (tolerance))                                                                              \
            LP_FAIL("%s is %g, expected %g", #actual, (double)a_, (double)e_);                                         \
    } while (0)

#endif /* LILPET_TEST_H */
