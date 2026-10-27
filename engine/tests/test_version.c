#include "lilpet/lilpet.h"
#include "test.h"

void test_version_matches_macros(void)
{
    char expected[32];
    snprintf(expected, sizeof expected, "%d.%d.%d", LP_VERSION_MAJOR, LP_VERSION_MINOR, LP_VERSION_PATCH);
    CHECK_STR_EQ(lp_version(), expected);
}
