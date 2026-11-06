/*
 * What is inside a lp_renderer: the geometry of the frame being drawn and the rows being composed. Included by
 * lilpet.h, because the caller owns the memory and needs its size; nothing here is part of the API.
 */
#ifndef LILPET_RENDER_STATE_H
#define LILPET_RENDER_STATE_H

/* 2D affine transform: (x, y) -> (a x + b y + tx, c x + d y + ty) */
typedef struct {
    float a, b, c, d, tx, ty;
} lp_affine;

/*
 * An ellipse (center cu, cv; radii ra, rb, in a local frame) made ready for the sub-rows of that frame, which all
 * share one step (du, dv): the divisions are done once a frame, not per sub-row (the ESP32-S3 has no fast division).
 */
typedef struct {
    float cu, cv, inv_ra, inv_rb;
    float qu, qv, a, half_inv_a; /* step in unit-circle space, its squared length, 1 / (2 a) */
} lp_ellipse;

/* The half plane below the line v = m u + c (v grows downward), made ready the same way */
typedef struct {
    float m, c, g1, inv_g1; /* g1: how fast v - m u changes along a sub-row */
} lp_half_plane;

/* A color ready to blend, 0-255 per channel */
typedef struct {
    float r, g, b;
} lp_rgb;

#define LP_POLY_MAX 64 /* points per polygon */

/* A filled shape in screen coordinates */
typedef struct {
    int count;
    float pts[2 * LP_POLY_MAX]; /* x0 y0 x1 y1 ... */
    lp_rect box;
} lp_polygon;

/* Geometry of one eye for the frame being drawn */
typedef struct {
    bool closed;
    lp_rect box;
    lp_affine inv_eye;   /* screen -> eye frame */
    lp_affine inv_pupil; /* screen -> pupil frame: upright, then leaned around the pupil center */
    /* Which of the optional shapes below an open eye has */
    bool blink_lid, crescent, happy, glint;
    struct {
        lp_ellipse eye;
        lp_half_plane lid;   /* emotion top lid */
        lp_half_plane blink; /* blink lid */
        lp_ellipse crescent; /* cuts of the eye */
        lp_ellipse happy;
        lp_ellipse pupil; /* in the pupil frame, like the glint */
        lp_ellipse glint;
    } shapes;
    lp_rgb sclera, pupil_color;
    lp_polygon shape; /* closed-eye stroke */
} lp_eye_geometry;

typedef struct {
    lp_rect prev;    /* region covered by the previous frame */
    lp_rect dirty;   /* region of the frame being drawn, set by lp_render_begin */
    bool drawn;      /* a frame was drawn before (otherwise the whole screen is redrawn) */
    int size;        /* output width and height in pixels (lp_renderer_set_size) */
    float scale;     /* size / LP_SCREEN_SIZE */
    bool big_endian; /* pixels written high byte first (lp_renderer_set_big_endian) */
    lp_eye_geometry eyes[2];
    lp_polygon particles[LP_MAX_PARTICLES];
    lp_rgb particle_color[LP_MAX_PARTICLES];
    float particle_alpha[LP_MAX_PARTICLES];
    int particle_count;
    float line[LP_SCREEN_SIZE * 3];   /* RGB of the row being composed, 0-255 */
    float cov[3][LP_SCREEN_SIZE + 1]; /* coverage rows, stored as differences */
} lp_renderer;

#endif /* LILPET_RENDER_STATE_H */
