#include "raster.h"

#include <math.h>

#define FAR 1e9f

lp_affine lp_affine_mul(lp_affine m, lp_affine n)
{
    return (lp_affine){
        .a = m.a * n.a + m.b * n.c,
        .b = m.a * n.b + m.b * n.d,
        .c = m.c * n.a + m.d * n.c,
        .d = m.c * n.b + m.d * n.d,
        .tx = m.a * n.tx + m.b * n.ty + m.tx,
        .ty = m.c * n.tx + m.d * n.ty + m.ty,
    };
}

lp_affine lp_affine_inverse(lp_affine m)
{
    const float det = m.a * m.d - m.b * m.c;
    const float ia = m.d / det;
    const float ib = -m.b / det;
    const float ic = -m.c / det;
    const float id = m.a / det;
    return (lp_affine){
        .a = ia, .b = ib, .c = ic, .d = id, .tx = -(ia * m.tx + ib * m.ty), .ty = -(ic * m.tx + id * m.ty)};
}

/* Same convention as CanvasRenderingContext2D.rotate: y grows downward, positive angles turn clockwise */
lp_affine lp_affine_rotate(float angle)
{
    const float c = cosf(angle);
    const float s = sinf(angle);
    return (lp_affine){.a = c, .b = -s, .c = s, .d = c, .tx = 0.0f, .ty = 0.0f};
}

lp_affine lp_affine_scale(float sx, float sy)
{
    return (lp_affine){.a = sx, .b = 0.0f, .c = 0.0f, .d = sy, .tx = 0.0f, .ty = 0.0f};
}

void lp_affine_apply(lp_affine m, float x, float y, float *ox, float *oy)
{
    *ox = m.a * x + m.b * y + m.tx;
    *oy = m.c * x + m.d * y + m.ty;
}

void lp_spans_clip(lp_spans *s, float lo, float hi)
{
    int n = 0;
    for (int i = 0; i < s->n; i++) {
        const float a = s->a[i] > lo ? s->a[i] : lo;
        const float b = s->b[i] < hi ? s->b[i] : hi;
        if (a < b) {
            s->a[n] = a;
            s->b[n] = b;
            n++;
        }
    }
    s->n = n;
}

/* Removes [lo, hi), in place */
static void spans_cut(lp_spans *s, float lo, float hi)
{
    const int count = s->n;
    int n = 0;
    for (int i = 0; i < count; i++) {
        const float a = s->a[i];
        const float b = s->b[i];
        if (a < lo && hi < b &&
            count < LP_MAX_SPANS) { /* a hole in this span: it splits, the others stay as they are */
            for (int k = count; k > i; k--) {
                s->a[k] = s->a[k - 1];
                s->b[k] = s->b[k - 1];
            }
            s->b[i] = lo;
            s->a[i + 1] = hi;
            s->n = count + 1;
            return;
        }
        const float left_end = b < lo ? b : lo;
        const float right_start = a > hi ? a : hi;
        if (a < left_end) {
            s->a[n] = a;
            s->b[n] = left_end;
            n++;
        } else if (right_start < b) {
            s->a[n] = right_start;
            s->b[n] = b;
            n++;
        }
    }
    s->n = n;
}

lp_ellipse lp_ellipse_prepare(float du, float dv, float cu, float cv, float ra, float rb)
{
    lp_ellipse e = {.cu = cu, .cv = cv, .inv_ra = 1.0f / ra, .inv_rb = 1.0f / rb};
    e.qu = du * e.inv_ra;
    e.qv = dv * e.inv_rb;
    e.a = e.qu * e.qu + e.qv * e.qv;
    e.half_inv_a = e.a > 0.0f ? 0.5f / e.a : 0.0f;
    return e;
}

lp_half_plane lp_half_plane_prepare(float du, float dv, float m, float c)
{
    const float g1 = dv - m * du;
    return (lp_half_plane){.m = m, .c = c, .g1 = g1, .inv_g1 = fabsf(g1) < 1e-12f ? 0.0f : 1.0f / g1};
}

/* Screen x interval where the line is inside the ellipse. False when it misses. */
static bool line_ellipse(const lp_line *l, const lp_ellipse *e, float *x0, float *x1)
{
    /* The line in unit-circle space: p + x q; solve |p + x q| = 1 */
    const float pu = (l->u0 - e->cu) * e->inv_ra;
    const float pv = (l->v0 - e->cv) * e->inv_rb;
    const float b = 2.0f * (pu * e->qu + pv * e->qv);
    const float c = pu * pu + pv * pv - 1.0f;
    const float disc = b * b - 4.0f * e->a * c;
    if (e->a <= 0.0f || !(disc > 0.0f)) /* a line so far away overflows into a NaN: it misses too */
        return false;
    const float root = sqrtf(disc);
    *x0 = (-b - root) * e->half_inv_a;
    *x1 = (-b + root) * e->half_inv_a;
    return true;
}

void lp_spans_ellipse(lp_spans *out, const lp_line *l, const lp_ellipse *e, float lo, float hi)
{
    float x0;
    float x1;
    if (!line_ellipse(l, e, &x0, &x1)) {
        out->n = 0;
        return;
    }
    out->a[0] = x0 > lo ? x0 : lo;
    out->b[0] = x1 < hi ? x1 : hi;
    out->n = out->a[0] < out->b[0] ? 1 : 0;
}

void lp_spans_keep_below(lp_spans *s, const lp_line *l, const lp_half_plane *h)
{
    /* g(x) = v - m u - c along the line, g0 + x g1; keep g >= 0 */
    const float g0 = l->v0 - h->m * l->u0 - h->c;
    if (h->inv_g1 == 0.0f) {
        if (g0 < 0.0f)
            s->n = 0;
        return;
    }
    const float x = -g0 * h->inv_g1;
    if (h->g1 > 0.0f)
        lp_spans_clip(s, x, FAR);
    else
        lp_spans_clip(s, -FAR, x);
}

void lp_spans_cut_ellipse(lp_spans *s, const lp_line *l, const lp_ellipse *e)
{
    float x0;
    float x1;
    if (line_ellipse(l, e, &x0, &x1))
        spans_cut(s, x0, x1);
}

int lp_polygon_edges(lp_edge *out, const float *pts, int count, float y0, float y1)
{
    int n = 0;
    for (int i = 0, j = count - 1; i < count; j = i++) {
        const float xi = pts[2 * i];
        const float yi = pts[2 * i + 1];
        const float xj = pts[2 * j];
        const float yj = pts[2 * j + 1];
        const bool above = yi <= y0 && yj <= y0;
        const bool below = yi > y1 && yj > y1;
        if (!above && !below)
            out[n++] = (lp_edge){.x = xi, .y0 = yi, .y1 = yj, .slope = yi != yj ? (xj - xi) / (yj - yi) : 0.0f};
    }
    return n;
}

void lp_spans_polygon(lp_spans *out, const lp_edge *edges, int count, float y, float lo, float hi)
{
    float xs[2 * LP_MAX_SPANS];
    int n = 0;
    for (int i = 0; i < count && n < 2 * LP_MAX_SPANS; i++) {
        const lp_edge *e = &edges[i];
        if ((e->y0 > y) != (e->y1 > y)) {
            const float x = e->x + (y - e->y0) * e->slope;
            int k = n++;
            while (k > 0 && xs[k - 1] > x) { /* insertion sort, few crossings */
                xs[k] = xs[k - 1];
                k--;
            }
            xs[k] = x;
        }
    }
    out->n = 0;
    for (int k = 0; k + 1 < n; k += 2) {
        out->a[out->n] = xs[k];
        out->b[out->n] = xs[k + 1];
        out->n++;
    }
    lp_spans_clip(out, lo, hi);
}

static void add_cell(float *diff, int i, float v)
{
    diff[i] += v;
    diff[i + 1] -= v;
}

void lp_coverage_add(float *diff, int origin, int width, const lp_spans *s, float weight)
{
    const float w = (float)width;
    for (int k = 0; k < s->n; k++) {
        float a = s->a[k] - (float)origin;
        float b = s->b[k] - (float)origin;
        a = a < 0.0f ? 0.0f : a;
        b = b > w ? w : b;
        if (a >= b)
            continue;
        const int ia = (int)a;
        const int ib = (int)b;
        add_cell(diff, ia, weight * ((float)(ia + 1) - a));
        diff[ia + 1] += weight; /* full pixels ia + 1 .. ib - 1 */
        diff[ib] -= weight;
        if (b > (float)ib)
            add_cell(diff, ib, weight * (b - (float)ib));
    }
}
