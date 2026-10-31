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
#define LP_VERSION_MINOR 6
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

/* Official pupil names (notes/yeux.md) */
typedef enum { LP_PUPIL_DOT, LP_PUPIL_CAT, LP_PUPIL_ACORN, LP_PUPIL_CIRCLE } lp_pupil;

typedef struct {
    uint32_t sclera;         /* eye color, 0xRRGGBB */
    lp_pupil pupil;          /* pupil shape */
    uint32_t pupil_color[2]; /* left and right pupil colors, 0xRRGGBB */
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
/* Follow the active pointer instead of idle saccades (on by default) */
void lp_eyes_set_follow(lp_eyes *eyes, bool follow);
/* Eye shape: radii, gap and tilt (the prototype page's sliders, later the PC app) */
void lp_eyes_set_shape(lp_eyes *eyes, const lp_shape *shape);
/* How much the head follows the gaze: 0 still, 1 fully (0.6 by default) */
void lp_eyes_set_head_amount(lp_eyes *eyes, float amount);
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

/* --- Rendering into an RGB565 frame ------------------------------------------------------------- */

/* Pixel rectangle, x1 and y1 excluded; empty when x0 >= x1 or y0 >= y1 */
typedef struct {
    int x0, y0, x1, y1;
} lp_rect;

#define LP_POLY_MAX 64 /* points per polygon */

/* A filled shape in screen coordinates (internal to the renderer) */
typedef struct {
    int count;
    float pts[2 * LP_POLY_MAX]; /* x0 y0 x1 y1 ... */
    lp_rect box;
} lp_polygon;

/* Geometry of one eye for the frame being drawn (internal to the renderer) */
typedef struct {
    bool closed;
    lp_rect box;
    float inv_eye[6];   /* screen -> eye frame (affine a b c d tx ty) */
    float inv_pupil[6]; /* screen -> pupil frame, which stays upright */
    float rx, ry;
    float lid_m, lid_c; /* emotion top lid: keep v >= lid_m u + lid_c */
    bool blink_lid;
    float blink_m, blink_c;
    bool crescent, happy; /* cut by an ellipse (center 0, cv; radii ra, rb) */
    float crescent_cv, crescent_ra, crescent_rb;
    float happy_cv, happy_ra, happy_rb;
    bool pupil_polygon; /* acorn: `shape` holds the pupil; otherwise an ellipse in the pupil frame */
    float px, py, prx, pry;
    bool glint;
    float gx, gy, gr;
    uint32_t sclera, pupil_color;
    lp_polygon shape; /* acorn pupil, or the closed-eye stroke */
} lp_eye_geometry;

/* Scratch memory and dirty-region state of the renderer, owned by the caller (about 30 KB). Opaque in practice. */
typedef struct {
    lp_rect prev;  /* region covered by the previous frame */
    lp_rect dirty; /* region of the frame being drawn, set by lp_render_begin */
    bool drawn;    /* a frame was drawn before (otherwise the whole screen is redrawn) */
    lp_eye_geometry eyes[2];
    lp_polygon particles[LP_MAX_PARTICLES];
    uint32_t particle_color[LP_MAX_PARTICLES];
    float particle_alpha[LP_MAX_PARTICLES];
    int particle_count;
    float line[LP_SCREEN_SIZE * 3];   /* RGB of the row being composed, 0-255 */
    float cov[3][LP_SCREEN_SIZE + 1]; /* coverage rows, stored as differences */
} lp_renderer;

void lp_renderer_init(lp_renderer *renderer);
/* 0xRRGGBB to RGB565 (native byte order) */
uint16_t lp_rgb565(uint32_t rgb);
/*
 * Draws the eyes into `frame` (LP_SCREEN_SIZE x LP_SCREEN_SIZE RGB565 pixels, row-major) and returns the region that
 * changed since the previous call, the only part to send to the display. The first call redraws the whole screen.
 * The region's coordinates are even, as the AMOLED controller requires for a window.
 */
lp_rect lp_render(lp_renderer *renderer, const lp_eyes *eyes, uint16_t *frame);

/*
 * The same in bands, without a whole frame in memory: the renderer never reads the previous frame back, so the
 * device can draw a few rows at a time into a small buffer and send each band while drawing the next one.
 *
 * lp_render_begin prepares a frame and returns the region that changed since the previous frame (the whole screen the
 * first time). lp_render_rows then draws rows y0 to y1 (excluded) of that region into `band`: one row after the other,
 * each (dirty.x1 - dirty.x0) pixels wide, the layout a display expects for a window. Rows outside the region are
 * skipped (their place in `band` is left as is). Draw every row of the region before the next lp_render_begin.
 */
lp_rect lp_render_begin(lp_renderer *renderer, const lp_eyes *eyes);
void lp_render_rows(lp_renderer *renderer, int y0, int y1, uint16_t *band);

#ifdef __cplusplus
}
#endif

#endif /* LILPET_LILPET_H */
