/* Renderer: colors where they belong, closed eye, dirty regions, determinism, extreme poses */
#include "../src/raster.h"
#include "js_vectors.h"
#include "lilpet/lilpet.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

#define SIZE   LP_SCREEN_SIZE
#define PIXELS (SIZE * SIZE)

/* Large buffers live outside the stack */
static lp_eyes eyes;
static lp_renderer renderer;
static lp_renderer other;
static uint16_t frame[PIXELS];
static uint16_t reference[PIXELS];

static int find_scene(const char *id)
{
    for (int i = 0; i < (int)(sizeof LPV_SCENES / sizeof LPV_SCENES[0]); i++)
        if (strcmp(LPV_SCENES[i].id, id) == 0)
            return i;
    LP_FAIL("unknown scene %s", id);
    return 0;
}

static void set_scene(lp_eyes *e, const char *id)
{
    const int i = find_scene(id);
    lp_eyes_set_scene(e, &LPV_SCENES[i].scene, &LPV_LOOKS[LPV_SCENES[i].look]);
}

/* Fresh full render of a scene into `out` */
static void render_scene(const char *id, uint16_t *out)
{
    set_scene(&eyes, id);
    lp_renderer_init(&other);
    lp_render(&other, &eyes, out);
}

/* Screen pixel of a point given in a frame whose screen->frame transform is `inv` */
static void to_screen(lp_affine inv, float u, float v, int *x, int *y)
{
    const lp_affine m = lp_affine_inverse(inv);
    float sx;
    float sy;
    lp_affine_apply(m, u, v, &sx, &sy);
    *x = (int)sx;
    *y = (int)sy;
}

/* RGB565 colors match within one step per channel (float vs integer rounding) */
static int channel(uint16_t px, unsigned shift, unsigned mask)
{
    return (int)(((unsigned)px >> shift) & mask);
}

/* 0xRRGGBB to RGB565, rounded: the expected color of a solid area */
static uint16_t rgb565(uint32_t rgb)
{
    const uint32_t r = (rgb >> 16u) & 0xFFu;
    const uint32_t g = (rgb >> 8u) & 0xFFu;
    const uint32_t b = rgb & 0xFFu;
    return (uint16_t)(((r * 31u + 127u) / 255u) << 11u | ((g * 63u + 127u) / 255u) << 5u | ((b * 31u + 127u) / 255u));
}

static int close565(uint16_t a, uint16_t b)
{
    return abs(channel(a, 11u, 31u) - channel(b, 11u, 31u)) <= 1 &&
           abs(channel(a, 5u, 63u) - channel(b, 5u, 63u)) <= 1 && abs(channel(a, 0u, 31u) - channel(b, 0u, 31u)) <= 1;
}

#define CHECK_COLOR(px, rgb)                                                                                           \
    do {                                                                                                               \
        const uint16_t got_ = (px);                                                                                    \
        const uint16_t want_ = rgb565(rgb);                                                                            \
        if (!close565(got_, want_))                                                                                    \
            LP_FAIL("%s is 0x%04x, expected 0x%04x", #px, (unsigned)got_, (unsigned)want_);                            \
    } while (0)

void test_rgb565_conversion(void)
{
    CHECK(rgb565(0x000000) == 0x0000);
    CHECK(rgb565(0xFFFFFF) == 0xFFFF);
    CHECK(rgb565(0xFF0000) == 0xF800);
    CHECK(rgb565(0x00FF00) == 0x07E0);
    CHECK(rgb565(0x0000FF) == 0x001F);
}

void test_render_first_frame_is_full_screen(void)
{
    set_scene(&eyes, "look-cherry");
    lp_renderer_init(&renderer);
    const lp_rect dirty = lp_render(&renderer, &eyes, frame);
    CHECK(dirty.x0 == 0 && dirty.y0 == 0 && dirty.x1 == SIZE && dirty.y1 == SIZE);
}

void test_render_puts_colors_where_they_belong(void)
{
    render_scene("look-cherry", frame);
    const lp_look *look = &LPV_LOOKS[LPV_SCENES[find_scene("look-cherry")].look];
    CHECK_COLOR(frame[5 * SIZE + 5], 0x000000); /* corner: background */
    for (int side = 0; side < 2; side++) {
        const lp_eye_geometry *g = &other.eyes[side];
        int x;
        int y;
        to_screen(g->inv_pupil, g->shapes.pupil.cu, g->shapes.pupil.cv, &x, &y); /* pupil center */
        CHECK_COLOR(frame[y * SIZE + x], look->pupil_color[side]);
        const float rx = eyes.shape.rx;
        const float ry = eyes.shape.ry;
        to_screen(g->inv_eye, side == 0 ? -0.5f * rx : 0.5f * rx, 0.0f, &x, &y); /* outer half of the eye */
        CHECK_COLOR(frame[y * SIZE + x], look->sclera);
        to_screen(g->inv_eye, 0.0f, -1.3f * ry, &x, &y); /* above the eye */
        CHECK_COLOR(frame[y * SIZE + x], 0x000000);
    }
}

void test_pixels_differing_in_one_channel_stay_apart(void)
{
    /* Over black, each of these eyes changes a single channel: the pixel must not pass for the one before it */
    const uint32_t colors[] = {0xFF0000u, 0x00FF00u, 0x0000FFu};
    for (int i = 0; i < 3; i++) {
        const lp_look look = {colors[i], LP_PUPIL_DOT, {colors[i], colors[i]}};
        lp_eyes_init(&eyes, 1);
        lp_eyes_set_look(&eyes, &look);
        lp_renderer_init(&renderer);
        lp_render(&renderer, &eyes, frame);
        int x;
        int y;
        to_screen(renderer.eyes[0].inv_eye, 0.0f, 0.0f, &x, &y);
        CHECK_COLOR(frame[y * SIZE + x], colors[i]);
    }
}

void test_render_glint_is_white(void)
{
    render_scene("emotion-love", frame); /* looks carry no glint: it comes with emotions */
    const lp_eye_geometry *g = &other.eyes[0];
    CHECK(g->glint);
    int x;
    int y;
    to_screen(g->inv_pupil, g->shapes.glint.cu, g->shapes.glint.cv, &x, &y);
    CHECK_COLOR(frame[y * SIZE + x], 0xFFFFFF);
}

void test_closed_eye_shows_only_the_stroke(void)
{
    render_scene("blink-closed", frame);
    const lp_look *look = &LPV_LOOKS[LPV_SCENES[find_scene("blink-closed")].look];
    for (int side = 0; side < 2; side++) {
        const lp_eye_geometry *g = &other.eyes[side];
        CHECK(g->closed);
        int x;
        int y;
        to_screen(g->inv_pupil, 0.0f, 0.0f, &x, &y); /* eye center: nothing drawn */
        CHECK_COLOR(frame[y * SIZE + x], 0x000000);
        to_screen(g->inv_pupil, 0.0f, 0.7f * eyes.shape.ry - 2.25f, &x, &y); /* middle of the stroke */
        CHECK_COLOR(frame[y * SIZE + x], look->sclera);
    }
}

void test_box_hugs_the_shape(void)
{
    /* Coverage is exact: a pixel more on each side would only redraw black */
    render_scene("blink-closed", frame);
    for (int side = 0; side < 2; side++) {
        const lp_polygon *p = &other.eyes[side].shape;
        float x0 = (float)SIZE;
        float y0 = (float)SIZE;
        float x1 = 0.0f;
        float y1 = 0.0f;
        for (int i = 0; i < p->count; i++) {
            x0 = fminf(x0, p->pts[2 * i]);
            y0 = fminf(y0, p->pts[2 * i + 1]);
            x1 = fmaxf(x1, p->pts[2 * i]);
            y1 = fmaxf(y1, p->pts[2 * i + 1]);
        }
        CHECK(p->box.x0 == (int)floorf(x0) && p->box.y0 == (int)floorf(y0));
        CHECK(p->box.x1 == (int)ceilf(x1) && p->box.y1 == (int)ceilf(y1));
    }
}

void test_render_is_deterministic(void)
{
    render_scene("emotion-love", reference);
    render_scene("emotion-love", frame);
    CHECK(memcmp(frame, reference, sizeof frame) == 0);
}

/*
 * Drawing scene b over scene a, through the dirty region only, must give the same frame as drawing b alone, at the
 * full size and at the AtomS3R's (scaled output, regions in output pixels)
 */
static void check_incremental(const char *a, const char *b, int size)
{
    set_scene(&eyes, b);
    lp_renderer_init(&other);
    lp_renderer_set_size(&other, size);
    lp_render(&other, &eyes, reference);
    set_scene(&eyes, a);
    lp_renderer_init(&renderer);
    lp_renderer_set_size(&renderer, size);
    memset(frame, 0, sizeof frame);
    lp_render(&renderer, &eyes, frame);
    set_scene(&eyes, b);
    lp_render(&renderer, &eyes, frame);
    if (memcmp(frame, reference, (size_t)(size * size) * sizeof frame[0]) != 0)
        LP_FAIL("drawing %s over %s at %d px leaves stale pixels", b, a, size);
}

void test_dirty_region_redraws_everything_that_changed(void)
{
    const int sizes[] = {SIZE, 128};
    for (int i = 0; i < 2; i++) {
        check_incremental("head-turn-right", "head-turn-left", sizes[i]);
        check_incremental("emotion-love", "look-terrarium", sizes[i]);
        check_incremental("look-terrarium", "blink-closed", sizes[i]);
        check_incremental("emotion-happy", "emotion-sleepy", sizes[i]);
    }
}

void test_dirty_region_stays_inside_the_screen(void)
{
    set_scene(&eyes, "head-turn-right");
    lp_renderer_init(&renderer);
    lp_render(&renderer, &eyes, frame);
    const lp_rect dirty = lp_render(&renderer, &eyes, frame);
    CHECK(dirty.x0 >= 0 && dirty.y0 >= 0 && dirty.x1 <= SIZE && dirty.y1 <= SIZE);
    CHECK(dirty.x0 > 0 || dirty.x1 < SIZE); /* not the whole screen once a frame was drawn */
}

enum { BAND_ROWS = 7 }; /* does not divide the region height: the last band is partial */
static uint16_t band[SIZE * BAND_ROWS];

/* Drawn band by band, an animation gives the same frames as drawn whole; returns the last region */
static lp_rect check_bands(int size)
{
    lp_eyes_init(&eyes, 1);
    lp_eyes_set_look(&eyes,
                     &LPV_LOOKS[LPV_SCENES[find_scene("look-terrarium")].look]); /* acorn pupil, cut by the eye */
    lp_eyes_set_emotion(&eyes, LP_EMOTION_LOVE, 0.0f);                           /* hearts */
    lp_renderer_init(&renderer);
    lp_renderer_init(&other);
    lp_renderer_set_size(&renderer, size);
    lp_renderer_set_size(&other, size);
    memset(reference, 0, sizeof reference);
    memset(frame, 0, sizeof frame);
    lp_rect dirty = {0, 0, 0, 0};
    for (int f = 0; f < 90; f++) {
        lp_eyes_update(&eyes, 1.0f / 64.0f);
        const lp_rect whole = lp_render(&renderer, &eyes, reference);
        dirty = lp_render_begin(&other, &eyes);
        CHECK(dirty.x0 == whole.x0 && dirty.y0 == whole.y0 && dirty.x1 == whole.x1 && dirty.y1 == whole.y1);
        const int width = dirty.x1 - dirty.x0;
        for (int y0 = dirty.y0; y0 < dirty.y1; y0 += BAND_ROWS) {
            lp_render_rows(&other, y0, y0 + BAND_ROWS, band);
            for (int y = y0; y < y0 + BAND_ROWS && y < dirty.y1; y++)
                memcpy(&frame[y * size + dirty.x0], &band[(y - y0) * width], (size_t)width * sizeof band[0]);
        }
        if (memcmp(frame, reference, (size_t)(size * size) * sizeof frame[0]) != 0)
            LP_FAIL("bands differ from the whole frame at %d px, frame %d", size, f);
    }

    /* A band across each edge of the region: rows outside are left as they were, rows inside land at their place */
    const int width = dirty.x1 - dirty.x0;
    const int edges[2] = {dirty.y0, dirty.y1};
    for (int e = 0; e < 2; e++) {
        const int y0 = edges[e] - BAND_ROWS / 2;
        for (int i = 0; i < SIZE * BAND_ROWS; i++)
            band[i] = 0xABCD;
        lp_render_rows(&other, y0, y0 + BAND_ROWS, band);
        for (int y = y0; y < y0 + BAND_ROWS; y++) {
            const bool inside = y >= dirty.y0 && y < dirty.y1;
            for (int x = 0; x < width; x++)
                CHECK(band[(y - y0) * width + x] == (inside ? reference[y * size + dirty.x0 + x] : 0xABCD));
        }
    }
    return dirty;
}

void test_band_rendering_matches_whole_frame(void)
{
    check_bands(128);
    const lp_rect dirty = check_bands(SIZE);

    /* Rows outside the region leave the band untouched */
    CHECK(dirty.y0 > 0);
    for (int i = 0; i < SIZE * BAND_ROWS; i++)
        band[i] = 0xABCD;
    lp_render_rows(&other, dirty.y0 - BAND_ROWS, dirty.y0, band);
    for (int i = 0; i < SIZE * BAND_ROWS; i++)
        CHECK(band[i] == 0xABCD);
}

static uint16_t swapped(uint16_t px)
{
    return (uint16_t)((unsigned)px << 8u | (unsigned)px >> 8u);
}

void test_big_endian_swaps_the_bytes_of_every_pixel(void)
{
    /* The same animation in both orders, whole frames and bands, at both sizes */
    const int sizes[] = {SIZE, 128};
    for (int i = 0; i < 2; i++) {
        const int size = sizes[i];
        lp_eyes_init(&eyes, 1);
        lp_eyes_set_emotion(&eyes, LP_EMOTION_LOVE, 0.0f);
        lp_renderer_init(&renderer);
        lp_renderer_init(&other);
        lp_renderer_set_size(&renderer, size);
        lp_renderer_set_size(&other, size);
        lp_renderer_set_big_endian(&other, true);
        memset(reference, 0, sizeof reference);
        memset(frame, 0, sizeof frame);
        for (int f = 0; f < 60; f++) {
            lp_eyes_update(&eyes, 1.0f / 64.0f);
            lp_render(&renderer, &eyes, reference);
            if (f % 2) {
                lp_render(&other, &eyes, frame);
            } else {
                const lp_rect d = lp_render_begin(&other, &eyes);
                for (int y = d.y0; y < d.y1; y++) {
                    lp_render_rows(&other, y, y + 1, band);
                    memcpy(&frame[y * size + d.x0], band, (size_t)(d.x1 - d.x0) * sizeof band[0]);
                }
            }
        }
        int wrong = 0;
        for (int p = 0; p < size * size; p++)
            wrong += frame[p] != swapped(reference[p]);
        CHECK(wrong == 0);
    }

    /* Back to the CPU's order: the region drawn so far is stale, so the whole frame comes again */
    lp_renderer_set_big_endian(&other, false);
    const lp_rect d = lp_render(&other, &eyes, frame);
    CHECK(d.x0 == 0 && d.y0 == 0 && d.x1 == 128 && d.y1 == 128);
    CHECK(memcmp(frame, reference, 128u * 128u * sizeof frame[0]) == 0);
}

void test_dirty_region_has_even_coordinates(void)
{
    lp_eyes_init(&eyes, 3);
    lp_eyes_set_emotion(&eyes, LP_EMOTION_LOVE, 0.0f); /* hearts move the region every frame */
    lp_renderer_init(&renderer);
    for (int f = 0; f < 120; f++) {
        lp_eyes_update(&eyes, 1.0f / 64.0f);
        const lp_rect d = lp_render_begin(&renderer, &eyes);
        CHECK(d.x0 % 2 == 0 && d.y0 % 2 == 0 && d.x1 % 2 == 0 && d.y1 % 2 == 0);
        CHECK(d.x0 >= 0 && d.y0 >= 0 && d.x1 <= SIZE && d.y1 <= SIZE);
    }
}

void test_extreme_poses_render_inside_the_frame(void)
{
    /* AddressSanitizer catches any write outside the frame */
    const float poses[][5] = {
        {1.0f, 1.0f, 1.0f, 1.0f, 1.0f},
        {-1.0f, -1.0f, -1.0f, -1.0f, -1.0f},
        {1.0f, -1.0f, 0.5f, -1.0f, 1.0f},
    };
    for (int e = 0; e < LP_EMOTION_COUNT; e++) {
        for (int p = 0; p < 3; p++) {
            lp_scene scene = {.emotion = (lp_emotion)e,
                              .yaw = poses[p][0],
                              .pitch = poses[p][1],
                              .roll = poses[p][2],
                              .gaze_x = poses[p][3],
                              .gaze_y = poses[p][4],
                              .has_particles = true,
                              .particle_seed = 5,
                              .particle_time = 3.0f};
            if (!lp_emotion_get((lp_emotion)e)->emit)
                scene.has_particles = false;
            lp_eyes_set_scene(&eyes, &scene, &LPV_LOOKS[p]);
            lp_renderer_init(&renderer);
            lp_render(&renderer, &eyes, frame);
        }
    }
    CHECK(1);
}

void test_unknown_pupil_renders_as_a_circle(void)
{
    lp_look look = LPV_LOOKS[0];
    look.pupil = LP_PUPIL_CIRCLE;
    lp_eyes_init(&eyes, 1);
    lp_eyes_set_look(&eyes, &look);
    lp_renderer_init(&renderer);
    lp_render(&renderer, &eyes, reference);
    look.pupil = LP_PUPIL_COUNT;
    lp_eyes_set_look(&eyes, &look);
    lp_renderer_init(&renderer);
    lp_render(&renderer, &eyes, frame);
    CHECK(memcmp(frame, reference, sizeof frame) == 0);
}

void test_eyes_off_the_screen_render_nothing(void)
{
    /* Half past the edge, all the way past it, too far for an int, wider than the screen; open and closed. The
     * sanitizers catch any write outside the buffers. */
    const lp_shape deflt = lp_default_shape();
    const lp_shape shapes[] = {
        {deflt.rx, deflt.ry, 300.0f, deflt.tilt},
        {deflt.rx, deflt.ry, 400.0f, deflt.tilt},
        {deflt.rx, deflt.ry, 1e30f, deflt.tilt},
        {600.0f, 600.0f, deflt.gap, deflt.tilt},
    };
    const char *scenes[] = {"look-cherry", "blink-closed"};
    const int sizes[] = {SIZE, 128};
    for (int s = 0; s < 2; s++) {
        for (int k = 0; k < 4; k++) {
            for (int i = 0; i < 2; i++) {
                set_scene(&eyes, scenes[s]);
                lp_eyes_set_shape(&eyes, &shapes[k]);
                lp_renderer_init(&renderer);
                lp_renderer_set_size(&renderer, sizes[i]);
                lp_render(&renderer, &eyes, frame);
                const lp_rect d = lp_render(&renderer, &eyes, frame);
                CHECK(d.x0 >= 0 && d.y0 >= 0 && d.x1 <= sizes[i] && d.y1 <= sizes[i]);
                int lit = 0;
                for (int p = 0; p < sizes[i] * sizes[i]; p++)
                    lit += frame[p] != 0;
                if (k == 1 || k == 2) /* the eyes start past the edge, whatever their state */
                    CHECK(lit == 0);
            }
        }
    }
}

/* Cuts [40, 60) out of the spans given as pairs of ends, and checks the pairs left */
static void check_cut(const float (*in)[2], int count, const float (*want)[2], int want_count)
{
    const lp_line line = {.u0 = 0.0f, .du = 1.0f, .v0 = 0.0f, .dv = 0.0f};
    const lp_ellipse hole = lp_ellipse_prepare(1.0f, 0.0f, 50.0f, 0.0f, 10.0f, 1.0f);
    lp_spans s = {.n = count};
    for (int i = 0; i < count; i++) {
        s.a[i] = in[i][0];
        s.b[i] = in[i][1];
    }
    lp_spans_cut_ellipse(&s, &line, &hole);
    CHECK(s.n == want_count);
    for (int i = 0; i < want_count && i < s.n; i++) {
        CHECK_NEAR(s.a[i], want[i][0], 0.001f);
        CHECK_NEAR(s.b[i], want[i][1], 0.001f);
    }
}

void test_cutting_spans(void)
{
    const float whole[][2] = {{0.0f, 100.0f}};
    const float split[][2] = {{0.0f, 40.0f}, {60.0f, 100.0f}};
    check_cut(whole, 1, split, 2);

    const float around[][2] = {{0.0f, 10.0f}, {20.0f, 100.0f}, {110.0f, 120.0f}};
    const float around_cut[][2] = {{0.0f, 10.0f}, {20.0f, 40.0f}, {60.0f, 100.0f}, {110.0f, 120.0f}};
    check_cut(around, 3, around_cut, 4);

    const float ends[][2] = {{0.0f, 45.0f}, {55.0f, 100.0f}};
    check_cut(ends, 2, split, 2);

    const float inside[][2] = {{0.0f, 20.0f}, {45.0f, 55.0f}, {70.0f, 90.0f}};
    const float inside_cut[][2] = {{0.0f, 20.0f}, {70.0f, 90.0f}};
    check_cut(inside, 3, inside_cut, 2);

    check_cut(inside_cut, 2, inside_cut, 2); /* nothing inside the hole */
}

void test_line_too_far_misses_an_ellipse(void)
{
    /* So far that the distance overflows into a NaN: no span, and not the whole row */
    const lp_line line = {.u0 = 1e30f, .du = 1.0f, .v0 = 0.0f, .dv = 0.0f};
    const lp_ellipse e = lp_ellipse_prepare(1.0f, 0.0f, 50.0f, 0.0f, 10.0f, 1.0f);
    lp_spans s;
    lp_spans_ellipse(&s, &line, &e, 0.0f, 100.0f);
    CHECK(s.n == 0);
}

/* --- Smaller output (lp_renderer_set_size) ------------------------------------------------------------------- */

enum { SMALL = 128 };

static float minf(float a, float b)
{
    return a < b ? a : b;
}

static float maxf(float a, float b)
{
    return a > b ? a : b;
}

/* Channel c (0 red, 1 green, 2 blue) of an RGB565 pixel, 0-255 */
static float channel565(uint16_t px, int c)
{
    static const unsigned SHIFT[3] = {11u, 5u, 0u};
    static const unsigned MAX[3] = {31u, 63u, 31u};
    const unsigned v = ((unsigned)px >> SHIFT[c]) & MAX[c];
    return (float)v * 255.0f / (float)MAX[c];
}

/* Mean of the full-size frame over the area one small pixel covers, for channel c */
static float area_mean(const uint16_t *big, int sx, int sy, int c)
{
    const float k = (float)SIZE / (float)SMALL;
    const float x0 = (float)sx * k;
    const float y0 = (float)sy * k;
    float sum = 0.0f;
    float weight = 0.0f;
    for (int y = (int)y0; (float)y < y0 + k && y < SIZE; y++) {
        const float wy = minf((float)y + 1.0f, y0 + k) - maxf((float)y, y0);
        for (int x = (int)x0; (float)x < x0 + k && x < SIZE; x++) {
            const float w = wy * (minf((float)x + 1.0f, x0 + k) - maxf((float)x, x0));
            sum += w * channel565(big[y * SIZE + x], c);
            weight += w;
        }
    }
    return sum / weight;
}

void test_small_output_is_the_frame_scaled_down(void)
{
    static uint16_t small[SMALL * SMALL];
    const char *scenes[] = {"look-popcorn", "look-laser", "emotion-love", "blink-closed", "head-up-left-tilted"};
    for (size_t i = 0; i < sizeof scenes / sizeof scenes[0]; i++) {
        render_scene(scenes[i], frame);
        set_scene(&eyes, scenes[i]);
        lp_renderer_init(&renderer);
        lp_renderer_set_size(&renderer, SMALL);
        const lp_rect dirty = lp_render(&renderer, &eyes, small);
        CHECK(dirty.x0 == 0 && dirty.y0 == 0 && dirty.x1 == SMALL && dirty.y1 == SMALL);
        float diff = 0.0f;
        int far = 0;
        for (int y = 0; y < SMALL; y++)
            for (int x = 0; x < SMALL; x++) {
                float d = 0.0f;
                for (int c = 0; c < 3; c++)
                    d += fabsf(channel565(small[y * SMALL + x], c) - area_mean(frame, x, y, c)) / 3.0f;
                diff += d;
                far += d > 48.0f;
            }
        /* Same picture: only anti-aliased edges differ (sampled at another resolution) */
        const float mean = diff / (float)(SMALL * SMALL);
        if (mean > 1.0f || far > 10) /* measured: mean 0.06 to 0.21, no pixel far off */
            LP_FAIL("%s at %d px: mean difference %.2f, %d pixels far off", scenes[i], SMALL, (double)mean, far);
    }
}

void test_new_size_redraws_everything(void)
{
    set_scene(&eyes, "look-cherry");
    lp_renderer_init(&renderer);
    lp_render(&renderer, &eyes, frame);
    lp_renderer_set_size(&renderer, SMALL);
    const lp_rect dirty = lp_render(&renderer, &eyes, frame);
    CHECK(dirty.x0 == 0 && dirty.y0 == 0 && dirty.x1 == SMALL && dirty.y1 == SMALL);
}

void test_small_output_regions_stay_inside(void)
{
    static uint16_t small[SMALL * SMALL];
    lp_renderer_init(&renderer);
    lp_renderer_set_size(&renderer, 1000);
    CHECK(renderer.size == SIZE);
    lp_renderer_set_size(&renderer, 7);
    CHECK(renderer.size == 6);

    set_scene(&eyes, "head-turn-right");
    lp_eyes_set_emotion(&eyes, LP_EMOTION_LOVE, 0.0f); /* hearts start below the bottom edge of the small screen */
    lp_renderer_set_size(&renderer, SMALL);
    lp_render(&renderer, &eyes, small);
    for (int f = 0; f < 200; f++) {
        lp_eyes_set_pointer(&eyes, true, f % 2 ? 1.0f : -1.0f, 0.5f);
        lp_eyes_update(&eyes, 1.0f / 64.0f);
        const lp_rect d = lp_render(&renderer, &eyes, small);
        CHECK(d.x0 >= 0 && d.y0 >= 0 && d.x1 <= SMALL && d.y1 <= SMALL);
        CHECK(d.x0 % 2 == 0 && d.y0 % 2 == 0 && d.x1 % 2 == 0 && d.y1 % 2 == 0);
    }
}
