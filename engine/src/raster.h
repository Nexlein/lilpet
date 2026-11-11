/*
 * Scanline rasterizer: shapes are turned into horizontal spans, one sub-row at a time, and the spans are
 * accumulated into per-pixel coverage. Exact horizontally (fractional span ends), LP_SUBROWS samples vertically.
 * Not part of the public API.
 */
#ifndef LILPET_RASTER_H
#define LILPET_RASTER_H

#include "lilpet/lilpet.h"

#define LP_SUBROWS   8 /* vertical samples per pixel row */
#define LP_MAX_SPANS 8

/* Sorted, disjoint intervals [a, b) on a sub-row, in screen x */
typedef struct {
    int n;
    float a[LP_MAX_SPANS];
    float b[LP_MAX_SPANS];
} lp_spans;

/* A screen sub-row seen in some local frame: the local point at column 0; shapes keep the step per column (du, dv) */
typedef struct {
    float u0, v0;
} lp_line;

lp_affine lp_affine_mul(lp_affine m, lp_affine n); /* m after n */
lp_affine lp_affine_inverse(lp_affine m);
lp_affine lp_affine_rotate(float angle);
lp_affine lp_affine_scale(float sx, float sy);
void lp_affine_apply(lp_affine m, float x, float y, float *ox, float *oy);
/* The screen sub-row at height y, in the local frame whose local->screen transform has inverse `inv` */
static inline lp_line lp_line_at(lp_affine inv, float y)
{
    return (lp_line){.u0 = inv.b * y + inv.tx, .v0 = inv.d * y + inv.ty};
}

/* The part of `in` inside [lo, hi), into `out`, which may be `in` */
void lp_spans_clip(lp_spans *out, const lp_spans *in, float lo, float hi);

/* Prepares an ellipse for the sub-rows of a frame (lines of step du, dv) */
lp_ellipse lp_ellipse_prepare(float du, float dv, float cu, float cv, float ra, float rb);
lp_half_plane lp_half_plane_prepare(float du, float dv, float m, float c);
/* Spans of an ellipse on the line, limited to [lo, hi) */
void lp_spans_ellipse(lp_spans *out, const lp_line *l, const lp_ellipse *e, float lo, float hi);
/* Keeps the part of the spans inside the half plane */
void lp_spans_keep_below(lp_spans *s, const lp_line *l, const lp_half_plane *h);
/* Removes the part inside an ellipse */
void lp_spans_cut_ellipse(lp_spans *s, const lp_line *l, const lp_ellipse *e);
/*
 * Keeps the edges of a closed polygon (`count` screen points, x0 y0 x1 y1 ...) that may cross a sub-row between y0 and
 * y1, in order, and returns how many: a polygon has many edges and a pixel row meets few of them. `out` has `count`
 * cells.
 */
int lp_polygon_edges(lp_edge *out, const float *pts, int count, float y0, float y1);
/* Spans of a polygon on the sub-row y, from its edges that reach it, even-odd rule, limited to [lo, hi) */
void lp_spans_polygon(lp_spans *out, const lp_edge *edges, int count, float y, float lo, float hi);

/*
 * Adds the spans to a coverage row stored as differences: after a prefix sum, cell i holds the covered fraction of
 * pixel column (origin + i), times weight. `diff` has width + 1 cells.
 */
void lp_coverage_add(float *diff, int origin, int width, const lp_spans *s, float weight);

#endif /* LILPET_RASTER_H */
