/*
 * Renderer: draws the eyes row by row into an RGB565 frame. Port of drawEyes() and its helpers in prototype/draw.js;
 * the shapes and transforms follow the same order as the canvas calls there.
 * Each row is composed in a float RGB buffer: eyes (sclera, pupil, glint, each clipped to the eye), then particles.
 */
#include "internal.h"
#include "lilpet/lilpet.h"
#include "raster.h"
#include "renderer.h"

#include <string.h>

/* Closed-eye stroke, measured on the official footage (see CLOSED in prototype/draw.js) */
#define CLOSED_HALF_WIDTH 0.76f
#define CLOSED_BOTTOM     0.7f
#define CLOSED_DEPTH      0.245f
#define CLOSED_THICKNESS  4.5f

/* Polygon resolution */
#define STROKE_STEPS  24 /* along the closed-eye stroke */
#define CAP_STEPS     7  /* inner points of each round tip */
#define HEART_STEPS   12 /* per Bezier half of a heart */
#define SPARKLE_STEPS 6  /* per quarter of a sparkle */
/* Points past LP_POLY_MAX are dropped: every shape must fit */
_Static_assert(2 * (STROKE_STEPS + 1) + 2 * CAP_STEPS <= LP_POLY_MAX, "closed-eye stroke too long");
_Static_assert(2 * HEART_STEPS <= LP_POLY_MAX, "heart too long");
_Static_assert(4 * SPARKLE_STEPS <= LP_POLY_MAX, "sparkle too long");

#define HEART_COLOR   0xFF2A3Au
#define SPARKLE_COLOR 0xFFFFFFu
#define GLINT_COLOR   0xFFFFFFu

/* Resting pupil position, fractions of the eye radii (x toward the inner corner, y down) */
static const float PUPIL_REST[LP_PUPIL_COUNT][2] = {
    [LP_PUPIL_DOT] = {0.57f, 0.12f},
    [LP_PUPIL_CAT] = {0.35f, 0.11f},
    [LP_PUPIL_ACORN] = {0.18f, 0.25f},
    [LP_PUPIL_CIRCLE] = {0.36f, 0.0f},
};
#define PUPIL_LEAN 0.09f /* pupils lean a little, top toward the nose (rad) */

static lp_rgb to_rgb(uint32_t color)
{
    return (lp_rgb){(float)((color >> 16u) & 0xFFu), (float)((color >> 8u) & 0xFFu), (float)(color & 0xFFu)};
}

static lp_rect rect_empty(void)
{
    return (lp_rect){.x0 = LP_SCREEN_SIZE, .y0 = LP_SCREEN_SIZE, .x1 = 0, .y1 = 0};
}

static bool rect_is_empty(lp_rect r)
{
    return r.x0 >= r.x1 || r.y0 >= r.y1;
}

static lp_rect rect_union(lp_rect r, lp_rect s)
{
    if (rect_is_empty(r))
        return s;
    if (rect_is_empty(s))
        return r;
    return (lp_rect){.x0 = r.x0 < s.x0 ? r.x0 : s.x0,
                     .y0 = r.y0 < s.y0 ? r.y0 : s.y0,
                     .x1 = r.x1 > s.x1 ? r.x1 : s.x1,
                     .y1 = r.y1 > s.y1 ? r.y1 : s.y1};
}

/*
 * Grows a rectangle to even coordinates: the AMOLED controller (CO5300) takes windows that start on an even pixel and
 * have an even size (see the rounder of Waveshare's display demo). The screen size is even, so it stays inside.
 */
static lp_rect align_even(lp_rect r)
{
    if (rect_is_empty(r))
        return r;
    return (lp_rect){.x0 = r.x0 - r.x0 % 2, .y0 = r.y0 - r.y0 % 2, .x1 = r.x1 + r.x1 % 2, .y1 = r.y1 + r.y1 % 2};
}

/* Keeps a rectangle inside a smaller output (lp_renderer_set_size); boxes are only clamped to LP_SCREEN_SIZE */
static lp_rect clip_to(lp_rect r, int size)
{
    r.x1 = r.x1 < size ? r.x1 : size;
    r.y1 = r.y1 < size ? r.y1 : size;
    return r.x0 < r.x1 && r.y0 < r.y1 ? r : rect_empty();
}

/* A coordinate inside the screen, so that it fits an int whatever the shape; a NaN goes to 0 */
static float on_screen(float v)
{
    return lp_minf(lp_maxf(v, 0.0f), (float)LP_SCREEN_SIZE);
}

/* Pixel box around a float box, clamped to the screen; empty outside it */
static lp_rect rect_around(float x0, float y0, float x1, float y1)
{
    const lp_rect r = {(int)floorf(on_screen(x0)), (int)floorf(on_screen(y0)), (int)ceilf(on_screen(x1)),
                       (int)ceilf(on_screen(y1))};
    return rect_is_empty(r) ? rect_empty() : r;
}

/* --- Polygons --------------------------------------------------------------------------------------- */

static void poly_add(lp_polygon *p, float x, float y)
{
    if (p->count < LP_POLY_MAX) {
        p->pts[2 * p->count] = x;
        p->pts[2 * p->count + 1] = y;
        p->count++;
    }
}

static void poly_cubic(lp_polygon *p, const float c[8], int steps)
{
    for (int i = 1; i <= steps; i++) {
        const float t = (float)i / (float)steps;
        const float s = 1.0f - t;
        const float b0 = s * s * s;
        const float b1 = 3.0f * s * s * t;
        const float b2 = 3.0f * s * t * t;
        const float b3 = t * t * t;
        poly_add(p, b0 * c[0] + b1 * c[2] + b2 * c[4] + b3 * c[6], b0 * c[1] + b1 * c[3] + b2 * c[5] + b3 * c[7]);
    }
}

/* Maps the polygon through m and computes its pixel box */
static void poly_transform(lp_polygon *p, lp_affine m)
{
    float x0 = 1e9f;
    float y0 = 1e9f;
    float x1 = -1e9f;
    float y1 = -1e9f;
    for (int i = 0; i < p->count; i++) {
        float x;
        float y;
        lp_affine_apply(m, p->pts[2 * i], p->pts[2 * i + 1], &x, &y);
        p->pts[2 * i] = x;
        p->pts[2 * i + 1] = y;
        x0 = x < x0 ? x : x0;
        y0 = y < y0 ? y : y0;
        x1 = x > x1 ? x : x1;
        y1 = y > y1 ? y : y1;
    }
    p->box = p->count ? rect_around(x0, y0, x1, y1) : rect_empty();
}

/* Half circle of radius |(nx, ny)| around (cx, cy), from side n to side -n, bulging toward the unit vector f */
static void poly_cap(lp_polygon *p, float cx, float cy, float nx, float ny, float fx, float fy, float r)
{
    for (int i = 1; i <= CAP_STEPS; i++) {
        const float a = LP_PI * (float)i / (float)(CAP_STEPS + 1);
        poly_add(p, cx + nx * cosf(a) + fx * r * sinf(a), cy + ny * cosf(a) + fy * r * sinf(a));
    }
}

/* Closed eye: lower half of a flat ellipse stroked with round tips (drawClosedEye in the prototype) */
static void poly_closed(lp_polygon *p, float rx, float ry)
{
    p->count = 0;
    const float h = CLOSED_THICKNESS / 2.0f;
    const float a = CLOSED_HALF_WIDTH * rx;
    const float b = CLOSED_DEPTH * ry - h; /* centerline: the outer edge sits at the measured depth */
    const float yc = (CLOSED_BOTTOM - CLOSED_DEPTH) * ry;
    float cx[STROKE_STEPS + 1];
    float cy[STROKE_STEPS + 1];
    float nx[STROKE_STEPS + 1];
    float ny[STROKE_STEPS + 1];
    for (int i = 0; i <= STROKE_STEPS; i++) {
        const float t = LP_PI * (float)i / (float)STROKE_STEPS;
        cx[i] = a * cosf(t);
        cy[i] = yc + b * sinf(t);
        const float gx = b * cosf(t); /* normal of the ellipse, pointing out */
        const float gy = a * sinf(t);
        const float len = hypotf(gx, gy);
        nx[i] = gx / len * h;
        ny[i] = gy / len * h;
    }
    for (int i = 0; i <= STROKE_STEPS; i++)
        poly_add(p, cx[i] + nx[i], cy[i] + ny[i]);
    poly_cap(p, cx[STROKE_STEPS], cy[STROKE_STEPS], nx[STROKE_STEPS], ny[STROKE_STEPS], 0.0f, -1.0f, h);
    for (int i = STROKE_STEPS; i >= 0; i--)
        poly_add(p, cx[i] - nx[i], cy[i] - ny[i]);
    poly_cap(p, cx[0], cy[0], -nx[0], -ny[0], 0.0f, -1.0f, h);
}

/* Heart of size s at the origin (drawHeart in the prototype) */
static void poly_heart(lp_polygon *p, float s)
{
    p->count = 0;
    const float left[8] = {0.0f, s * 0.9f, -s * 1.4f, 0.0f, -s * 0.9f, -s * 1.1f, 0.0f, -s * 0.35f};
    const float right[8] = {0.0f, -s * 0.35f, s * 0.9f, -s * 1.1f, s * 1.4f, 0.0f, 0.0f, s * 0.9f};
    poly_cubic(p, left, HEART_STEPS);
    poly_cubic(p, right, HEART_STEPS);
}

/* 4-point sparkle of size s: quadratic curves through the center (drawSparkle in the prototype) */
static void poly_sparkle(lp_polygon *p, float s)
{
    p->count = 0;
    const float corners[5][2] = {{0.0f, -s}, {s, 0.0f}, {0.0f, s}, {-s, 0.0f}, {0.0f, -s}};
    for (int q = 0; q < 4; q++) {
        for (int i = 1; i <= SPARKLE_STEPS; i++) {
            const float t = (float)i / (float)SPARKLE_STEPS;
            const float u = 1.0f - t; /* control point at the origin: only the end terms remain */
            poly_add(p, u * u * corners[q][0] + t * t * corners[q + 1][0],
                     u * u * corners[q][1] + t * t * corners[q + 1][1]);
        }
    }
}

/* --- Frame setup ------------------------------------------------------------------------------------- */

static void setup_pupil(lp_eye_geometry *g, const lp_eyes *e, float rx, float ry, float inner, lp_affine to_screen)
{
    const float ps = e->cur.ps;
    const lp_pupil type = e->look.pupil;
    float prx;
    float pry;
    switch (type) {
    case LP_PUPIL_DOT:
        prx = rx * 0.105f * ps;
        pry = ry * 0.125f * ps;
        break;
    case LP_PUPIL_CAT:
        prx = rx * 0.21f * ps;
        pry = ry * 0.72f;
        break;
    case LP_PUPIL_ACORN: /* a tall ellipse set low: the bottom of the eye cuts it into its acorn shape */
        prx = rx * 0.5f * ps;
        pry = ry * 0.98f * lp_minf(ps, 1.05f);
        break;
    case LP_PUPIL_CIRCLE:
    default:
        prx = pry = rx * 0.52f * ps;
        break;
    }
    /* Travel is limited by the room left inside the eye; big acorn pupils may run past the edge */
    const int t = (unsigned)type < LP_PUPIL_COUNT ? (int)type : LP_PUPIL_CIRCLE;
    const float lim_x = type == LP_PUPIL_ACORN ? rx : rx - prx - 3.0f;
    const float lim_y = type == LP_PUPIL_ACORN ? ry : ry - pry - 3.0f;
    const float px =
        lp_clampf(inner * PUPIL_REST[t][0] * rx + e->gaze.x * lp_maxf(rx - prx, rx * 0.15f) * 0.85f, lim_x);
    const float py = lp_clampf(PUPIL_REST[t][1] * ry + e->gaze.y * lp_maxf(ry - pry, ry * 0.12f) * 0.85f, lim_y);
    /* Lean around the pupil center */
    lp_affine lean = lp_affine_rotate(inner * PUPIL_LEAN);
    lean.tx = px - (lean.a * px + lean.b * py);
    lean.ty = py - (lean.c * px + lean.d * py);
    g->inv_pupil = lp_affine_inverse(lp_affine_mul(to_screen, lean));
    const float du = g->inv_pupil.a;
    const float dv = g->inv_pupil.c;
    g->shapes.pupil = lp_ellipse_prepare(du, dv, px, py, prx, pry);
    g->glint = e->cur.hl > 0.5f;
    if (g->glint) {
        const float gr = lp_maxf(lp_minf(prx, pry) * 0.28f, 4.0f);
        g->shapes.glint = lp_ellipse_prepare(du, dv, px - prx * 0.35f, py - pry * 0.38f, gr, gr);
    }
}

/* Mirrors drawEye in the prototype: same transform chain, lids and cuts */
/* `scale` maps the 466-pixel layout to the output size (lp_renderer_set_size) */
static void setup_eye(lp_eye_geometry *g, const lp_eyes *e, int side, lp_blink_pose bp, float squash_x, float squash_y,
                      float scale)
{
    const lp_expression *cur = &e->cur;
    const float rx = e->shape.rx;
    const float ry = e->shape.ry;
    const float tilt = e->shape.tilt;
    const float inner = (float)-side;
    const float upright = inner * (cur->rot - tilt);
    const lp_eye_pose pose = lp_eyes_eye_pose(e, side);

    lp_affine m = {.a = scale, .b = 0.0f, .c = 0.0f, .d = scale, .tx = scale * pose.x, .ty = scale * pose.y};
    m = lp_affine_mul(m, lp_affine_rotate(e->head.roll));
    m = lp_affine_mul(m, lp_affine_scale(pose.persp * pose.fx, pose.persp * pose.fy));
    m = lp_affine_mul(m, lp_affine_rotate(upright));
    m = lp_affine_mul(m, lp_affine_scale(cur->es * squash_x * bp.sx * (1.0f + cur->sq * 0.1f),
                                         cur->es * squash_y * bp.sy * (1.0f - cur->sq)));
    const lp_affine pupil = lp_affine_mul(m, lp_affine_rotate(-upright)); /* pupils stay upright */
    g->inv_eye = lp_affine_inverse(m);
    g->inv_pupil = lp_affine_inverse(pupil); /* setup_pupil leans it when the eye is open */
    g->sclera = to_rgb(e->look.sclera);
    g->pupil_color = to_rgb(e->look.pupil_color[side < 0 ? 0 : 1]);
    g->closed = bp.closed;

    if (bp.closed) {
        poly_closed(&g->shape, rx, ry);
        poly_transform(&g->shape, pupil);
        g->box = g->shape.box;
        return;
    }

    const float hx = hypotf(m.a * rx, m.b * ry);
    const float hy = hypotf(m.c * rx, m.d * ry);
    g->box = rect_around(m.tx - hx, m.ty - hy, m.tx + hx, m.ty + hy);

    const float du = g->inv_eye.a;
    const float dv = g->inv_eye.c;
    g->shapes.eye = lp_ellipse_prepare(du, dv, 0.0f, 0.0f, rx, ry);
    g->shapes.lid = lp_half_plane_prepare(du, dv, cur->ts * ry * inner / rx, -ry + cur->tl * 2.0f * ry);
    g->crescent = cur->cr > 0.002f;
    if (g->crescent)
        g->shapes.crescent = lp_ellipse_prepare(du, dv, 0.0f, 2.0f * ry * (1.0f - cur->cr), rx * 1.05f, ry);
    g->happy = cur->bl > 0.002f;
    if (g->happy) {
        const float rb = ry * 1.1f;
        g->shapes.happy = lp_ellipse_prepare(du, dv, 0.0f, ry + rb - cur->bl * 2.0f * ry, rx * 1.6f, rb);
    }
    g->blink_lid = bp.lid > 0.0f;
    if (g->blink_lid) {
        /* Straight lid coming down, level with the face: its slope cancels the eye tilt */
        const float slope = tanf(inner * (tilt - cur->rot));
        const float slack = fabsf(slope) * (rx + 10.0f);
        g->shapes.blink = lp_half_plane_prepare(du, dv, slope, -ry - slack + bp.lid * (2.0f * ry + 2.0f * slack));
    }
    setup_pupil(g, e, rx, ry, inner, pupil);
}

static void setup_particles(lp_render_state *r, const lp_eyes *e)
{
    r->particle_count = 0;
    for (int i = 0; i < e->particle_count; i++) {
        const lp_particle *p = &e->particles[i];
        const float k = p->age / p->life;
        lp_polygon *poly = &r->particles[r->particle_count];
        float alpha = 1.0f;
        const float s = r->scale;
        lp_affine m = {.a = s, .b = 0.0f, .c = 0.0f, .d = s, .tx = s * p->x, .ty = s * p->y};
        if (p->type == LP_PARTICLE_HEART) {
            alpha = lp_minf(lp_minf(k * 6.0f, 1.0f), (1.0f - k) * 4.0f); /* quick fade in, fade out */
            poly_heart(poly, p->size);
            r->particle_color[r->particle_count] = to_rgb(HEART_COLOR);
        } else {
            const float twinkle = sinf(k * LP_PI); /* grow then shrink */
            if (twinkle <= 0.0f)
                continue;
            m = lp_affine_mul(m, lp_affine_rotate(p->spin * p->age));
            poly_sparkle(poly, p->size * twinkle);
            r->particle_color[r->particle_count] = to_rgb(SPARKLE_COLOR);
        }
        if (alpha <= 0.0f)
            continue;
        poly_transform(poly, m);
        r->particle_alpha[r->particle_count] = alpha;
        r->particle_count++;
    }
}

/* --- Row composition --------------------------------------------------------------------------------- */

static inline void composite(float *line, int x, lp_rgb color, float coverage)
{
    if (coverage <= 0.0f)
        return;
    const float a = coverage > 1.0f ? 1.0f : coverage;
    float *px = &line[3 * x];
    px[0] += (color.r - px[0]) * a;
    px[1] += (color.g - px[1]) * a;
    px[2] += (color.b - px[2]) * a;
}

static float sub_row_y(int y, int sub)
{
    return (float)y + ((float)sub + 0.5f) / (float)LP_SUBROWS;
}

/* Eye region on one sub-row: ellipse, then lids and cuts */
static void eye_spans(const lp_eye_geometry *g, float y, lp_spans *out)
{
    const lp_line line = lp_line_at(g->inv_eye, y);
    lp_spans_ellipse(out, &line, &g->shapes.eye, (float)g->box.x0, (float)g->box.x1);
    lp_spans_keep_below(out, &line, &g->shapes.lid);
    if (g->crescent)
        lp_spans_cut_ellipse(out, &line, &g->shapes.crescent);
    if (g->happy)
        lp_spans_cut_ellipse(out, &line, &g->shapes.happy);
    if (g->blink_lid)
        lp_spans_keep_below(out, &line, &g->shapes.blink);
}

/* Blends a coverage row, stored as differences, over the row being composed */
static void blend_row(float *line, int x0, const float *cov, int width, lp_rgb color)
{
    float acc = 0.0f;
    for (int i = 0; i < width; i++) {
        acc += cov[i];
        composite(line, x0 + i, color, acc);
    }
}

static void draw_polygon_row(lp_render_state *r, const lp_polygon *p, int y, lp_rgb color, float alpha)
{
    if (y < p->box.y0 || y >= p->box.y1)
        return;
    const int x0 = p->box.x0;
    const int width = p->box.x1 - x0;
    float *cov = r->cov[0];
    memset(cov, 0, (size_t)(width + 1) * sizeof *cov);
    for (int sub = 0; sub < LP_SUBROWS; sub++) {
        lp_spans s;
        lp_spans_polygon(&s, p->pts, p->count, sub_row_y(y, sub), (float)x0, (float)p->box.x1);
        lp_coverage_add(cov, x0, width, &s, alpha / (float)LP_SUBROWS);
    }
    blend_row(r->line, x0, cov, width, color);
}

/* Adds to a coverage row the part of the eye inside `shape`, which is one span at most (an ellipse on a sub-row);
 * returns whether there was one */
static bool add_inside_eye(float *cov, int x0, int width, const lp_spans *eye, const lp_spans *shape, float w)
{
    if (shape->n == 0)
        return false;
    lp_spans clipped;
    clipped.n = eye->n;
    for (int i = 0; i < eye->n; i++) { /* the spans in use only: the whole structure is 68 bytes, per sub-row */
        clipped.a[i] = eye->a[i];
        clipped.b[i] = eye->b[i];
    }
    lp_spans_clip(&clipped, shape->a[0], shape->b[0]);
    lp_coverage_add(cov, x0, width, &clipped, w);
    return clipped.n > 0;
}

static void draw_eye_row(lp_render_state *r, const lp_eye_geometry *g, int y)
{
    if (y < g->box.y0 || y >= g->box.y1)
        return;
    if (g->closed) {
        draw_polygon_row(r, &g->shape, y, g->sclera, 1.0f);
        return;
    }
    const int x0 = g->box.x0;
    const int width = g->box.x1 - x0;
    const float lo = (float)x0;
    const float hi = (float)g->box.x1;
    const float w = 1.0f / (float)LP_SUBROWS;
    for (int k = 0; k < 3; k++)
        memset(r->cov[k], 0, (size_t)(width + 1) * sizeof r->cov[k][0]);

    bool sclera = false;
    bool pupil = false;
    bool glint = false;
    for (int sub = 0; sub < LP_SUBROWS; sub++) {
        const float ys = sub_row_y(y, sub);
        lp_spans eye;
        eye_spans(g, ys, &eye);
        if (eye.n == 0)
            continue;
        sclera = true;
        lp_coverage_add(r->cov[0], x0, width, &eye, w);

        const lp_line pl = lp_line_at(g->inv_pupil, ys);
        lp_spans shape;
        lp_spans_ellipse(&shape, &pl, &g->shapes.pupil, lo, hi);
        pupil |= add_inside_eye(r->cov[1], x0, width, &eye, &shape, w);

        if (g->glint) {
            lp_spans_ellipse(&shape, &pl, &g->shapes.glint, lo, hi);
            glint |= add_inside_eye(r->cov[2], x0, width, &eye, &shape, w);
        }
    }

    /* Layer after layer: most rows of an eye have no pupil or glint, and a pixel only sees its own layers in order */
    if (sclera)
        blend_row(r->line, x0, r->cov[0], width, g->sclera);
    if (pupil)
        blend_row(r->line, x0, r->cov[1], width, g->pupil_color);
    if (glint)
        blend_row(r->line, x0, r->cov[2], width, to_rgb(GLINT_COLOR));
}

/* --- Public API ------------------------------------------------------------------------------------- */

void lp_renderer_init(lp_renderer *renderer)
{
    memset(renderer, 0, sizeof *renderer);
    lp_render_state *r = lp_render_state_of(renderer);
    r->size = LP_SCREEN_SIZE;
    r->scale = 1.0f;
}

void lp_renderer_set_size(lp_renderer *renderer, int size)
{
    lp_render_state *r = lp_render_state_of(renderer);
    if (size < 2)
        size = 2;
    if (size > LP_SCREEN_SIZE)
        size = LP_SCREEN_SIZE;
    r->size = size - size % 2; /* even, like every region */
    r->scale = (float)r->size / (float)LP_SCREEN_SIZE;
    r->drawn = false; /* the next frame redraws everything */
}

void lp_renderer_set_big_endian(lp_renderer *renderer, bool big_endian)
{
    lp_render_state *r = lp_render_state_of(renderer);
    r->big_endian = big_endian;
    r->drawn = false; /* the next frame redraws everything */
}

/* swap: 8 exchanges the two bytes of the pixel, 0 leaves them */
static uint16_t pack565(const float *px, unsigned swap)
{
    const uint32_t r = (uint32_t)(px[0] * (31.0f / 255.0f) + 0.5f);
    const uint32_t g = (uint32_t)(px[1] * (63.0f / 255.0f) + 0.5f);
    const uint32_t b = (uint32_t)(px[2] * (31.0f / 255.0f) + 0.5f);
    const uint32_t v = r << 11u | g << 5u | b;
    return (uint16_t)(v << swap | v >> swap);
}

lp_rect lp_render_begin(lp_renderer *renderer, const lp_eyes *eyes)
{
    lp_render_state *r = lp_render_state_of(renderer);
    const lp_blink_pose bp = lp_eyes_blink_pose(eyes);

    /* Squash and stretch along the gaze movement, plus a slow "breathing" */
    const float speed = hypotf(eyes->gaze.vx, eyes->gaze.vy);
    const float st = lp_minf(speed * 0.025f, 0.12f);
    const float ax = speed > 0.0f ? fabsf(eyes->gaze.vx) / speed : 0.0f;
    const float ay = speed > 0.0f ? fabsf(eyes->gaze.vy) / speed : 0.0f;
    const float breath = 1.0f + 0.012f * sinf(eyes->time * 2.1f);
    const float squash_x = 1.0f + st * ax - st * ay * 0.5f;
    const float squash_y = (1.0f + st * ay - st * ax * 0.5f) * breath;

    setup_eye(&r->eyes[0], eyes, -1, bp, squash_x, squash_y, r->scale);
    setup_eye(&r->eyes[1], eyes, 1, bp, squash_x, squash_y, r->scale);
    setup_particles(r, eyes);

    lp_rect now = rect_union(r->eyes[0].box, r->eyes[1].box);
    for (int i = 0; i < r->particle_count; i++)
        now = rect_union(now, r->particles[i].box);
    const lp_rect full = {0, 0, r->size, r->size};
    r->dirty = r->drawn ? clip_to(align_even(rect_union(now, r->prev)), r->size) : full;
    r->prev = now;
    r->drawn = true;
    return r->dirty;
}

/* Composes row y of the dirty region and writes its pixels, from dirty.x0 to dirty.x1, to `out` */
static void draw_row(lp_render_state *r, int y, uint16_t *out)
{
    const lp_rect d = r->dirty;
    memset(&r->line[3 * d.x0], 0, (size_t)(d.x1 - d.x0) * 3 * sizeof r->line[0]);
    draw_eye_row(r, &r->eyes[0], y);
    draw_eye_row(r, &r->eyes[1], y);
    for (int i = 0; i < r->particle_count; i++)
        draw_polygon_row(r, &r->particles[i], y, r->particle_color[i], r->particle_alpha[i]);

    /* Most pixels repeat the one before them (background, inside of an eye), and converting to integers is slow */
    static const float NONE[3] = {-1.0f, -1.0f, -1.0f};
    const float *last = NONE;
    const unsigned swap = r->big_endian ? 8u : 0u;
    uint16_t packed = 0;
    for (int x = d.x0; x < d.x1; x++) {
        const float *px = &r->line[3 * x];
        if (px[0] != last[0] || px[1] != last[1] || px[2] != last[2])
            packed = pack565(px, swap);
        last = px;
        out[x - d.x0] = packed;
    }
}

void lp_render_rows(lp_renderer *renderer, int y0, int y1, uint16_t *band)
{
    lp_render_state *r = lp_render_state_of(renderer);
    const lp_rect d = r->dirty;
    const int width = d.x1 - d.x0;
    for (int y = y0 > d.y0 ? y0 : d.y0; y < y1 && y < d.y1; y++)
        draw_row(r, y, &band[(y - y0) * width]);
}

lp_rect lp_render(lp_renderer *renderer, const lp_eyes *eyes, uint16_t *frame)
{
    lp_render_state *r = lp_render_state_of(renderer);
    const lp_rect dirty = lp_render_begin(renderer, eyes);
    for (int y = dirty.y0; y < dirty.y1; y++)
        draw_row(r, y, &frame[y * r->size + dirty.x0]);
    return dirty;
}
