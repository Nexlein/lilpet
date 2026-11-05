#include "test.h"

int lp_test_failures = 0;

/* A NaN must fail a comparison, or a corrupted state would pass every parity test */
void test_check_near_fails_on_nan(void)
{
    const int before = lp_test_failures;
    CHECK_NEAR(NAN, 1.0f, 0.1f);
    const int caught = lp_test_failures - before;
    lp_test_failures = before;
    CHECK(caught == 1);
}

static void run(const char *name, void (*fn)(void))
{
    int before = lp_test_failures;
    fn();
    printf("%s %s\n", lp_test_failures == before ? "ok  " : "FAIL", name);
}

int main(void)
{
#define LP_RUN_TEST(fn) run(#fn, fn);
    LP_TESTS(LP_RUN_TEST)
    if (lp_test_failures) {
        printf("%d check(s) failed\n", lp_test_failures);
        return 1;
    }
    return 0;
}
