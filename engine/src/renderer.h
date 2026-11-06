/* What is inside a lp_renderer: the geometry of the frame being drawn and the rows being composed. Not part of the
 * public API. */
#ifndef LILPET_RENDERER_H
#define LILPET_RENDERER_H

#include "lilpet/lilpet.h"
#include "raster.h"

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
} lp_render_state;

_Static_assert(sizeof(lp_render_state) <= sizeof(lp_renderer), "LP_RENDERER_FLOATS in lilpet.h is too small");
_Static_assert(_Alignof(lp_render_state) <= _Alignof(lp_renderer), "lp_renderer is not aligned enough");

static inline lp_render_state *lp_render_state_of(lp_renderer *renderer)
{
    return (lp_render_state *)renderer;
}

#endif /* LILPET_RENDERER_H */
