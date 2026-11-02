/*
 * Motion sensor input that is not gravity: telling a shake from the accelerometer. Gravity alone stays at 1 g however
 * the object is turned; a shake moves the measured acceleration far from 1 g, again and again (measured on the
 * AtomS3R: up to 11 g, 3 to 6 g away from 1 g on average, against 0.01 g at rest).
 */
#include "internal.h"
#include "lilpet/lilpet.h"

#define SHAKE_RATE  8.0f /* per second: how fast the shake energy follows the distance from 1 g */
#define SHAKE_START 0.6f /* g: energy above it is shaking */
#define SHAKE_END   0.3f /* g: energy back below it ends the shake */
#define SHAKE_MIN   0.4f /* seconds of shaking: a knock, even a hard one, calms down sooner */

bool lp_shake_update(lp_shake *shake, float ax, float ay, float az, float dt)
{
    if (!(dt > 0.0f) || !isfinite(ax) || !isfinite(ay) || !isfinite(az))
        return false;
    const float off = fabsf(sqrtf(ax * ax + ay * ay + az * az) - 1.0f);
    shake->energy = lp_approach(shake->energy, off, SHAKE_RATE, dt);
    if (shake->energy >= SHAKE_START || (shake->shaking > 0.0f && shake->energy > SHAKE_END)) {
        shake->shaking += dt;
        return false;
    }
    /* Told once it is over: during the shake, nobody can see the eyes */
    const bool shaken = shake->shaking >= SHAKE_MIN;
    shake->shaking = 0.0f;
    return shaken;
}
