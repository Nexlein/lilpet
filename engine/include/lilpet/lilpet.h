/*
 * lilpet eye engine, shared by the ESP32-S3 firmware and the WebAssembly build (see notes/architecture.md).
 * Portable C17: no allocation, no global state, float only (doubles are emulated in software on the ESP32-S3).
 * It reproduces the design prototype (prototype/eyes.js); names and constants match it.
 * Units: pixels of the 466x466 round screen, seconds, radians.
 */
#ifndef LILPET_LILPET_H
#define LILPET_LILPET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bumped when the public API or the rendering changes */
#define LP_VERSION_MAJOR 0
#define LP_VERSION_MINOR 12
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
typedef enum { LP_PUPIL_DOT, LP_PUPIL_CAT, LP_PUPIL_ACORN, LP_PUPIL_CIRCLE, LP_PUPIL_COUNT } lp_pupil;

typedef struct {
    uint32_t sclera;         /* eye color, 0xRRGGBB */
    lp_pupil pupil;          /* pupil shape */
    uint32_t pupil_color[2]; /* left and right pupil colors, 0xRRGGBB */
} lp_look;

/* Looks as text, as the PC app sends them over USB (notes/app.md): "AE0002 FF5252 FF5252 dot" for the eye, left
 * pupil, right pupil and pupil type. Parsing returns false and leaves *look unchanged on malformed text. */
#define LP_LOOK_TEXT_SIZE 28 /* longest text with its NUL */
bool lp_look_parse(const char *text, lp_look *look);
/* Writes the text into `out`, LP_LOOK_TEXT_SIZE bytes; returns its length without the NUL, or -1 for an unknown pupil
 * type */
int lp_look_format(const lp_look *look, char *out);

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
    LP_EMOTION_DIZZY,
    LP_EMOTION_COUNT
} lp_emotion;

/*
 * Eyelid and pupil shape of an emotion:
 * tl top lid closure (0-1), ts top lid slope (+ inner corner lower), bl curved bottom lid ("happy"),
 * sq vertical squash (squint), cr crescent cut (frown), rot extra eye rotation, inner corner down (rad),
 * hl forced pupil glint (0/1), ps pupil scale, es eye scale, gy gaze bias down, spin dizzy gaze circling (rad/s).
 */
typedef struct {
    float tl, ts, bl, sq, cr, rot, hl, ps, es, gy, spin;
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

/* What happens to the object, then what its camera sees: gestures are the ones ESP-DL recognizes (notes/materiel.md) */
typedef enum {
    LP_EVENT_TAP,
    LP_EVENT_SHAKE,
    LP_EVENT_UPSIDE_DOWN,
    LP_EVENT_FACE_SEEN,
    LP_EVENT_GESTURE_LIKE,
    LP_EVENT_GESTURE_DISLIKE,
    LP_EVENT_GESTURE_OK,
    LP_EVENT_GESTURE_FIVE,
    LP_EVENT_GESTURE_CALL,
    LP_EVENT_EXPRESSION_HAPPY,
    LP_EVENT_EXPRESSION_SAD,
    LP_EVENT_EXPRESSION_ANGRY,
    LP_EVENT_EXPRESSION_SURPRISED,
    LP_EVENT_COUNT
} lp_event;

/* How the eyes react to an event: an emotion for a while, and a burst of sparkles (REACTIONS in prototype/eyes.js) */
typedef struct {
    const char *name; /* same names as the prototype ("tap", "gesture_like"...) */
    lp_emotion emotion;
    float duration; /* seconds */
    int sparkles;
} lp_reaction;

/* Reaction to an event; NULL when the event is unknown */
const lp_reaction *lp_reaction_get(lp_event event);
/* Event from its name; LP_EVENT_COUNT when unknown */
lp_event lp_event_from_name(const char *name);

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
    float time; /* seconds, from lp_eyes_init; goes back by a fixed amount every ~17 minutes (eyes.c) */

    lp_look look;
    lp_emotion emotion;
    lp_emotion mood;     /* the emotion picked for good: temporary ones come back to it */
    float emotion_timer; /* > 0: temporary emotion, back to the mood when it reaches 0 */
    float upside_down;   /* seconds held upside down (gravity) */
    bool shaking;        /* being shaken (lp_eyes_set_shaking) */
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
    struct {
        bool active;
        float x, y; /* in the screen plane: x right, y down, in g */
    } gravity;      /* accelerometer: tilting turns the face to stay level and slides the eyes */
    float next_saccade;
    struct {
        bool active;
        float x, y;
    } pending_gaze; /* gaze jump hidden behind the next blink */

    struct {
        float yaw, pitch, roll, roll_target;
        float level; /* tilt roll that keeps the face level, eased */
    } head;
    float next_roll;
    float spin_phase; /* dizzy: where the gaze is on its circle (rad) */

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
/* duration > 0: back to the mood after that many seconds; 0: the new mood */
void lp_eyes_set_emotion(lp_eyes *eyes, lp_emotion emotion, float duration);
/* Reacts to an event (lp_reaction_get); unknown events are ignored */
void lp_eyes_react(lp_eyes *eyes, lp_event event);
/* Starts a blink, unless one is running */
void lp_eyes_blink(lp_eyes *eyes);
/* Adds one particle; dropped when the pool is full (random numbers are still drawn, to keep the sequence) */
void lp_eyes_spawn(lp_eyes *eyes, lp_particle_type type);
/* Pointer (face position) in -1..1, or inactive */
void lp_eyes_set_pointer(lp_eyes *eyes, bool active, float x, float y);
/* Gravity from an accelerometer, in the screen plane (x right, y down, in g), or inactive without one */
void lp_eyes_set_gravity(lp_eyes *eyes, bool active, float x, float y);
/* Being shaken (lp_shake_update): the shake reaction lasts as long, and a little after */
void lp_eyes_set_shaking(lp_eyes *eyes, bool shaking);
/* Follow the active pointer instead of idle saccades (on by default) */
void lp_eyes_set_follow(lp_eyes *eyes, bool follow);
/* Eye shape: radii, gap and tilt (the prototype page's sliders, later the PC app) */
void lp_eyes_set_shape(lp_eyes *eyes, const lp_shape *shape);
/* How much the head follows the gaze: 0 still, 1 fully (0.6 by default) */
void lp_eyes_set_head_amount(lp_eyes *eyes, float amount);
/* Advances the animation by dt seconds */
void lp_eyes_update(lp_eyes *eyes, float dt);

/* --- Motion sensor -------------------------------------------------------------------------------- */

/* Tells a shake from raw accelerometer samples (motion.c). The caller owns it; zero it to start. */
typedef struct {
    float energy; /* distance from 1 g, smoothed */
    float strong; /* seconds of strong motion, before it counts as a shake */
    bool active;  /* being shaken */
} lp_shake;

/* Feeds one sample (in g, any axes, not smoothed); true while the object is being shaken (for lp_eyes_set_shaking) */
bool lp_shake_update(lp_shake *shake, float ax, float ay, float az, float dt);

/* --- What the object senses ----------------------------------------------------------------------- */

/*
 * Turns what the hardware reports into the state of the eyes, the same way on every board (senses.c): gravity and
 * shakes from an accelerometer, presses of a button, the position of a face seen by a camera. A sensor that stops
 * reporting never leaves the eyes stuck in what it last said.
 */
typedef struct {
    lp_shake shake;
    float gravity[2]; /* smoothed, in the screen plane */
    float motion_age; /* seconds since the last accelerometer sample */
    bool pressed;
    float button_age; /* seconds since the button last changed */
    bool face;        /* the eyes are following a face */
    float face_age;   /* seconds since the last position of a face */
} lp_senses;

void lp_senses_init(lp_senses *senses);
/* Once per frame, before lp_eyes_update. accel: in g, screen axes (x right, y down), raw; NULL without a sample. */
void lp_senses_update(lp_senses *senses, lp_eyes *eyes, const float *accel, bool pressed, float dt);
/* A face at x, y in -1..1, whenever the camera reports one; seen = false when it says the face is gone. A face that
 * comes back after a long absence is new: it fires LP_EVENT_FACE_SEEN. */
void lp_senses_face(lp_senses *senses, lp_eyes *eyes, bool seen, float x, float y);

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
    float inv_pupil[6]; /* screen -> pupil frame: upright, then leaned around the pupil center */
    float rx, ry;
    float lid_m, lid_c; /* emotion top lid: keep v >= lid_m u + lid_c */
    bool blink_lid;
    float blink_m, blink_c;
    bool crescent, happy; /* cut by an ellipse (center 0, cv; radii ra, rb) */
    float crescent_cv, crescent_ra, crescent_rb;
    float happy_cv, happy_ra, happy_rb;
    float px, py, prx, pry;
    bool glint;
    float gx, gy, gr;
    uint32_t sclera, pupil_color;
    lp_polygon shape; /* closed-eye stroke */
} lp_eye_geometry;

/* Scratch memory and dirty-region state of the renderer, owned by the caller (about 30 KB). Opaque in practice. */
typedef struct {
    lp_rect prev;  /* region covered by the previous frame */
    lp_rect dirty; /* region of the frame being drawn, set by lp_render_begin */
    bool drawn;    /* a frame was drawn before (otherwise the whole screen is redrawn) */
    int size;      /* output width and height in pixels (lp_renderer_set_size) */
    float scale;   /* size / LP_SCREEN_SIZE */
    lp_eye_geometry eyes[2];
    lp_polygon particles[LP_MAX_PARTICLES];
    uint32_t particle_color[LP_MAX_PARTICLES];
    float particle_alpha[LP_MAX_PARTICLES];
    int particle_count;
    float line[LP_SCREEN_SIZE * 3];   /* RGB of the row being composed, 0-255 */
    float cov[3][LP_SCREEN_SIZE + 1]; /* coverage rows, stored as differences */
} lp_renderer;

/* Prepares a renderer for the full LP_SCREEN_SIZE screen */
void lp_renderer_init(lp_renderer *renderer);
/* Draws at another square size, from 2 to LP_SCREEN_SIZE pixels (rounded down to even), e.g. 128 for a small screen:
 * the same picture, scaled. Frames passed to lp_render are then size x size; the next frame redraws everything. */
void lp_renderer_set_size(lp_renderer *renderer, int size);
/*
 * Draws the eyes into `frame` (size x size RGB565 pixels, row-major; size is LP_SCREEN_SIZE unless changed by
 * lp_renderer_set_size) and returns the region that changed since the previous call, the only part to send to the
 * display. The first call redraws the whole screen.
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
