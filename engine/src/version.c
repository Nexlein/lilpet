#include "lilpet/lilpet.h"

#define LP_STR_(x) #x
#define LP_STR(x)  LP_STR_(x)

const char *lp_version(void)
{
    return LP_STR(LP_VERSION_MAJOR) "." LP_STR(LP_VERSION_MINOR) "." LP_STR(LP_VERSION_PATCH);
}
