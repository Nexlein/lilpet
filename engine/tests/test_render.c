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
    lp_eyes_init(e, 1);
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
static void to_screen(const float inv[6], float u, float v, int *x, int *y)
{
    const lp_affine m = lp_affine_inverse((lp_affine){inv[0], inv[1], inv[2], inv[3], inv[4], inv[5]});
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

static int close565(uint16_t a, uint16_t b)
{
    return abs(channel(a, 11u, 31u) - channel(b, 11u, 31u)) <= 1 &&
           abs(channel(a, 5u, 63u) - channel(b, 5u, 63u)) <= 1 && abs(channel(a, 0u, 31u) - channel(b, 0u, 31u)) <= 1;
}

#define CHECK_COLOR(px, rgb)                                                                                           \
    do {                                                                                                               \
        const uint16_t got_ = (px);                                                                                    \
        const uint16_t want_ = lp_rgb565(rgb);                                                                         \
        if (!close565(got_, want_))                                                                                    \
            LP_FAIL("%s is 0x%04x, expected 0x%04x", #px, (unsigned)got_, (unsigned)want_);                            \
    } while (0)

void test_rgb565_conversion(void)
{
    CHECK(lp_rgb565(0x000000) == 0x0000);
    CHECK(lp_rgb565(0xFFFFFF) == 0xFFFF);
    CHECK(lp_rgb565(0xFF0000) == 0xF800);
    CHECK(lp_rgb565(0x00FF00) == 0x07E0);
    CHECK(lp_rgb565(0x0000FF) == 0x001F);
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
        to_screen(g->inv_pupil, g->px, g->py, &x, &y); /* pupil center */
        CHECK_COLOR(frame[y * SIZE + x], look->pupil_color[side]);
        to_screen(g->inv_eye, side == 0 ? -0.5f * g->rx : 0.5f * g->rx, 0.0f, &x, &y); /* outer half of the eye */
        CHECK_COLOR(frame[y * SIZE + x], look->sclera);
        to_screen(g->inv_eye, 0.0f, -1.3f * g->ry, &x, &y); /* above the eye */
        CHECK_COLOR(frame[y * SIZE + x], 0x000000);
    }
}

void test_render_glint_is_white(void)
{
    render_scene("look-acid", frame);
    const lp_eye_geometry *g = &other.eyes[0];
    CHECK(g->glint);
    int x;
    int y;
    to_screen(g->inv_pupil, g->gx, g->gy, &x, &y);
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
        to_screen(g->inv_pupil, 0.0f, 0.7f * g->ry - 2.25f, &x, &y); /* middle of the stroke */
        CHECK_COLOR(frame[y * SIZE + x], look->sclera);
    }
}

void test_render_is_deterministic(void)
{
    render_scene("emotion-love", reference);
    render_scene("emotion-love", frame);
    CHECK(memcmp(frame, reference, sizeof frame) == 0);
}

/* Drawing scene b over scene a, through the dirty region only, must give the same frame as drawing b alone */
static void check_incremental(const char *a, const char *b)
{
    render_scene(b, reference);
    set_scene(&eyes, a);
    lp_renderer_init(&renderer);
    memset(frame, 0, sizeof frame);
    lp_render(&renderer, &eyes, frame);
    set_scene(&eyes, b);
    lp_render(&renderer, &eyes, frame);
    if (memcmp(frame, reference, sizeof frame) != 0)
        LP_FAIL("drawing %s over %s leaves stale pixels", b, a);
}

void test_dirty_region_redraws_everything_that_changed(void)
{
    check_incremental("head-turn-right", "head-turn-left");
    check_incremental("emotion-love", "look-sky");
    check_incremental("look-mint", "blink-closed");
    check_incremental("emotion-happy", "emotion-sleepy");
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

void test_band_rendering_matches_whole_frame(void)
{
    enum { BAND_ROWS = 7 }; /* does not divide the region height: the last band is partial */
    static uint16_t band[SIZE * BAND_ROWS];
    lp_eyes_init(&eyes, 1);
    lp_eyes_set_look(&eyes, &LPV_LOOKS[7]);            /* acorn pupil: polygons */
    lp_eyes_set_emotion(&eyes, LP_EMOTION_LOVE, 0.0f); /* hearts */
    lp_renderer_init(&renderer);
    lp_renderer_init(&other);
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
                memcpy(&frame[y * SIZE + dirty.x0], &band[(y - y0) * width], (size_t)width * sizeof band[0]);
        }
        CHECK(memcmp(frame, reference, sizeof frame) == 0);
    }

    /* Rows outside the region leave the band untouched */
    CHECK(dirty.y0 > 0);
    for (int i = 0; i < SIZE * BAND_ROWS; i++)
        band[i] = 0xABCD;
    lp_render_rows(&other, dirty.y0 - BAND_ROWS, dirty.y0, band);
    for (int i = 0; i < SIZE * BAND_ROWS; i++)
        CHECK(band[i] == 0xABCD);
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
            lp_eyes_init(&eyes, 1);
            lp_eyes_set_scene(&eyes, &scene, &LPV_LOOKS[p]);
            lp_renderer_init(&renderer);
            lp_render(&renderer, &eyes, frame);
        }
    }
    CHECK(1);
}
