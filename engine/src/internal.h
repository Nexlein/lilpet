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

/* --- Shared by the engine's sources and tests, not by its users (lilpet.h is what they see) ------------------ */

void lp_rng_seed(lp_rng *rng, uint32_t seed);
/* Uniform in [0, 1), 24-bit resolution: exact in float, the same values on every platform */
float lp_rng_next(lp_rng *rng);

/* Emotion from its name; LP_EMOTION_COUNT when unknown */
lp_emotion lp_emotion_from_name(const char *name);
/* Event from its name; LP_EVENT_COUNT when unknown */
lp_event lp_event_from_name(const char *name);

/* Default shape, measured on the original (see docs/yeux.md), from eyes/looks.json */
lp_shape lp_default_shape(void);

/* Adds one particle; dropped when the pool is full (random numbers are still drawn, to keep the sequence) */
void lp_eyes_spawn(lp_eyes *eyes, lp_particle_type type);

/* Feeds one sample (in g, any axes, not smoothed); true while the object is being shaken (for lp_eyes_set_shaking) */
bool lp_shake_update(lp_shake *shake, float ax, float ay, float az, float dt);

/* Derived geometry, used by the renderer */
typedef struct {
    float lid;    /* blink lid closure, 0 open, 1 shut */
    float sx, sy; /* eye scale */
    bool closed;  /* draw the closed-eye stroke instead of the eye */
} lp_blink_pose;

typedef struct {
    float x, y;   /* eye center on screen */
    float persp;  /* perspective scale, 1 at rest */
    float fx, fy; /* foreshortening */
} lp_eye_pose;

lp_blink_pose lp_eyes_blink_pose(const lp_eyes *eyes);
/* side: -1 left eye, +1 right eye */
lp_eye_pose lp_eyes_eye_pose(const lp_eyes *eyes, int side);

/* A pupil type's name and shape before the emotion scales it (render.c), in fractions of the eye's radii: radii, and
 * resting position, x toward the nose and y down. From eyes/looks.json (looks_table.h). */
typedef struct {
    const char *name;
    float rx;
    float ry;
    float x;
    float y;
} lp_pupil_shape;

typedef struct {
    float lean; /* each pupil's tilt, top toward the nose (rad) */
    lp_pupil_shape types[LP_PUPIL_COUNT];
} lp_pupils;

/* The pupil types of eyes/looks.json (look.c) */
const lp_pupils *lp_pupils_get(void);

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
