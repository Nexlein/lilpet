/* Boards without a motion sensor */
#include "imu.h"

bool imu_init(void)
{
    return false;
}

bool imu_gravity(float out[3])
{
    (void)out;
    return false;
}
