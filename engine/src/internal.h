/* Helpers shared by the engine sources. Not part of the public API. */
#ifndef LILPET_INTERNAL_H
#define LILPET_INTERNAL_H

#include "lilpet/lilpet.h"

#include <math.h>

#define LP_PI     3.14159265f
#define LP_CENTER ((float)LP_SCREEN_SIZE / 2.0f)

/* Blink phase durations measured on the official footage (see docs/yeux.md) */
#define LP_BLINK_ANTICIP_TIME 0.06f
#define LP_BLINK_CLOSE_TIME   0.1f

/* Emitter debt often sums to exactly 1, which rounding can leave just below: the margin still emits on time */
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

static inline float lp_maxf(float a, float b)
{
    return a > b ? a : b;
}

/* v within -lim..lim. A negative limit counts as 0: a pupil bigger than its eye has no room to move, so it stays
 * centered (with max(-lim, min(lim, v)) as such, -lim would win and push it to one side) */
static inline float lp_clampf(float v, float lim)
{
    const float l = lp_maxf(lim, 0.0f);
    return lp_maxf(-l, lp_minf(l, v));
}

#endif /* LILPET_INTERNAL_H */
