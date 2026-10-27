/*
 * Scanline rasterizer: shapes are turned into horizontal spans, one sub-row at a time, and the spans are
 * accumulated into per-pixel coverage. Exact horizontally (fractional span ends), LP_SUBROWS samples vertically.
 * Not part of the public API.
 */
#ifndef LILPET_RASTER_H
#define LILPET_RASTER_H

#include <stdbool.h>

#define LP_SUBROWS   8 /* vertical samples per pixel row */
#define LP_MAX_SPANS 8

/* Sorted, disjoint intervals [a, b) on a sub-row, in screen x */
typedef struct {
    int n;
    float a[LP_MAX_SPANS];
    float b[LP_MAX_SPANS];
} lp_spans;

/* 2D affine transform: (x, y) -> (a x + b y + tx, c x + d y + ty) */
typedef struct {
    float a, b, c, d, tx, ty;
} lp_affine;

/* A screen sub-row seen in some local frame: the local point for screen column x is (u0 + x du, v0 + x dv) */
typedef struct {
    float u0, du, v0, dv;
} lp_line;

lp_affine lp_affine_mul(lp_affine m, lp_affine n); /* m after n */
lp_affine lp_affine_inverse(lp_affine m);
lp_affine lp_affine_rotate(float angle);
lp_affine lp_affine_scale(float sx, float sy);
void lp_affine_apply(lp_affine m, float x, float y, float *ox, float *oy);
/* The screen sub-row at height y, in the local frame whose local->screen transform has inverse `inv` */
lp_line lp_line_at(lp_affine inv, float y);

void lp_spans_set(lp_spans *s, float a, float b);
void lp_spans_clip(lp_spans *s, float lo, float hi); /* keep the part inside [lo, hi) */
void lp_spans_cut(lp_spans *s, float lo, float hi);  /* remove [lo, hi) */
void lp_spans_intersect(lp_spans *out, const lp_spans *s, const lp_spans *t);

/* Screen x interval where the line is inside the ellipse (center cu, cv; radii ra, rb). False when it misses. */
bool lp_line_ellipse(const lp_line *l, float cu, float cv, float ra, float rb, float *x0, float *x1);
/* Spans of an ellipse on the line, limited to [lo, hi) */
void lp_spans_ellipse(lp_spans *out, const lp_line *l, float cu, float cv, float ra, float rb, float lo, float hi);
/* Keeps the part of the spans where the local point is below the line v = m u + c (v grows downward) */
void lp_spans_keep_below(lp_spans *s, const lp_line *l, float m, float c);
/* Removes the part inside an ellipse */
void lp_spans_cut_ellipse(lp_spans *s, const lp_line *l, float cu, float cv, float ra, float rb);
/* Spans of a closed polygon (screen points, x0 y0 x1 y1 ...) on the sub-row y, even-odd rule, limited to [lo, hi) */
void lp_spans_polygon(lp_spans *out, const float *pts, int count, float y, float lo, float hi);

/*
 * Adds the spans to a coverage row stored as differences: after a prefix sum, cell i holds the covered fraction of
 * pixel column (origin + i), times weight. `diff` has width + 1 cells.
 */
void lp_coverage_add(float *diff, int origin, int width, const lp_spans *s, float weight);

#endif /* LILPET_RASTER_H */
