#include "test.h"

int lp_test_failures = 0;

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
