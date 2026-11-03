/*
 * Motion sensor input that is not gravity: telling a shake from the accelerometer. Gravity alone stays at 1 g however
 * the object is turned; a shake moves the measured acceleration far from 1 g, again and again (measured on the
 * AtomS3R: up to 11 g, 3 to 6 g away from 1 g on average, against 0.01 g at rest).
 */
#include "internal.h"
#include "lilpet/lilpet.h"

#define SHAKE_RATE    8.0f /* per second: how fast the shake energy follows the distance from 1 g */
#define SHAKE_START   1.5f /* g: energy above it counts toward a shake */
#define SHAKE_CONFIRM 0.3f /* seconds above SHAKE_START: a knock or a quick jiggle is not a shake */
#define SHAKE_END     0.5f /* g: a shake goes on until the energy falls back below it */

bool lp_shake_update(lp_shake *shake, float ax, float ay, float az, float dt)
{
    if (!(dt > 0.0f) || !isfinite(ax) || !isfinite(ay) || !isfinite(az))
        return shake->active;
    const float off = fabsf(sqrtf(ax * ax + ay * ay + az * az) - 1.0f);
    shake->energy = lp_approach(shake->energy, off, SHAKE_RATE, dt);
    if (shake->active) {
        shake->active = shake->energy > SHAKE_END;
    } else {
        shake->strong = shake->energy >= SHAKE_START ? shake->strong + dt : 0.0f;
        shake->active = shake->strong >= SHAKE_CONFIRM;
    }
    return shake->active;
}
