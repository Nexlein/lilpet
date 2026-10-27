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

lp_line lp_line_at(lp_affine inv, float y)
{
    return (lp_line){.u0 = inv.b * y + inv.tx, .du = inv.a, .v0 = inv.d * y + inv.ty, .dv = inv.c};
}

void lp_spans_set(lp_spans *s, float a, float b)
{
    s->n = a < b ? 1 : 0;
    s->a[0] = a;
    s->b[0] = b;
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

void lp_spans_cut(lp_spans *s, float lo, float hi)
{
    lp_spans out = {0};
    for (int i = 0; i < s->n && out.n < LP_MAX_SPANS; i++) {
        const float a = s->a[i];
        const float b = s->b[i];
        const float left_end = b < lo ? b : lo;
        if (a < left_end) {
            out.a[out.n] = a;
            out.b[out.n] = left_end;
            out.n++;
        }
        const float right_start = a > hi ? a : hi;
        if (right_start < b && out.n < LP_MAX_SPANS) {
            out.a[out.n] = right_start;
            out.b[out.n] = b;
            out.n++;
        }
    }
    *s = out;
}

void lp_spans_intersect(lp_spans *out, const lp_spans *s, const lp_spans *t)
{
    int i = 0;
    int j = 0;
    out->n = 0;
    while (i < s->n && j < t->n && out->n < LP_MAX_SPANS) {
        const float a = s->a[i] > t->a[j] ? s->a[i] : t->a[j];
        const float b = s->b[i] < t->b[j] ? s->b[i] : t->b[j];
        if (a < b) {
            out->a[out->n] = a;
            out->b[out->n] = b;
            out->n++;
        }
        if (s->b[i] < t->b[j])
            i++;
        else
            j++;
    }
}

bool lp_line_ellipse(const lp_line *l, float cu, float cv, float ra, float rb, float *x0, float *x1)
{
    const float pu = (l->u0 - cu) / ra;
    const float pv = (l->v0 - cv) / rb;
    const float qu = l->du / ra;
    const float qv = l->dv / rb;
    const float a = qu * qu + qv * qv;
    const float b = 2.0f * (pu * qu + pv * qv);
    const float c = pu * pu + pv * pv - 1.0f;
    const float disc = b * b - 4.0f * a * c;
    if (a <= 0.0f || disc <= 0.0f)
        return false;
    const float root = sqrtf(disc);
    *x0 = (-b - root) / (2.0f * a);
    *x1 = (-b + root) / (2.0f * a);
    return true;
}

void lp_spans_ellipse(lp_spans *out, const lp_line *l, float cu, float cv, float ra, float rb, float lo, float hi)
{
    float x0;
    float x1;
    if (!lp_line_ellipse(l, cu, cv, ra, rb, &x0, &x1)) {
        out->n = 0;
        return;
    }
    lp_spans_set(out, x0 > lo ? x0 : lo, x1 < hi ? x1 : hi);
}

void lp_spans_keep_below(lp_spans *s, const lp_line *l, float m, float c)
{
    /* g(x) = v - m u - c along the line; keep g >= 0 */
    const float g0 = l->v0 - m * l->u0 - c;
    const float g1 = l->dv - m * l->du;
    if (fabsf(g1) < 1e-12f) {
        if (g0 < 0.0f)
            s->n = 0;
        return;
    }
    const float x = -g0 / g1;
    if (g1 > 0.0f)
        lp_spans_clip(s, x, FAR);
    else
        lp_spans_clip(s, -FAR, x);
}

void lp_spans_cut_ellipse(lp_spans *s, const lp_line *l, float cu, float cv, float ra, float rb)
{
    float x0;
    float x1;
    if (lp_line_ellipse(l, cu, cv, ra, rb, &x0, &x1))
        lp_spans_cut(s, x0, x1);
}

void lp_spans_polygon(lp_spans *out, const float *pts, int count, float y, float lo, float hi)
{
    float xs[2 * LP_MAX_SPANS];
    int n = 0;
    for (int i = 0, j = count - 1; i < count && n < 2 * LP_MAX_SPANS; j = i++) {
        const float xi = pts[2 * i];
        const float yi = pts[2 * i + 1];
        const float xj = pts[2 * j];
        const float yj = pts[2 * j + 1];
        if ((yi > y) != (yj > y)) {
            const float x = xi + (y - yi) * (xj - xi) / (yj - yi);
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
        if (ia == ib) {
            add_cell(diff, ia, weight * (b - a));
            continue;
        }
        add_cell(diff, ia, weight * ((float)(ia + 1) - a));
        diff[ia + 1] += weight; /* full pixels ia + 1 .. ib - 1 */
        diff[ib] -= weight;
        if (b > (float)ib)
            add_cell(diff, ib, weight * (b - (float)ib));
    }
}
