/* Helpers shared by the engine sources. Not part of the public API. */
#ifndef LILPET_INTERNAL_H
#define LILPET_INTERNAL_H

#include "lilpet/lilpet.h"

#include <math.h>

#define LP_PI     3.14159265f
#define LP_CENTER ((float)LP_SCREEN_SIZE / 2.0f)

/* Blink phase durations measured on the official footage (see notes/yeux.md) */
#define LP_BLINK_ANTICIP_TIME 0.06f
#define LP_BLINK_CLOSE_TIME   0.1f

/* Emitter debt often sums to exactly 1, where float and double rounding disagree: same margin as the prototype */
#define LP_EMIT_EPSILON 1e-4f

/* Spring-like smoothing toward target, frame-rate independent */
static inline float lp_approach(float v, float target, float k, float dt)
{
    return v + (target - v) * (1.0f - expf(-k * dt));
}

static inline float lp_ease_in(float p)
{
    return p * p;
}

static inline float lp_ease_out(float p)
{
    const float q = 1.0f - p;
    return 1.0f - q * q * q;
}

static inline float lp_minf(float a, float b)
{
    return a < b ? a : b;
}

#endif /* LILPET_INTERNAL_H */
