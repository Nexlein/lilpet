/*
 * lilpet eye engine, shared by the ESP32-S3 firmware and the WebAssembly build (see notes/architecture.md).
 * Portable C17: no allocation, no global state, float only (doubles are emulated in software on the ESP32-S3).
 * It reproduces the design prototype (prototype/eyes.js); names and constants match it.
 * Units: pixels of the 466x466 round screen, seconds, radians.
 */
#ifndef LILPET_LILPET_H
#define LILPET_LILPET_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bumped when the public API or the rendering changes */
#define LP_VERSION_MAJOR 0
#define LP_VERSION_MINOR 2
#define LP_VERSION_PATCH 0

/* Engine version as "MAJOR.MINOR.PATCH" */
const char *lp_version(void);

#define LP_SCREEN_SIZE   466
#define LP_MAX_PARTICLES 32

/* --- Random numbers: mulberry32, same sequence as createRng in prototype/eyes.js ---------------- */

typedef struct {
    uint32_t state;
} lp_rng;

void lp_rng_seed(lp_rng *rng, uint32_t seed);
uint32_t lp_rng_next_u32(lp_rng *rng);
/* Uniform in [0, 1), 24-bit resolution: the exact same values as the prototype */
float lp_rng_next(lp_rng *rng);

/* --- Looks -------------------------------------------------------------------------------------- */

typedef enum { LP_PUPIL_DOT, LP_PUPIL_SLIT, LP_PUPIL_ACORN, LP_PUPIL_ROUND } lp_pupil;

typedef struct {
    uint32_t sclera;         /* eye color, 0xRRGGBB */
    lp_pupil pupil;          /* pupil shape */
    uint32_t pupil_color[2]; /* left and right pupil colors, 0xRRGGBB */
    bool highlight;          /* white glint on the pupils */
} lp_look;

/* --- Emotions ----------------------------------------------------------------------------------- */

typedef enum {
    LP_EMOTION_NEUTRAL,
    LP_EMOTION_HAPPY,
    LP_EMOTION_LOVE,
    LP_EMOTION_ANGRY,
    LP_EMOTION_SQUINT,
    LP_EMOTION_FROWN,
    LP_EMOTION_SAD,
    LP_EMOTION_SURPRISED,
    LP_EMOTION_SLEEPY,
    LP_EMOTION_COUNT
} lp_emotion;

/*
 * Eyelid and pupil shape of an emotion:
 * tl top lid closure (0-1), ts top lid slope (+ inner corner lower), bl curved bottom lid ("happy"),
 * sq vertical squash (squint), cr crescent cut (frown), rot extra eye rotation, inner corner down (rad),
 * hl forced pupil glint (0/1), ps pupil scale, es eye scale, gy gaze bias down.
 */
typedef struct {
    float tl, ts, bl, sq, cr, rot, hl, ps, es, gy;
} lp_expression;

typedef enum { LP_PARTICLE_NONE, LP_PARTICLE_HEART, LP_PARTICLE_SPARKLE } lp_particle_type;

typedef struct {
    const char *name;      /* same names as the prototype ("neutral", "love"...) */
    lp_expression expr;    /* target shape */
    lp_particle_type emit; /* particles spawned while the emotion is on */
    float emit_rate;       /* particles per second */
} lp_emotion_def;

/* Definition of an emotion; NULL when out of range */
const lp_emotion_def *lp_emotion_get(lp_emotion emotion);
/* Emotion from its name; LP_EMOTION_COUNT when unknown */
lp_emotion lp_emotion_from_name(const char *name);

/* --- Eyes state --------------------------------------------------------------------------------- */

typedef struct {
    float rx, ry; /* eye radii */
    float gap;    /* distance from the screen center to each eye center */
    float tilt;   /* eye tilt, tops leaning outward (rad) */
} lp_shape;

typedef enum { LP_BLINK_NONE, LP_BLINK_ANTICIP, LP_BLINK_CLOSE, LP_BLINK_CLOSED, LP_BLINK_OPEN } lp_blink_phase;

typedef struct {
    lp_particle_type type;
    float x, y;      /* screen position */
    float vy, sway;  /* hearts: rising speed, sway phase */
    float size;      /* radius-like size */
    float spin;      /* sparkles: rotation speed */
    float age, life; /* seconds */
} lp_particle;

/*
 * Whole state of the eyes. The caller owns the memory (no allocation inside); fields are readable,
 * but change them only through the functions below so the animation stays consistent.
 */
typedef struct {
    lp_rng rng;
    float time; /* seconds since lp_eyes_init */

    lp_look look;
    lp_emotion emotion;
    float emotion_timer; /* > 0: temporary emotion, back to neutral when it reaches 0 */
    lp_expression cur;   /* current shape, easing toward the emotion's target */

    lp_shape shape;
    float head_amount; /* 0-1, how much the head follows the gaze */

    struct {
        float x, y;   /* current gaze, -1..1 */
        float tx, ty; /* target */
        float vx, vy; /* speed, for squash and stretch */
    } gaze;
    bool follow; /* follow the pointer (face tracking) instead of idle saccades */
    struct {
        bool active;
        float x, y;
    } pointer;
    float next_saccade;
    struct {
        bool active;
        float x, y;
    } pending_gaze; /* gaze jump hidden behind the next blink */

    struct {
        float yaw, pitch, roll, roll_target;
    } head;
    float next_roll;

    struct {
        lp_blink_phase phase;
        float t;    /* seconds into the phase */
        float hold; /* closed duration */
        float open; /* reopening duration */
        bool twice; /* blink again right after this one */
    } blink;
    float next_blink;

    lp_particle particles[LP_MAX_PARTICLES];
    int particle_count;
    float emit_debt; /* fractional particles owed by the current emitter */
} lp_eyes;

/* Default shape, measured on the original (see notes/yeux.md) */
lp_shape lp_default_shape(void);

void lp_eyes_init(lp_eyes *eyes, uint32_t seed);
void lp_eyes_set_look(lp_eyes *eyes, const lp_look *look);
/* duration > 0: back to neutral after that many seconds */
void lp_eyes_set_emotion(lp_eyes *eyes, lp_emotion emotion, float duration);
/* Starts a blink, unless one is running */
void lp_eyes_blink(lp_eyes *eyes);
/* Adds one particle; dropped when the pool is full (random numbers are still drawn, to keep the sequence) */
void lp_eyes_spawn(lp_eyes *eyes, lp_particle_type type);
/* Pointer (face position) in -1..1, or inactive */
void lp_eyes_set_pointer(lp_eyes *eyes, bool active, float x, float y);
/* Advances the animation by dt seconds */
void lp_eyes_update(lp_eyes *eyes, float dt);

/* --- Reference scenes (prototype/scenes.json) --------------------------------------------------- */

typedef struct {
    lp_emotion emotion;
    lp_blink_phase blink_phase;
    float blink_t;
    float yaw, pitch, roll;
    float gaze_x, gaze_y;
    bool has_particles;
    uint32_t particle_seed;
    float particle_time;
} lp_scene;

/* Puts the eyes in the state of a scene: emotion settled, gaze still, particles replayed from their seed */
void lp_eyes_set_scene(lp_eyes *eyes, const lp_scene *scene, const lp_look *look);

/* --- Derived geometry, used by the renderer ----------------------------------------------------- */

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

#ifdef __cplusplus
}
#endif

#endif /* LILPET_LILPET_H */
