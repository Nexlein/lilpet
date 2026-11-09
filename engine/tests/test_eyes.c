/* The eyes themselves: names, random numbers and clamp, setters, bad input, time steps, scenes, the clock */
#include "../src/internal.h"
#include "lilpet/lilpet.h"
#include "test.h"

#include <math.h>

/* Whether two eyes are at the same point of their animation: every member an update can change, one by one (floats
 * and padding make a byte comparison unreliable) */
static bool same_motion(const lp_eyes *a, const lp_eyes *b)
{
    bool same = a->rng.state == b->rng.state && a->time == b->time && a->emotion == b->emotion && a->mood == b->mood &&
                a->emotion_timer == b->emotion_timer && a->burst_wait == b->burst_wait &&
                a->upside_down == b->upside_down && a->next_saccade == b->next_saccade &&
                a->next_roll == b->next_roll && a->spin_phase == b->spin_phase && a->next_blink == b->next_blink;
    same = same && a->cur.tl == b->cur.tl && a->cur.ts == b->cur.ts && a->cur.bl == b->cur.bl &&
           a->cur.sq == b->cur.sq && a->cur.cr == b->cur.cr && a->cur.rot == b->cur.rot && a->cur.hl == b->cur.hl &&
           a->cur.ps == b->cur.ps && a->cur.es == b->cur.es && a->cur.gy == b->cur.gy && a->cur.spin == b->cur.spin;
    same = same && a->gaze.x == b->gaze.x && a->gaze.y == b->gaze.y && a->gaze.tx == b->gaze.tx &&
           a->gaze.ty == b->gaze.ty && a->gaze.vx == b->gaze.vx && a->gaze.vy == b->gaze.vy &&
           a->pending_gaze.active == b->pending_gaze.active && a->pending_gaze.x == b->pending_gaze.x &&
           a->pending_gaze.y == b->pending_gaze.y;
    same = same && a->head.yaw == b->head.yaw && a->head.pitch == b->head.pitch && a->head.roll == b->head.roll &&
           a->head.roll_target == b->head.roll_target && a->head.level == b->head.level &&
           a->blink.phase == b->blink.phase && a->blink.t == b->blink.t && a->blink.hold == b->blink.hold &&
           a->blink.open == b->blink.open && a->blink.twice == b->blink.twice;
    same = same && a->particle_count == b->particle_count && a->emit_debt == b->emit_debt;
    for (int i = 0; same && i < a->particle_count; i++) {
        const lp_particle *p = &a->particles[i];
        const lp_particle *q = &b->particles[i];
        same = p->type == q->type && p->x == q->x && p->y == q->y && p->vy == q->vy && p->sway == q->sway &&
               p->size == q->size && p->spin == q->spin && p->age == q->age && p->life == q->life;
    }
    return same;
}

void test_unknown_emotion_is_rejected(void)
{
    CHECK(lp_emotion_from_name("furious") == LP_EMOTION_COUNT);
    CHECK(lp_emotion_get(LP_EMOTION_COUNT) == NULL);

    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    lp_eyes_set_emotion(&eyes, LP_EMOTION_COUNT, 0.0f);
    CHECK(eyes.emotion == LP_EMOTION_NEUTRAL);
}

void test_every_emotion_and_event_is_found_by_its_name(void)
{
    for (int i = 0; i < LP_EMOTION_COUNT; i++)
        CHECK(lp_emotion_from_name(lp_emotion_get((lp_emotion)i)->name) == (lp_emotion)i);
    for (int i = 0; i < LP_EVENT_COUNT; i++)
        CHECK(lp_event_from_name(lp_reaction_get((lp_event)i)->name) == (lp_event)i);
    /* Names the protocol sends (docs/protocole.md) */
    CHECK(lp_emotion_from_name("love") == LP_EMOTION_LOVE);
    CHECK(lp_event_from_name("gesture_like") == LP_EVENT_GESTURE_LIKE);
}

void test_rng_gives_the_mulberry32_sequence(void)
{
    /* 24-bit numerators over 2^24: exact in float. The second seed wraps the 32-bit state on its first step. */
    static const uint32_t seed_1[] = {10520554u, 45897u,    8849092u, 16459303u, 16246685u, 4716134u,
                                      10281729u, 12092063u, 7143687u, 16690359u, 7638182u,  8200507u,
                                      2330918u,  6773713u,  4152761u, 2591964u};
    static const uint32_t seed_deadbeef[] = {15793561u, 4482800u, 12952570u, 6008940u};
    lp_rng rng;
    lp_rng_seed(&rng, 1u);
    for (size_t i = 0; i < sizeof seed_1 / sizeof seed_1[0]; i++)
        CHECK(lp_rng_next(&rng) == (float)seed_1[i] / 16777216.0f);
    lp_rng_seed(&rng, 0xDEADBEEFu);
    for (size_t i = 0; i < sizeof seed_deadbeef / sizeof seed_deadbeef[0]; i++)
        CHECK(lp_rng_next(&rng) == (float)seed_deadbeef[i] / 16777216.0f);
}

void test_clamp_with_a_negative_limit_gives_zero(void)
{
    CHECK(lp_clampf(0.25f, 1.0f) == 0.25f);
    CHECK(lp_clampf(2.0f, 1.0f) == 1.0f);
    CHECK(lp_clampf(-2.0f, 1.0f) == -1.0f);
    /* A pupil bigger than its eye makes the limit negative: no room left, so the pupil stays centered */
    CHECK(lp_clampf(33.5f, -6.75f) == 0.0f);
    CHECK(lp_clampf(-33.5f, -6.75f) == 0.0f);
    CHECK(lp_clampf(0.0f, -6.75f) == 0.0f);
}

void test_follow_off_ignores_the_pointer(void)
{
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    lp_eyes_set_pointer(&eyes, true, 0.9f, 0.0f);
    lp_eyes_set_follow(&eyes, false);
    for (int i = 0; i < 64; i++)
        lp_eyes_update(&eyes, 1.0f / 64.0f);
    CHECK(eyes.gaze.tx != 0.9f);

    lp_eyes_set_follow(&eyes, true);
    lp_eyes_update(&eyes, 1.0f / 64.0f);
    CHECK(eyes.gaze.tx == 0.9f);
}

void test_head_amount_zero_keeps_the_head_still(void)
{
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    lp_eyes_set_head_amount(&eyes, 0.0f);
    lp_eyes_set_pointer(&eyes, true, 0.9f, 0.5f);
    for (int i = 0; i < 320; i++)
        lp_eyes_update(&eyes, 1.0f / 64.0f);
    CHECK(eyes.head.yaw == 0.0f && eyes.head.pitch == 0.0f && eyes.head.roll == 0.0f);
}

void test_shape_and_head_amount_stay_in_their_ranges(void)
{
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    lp_eyes_set_head_amount(&eyes, 1e20f);
    CHECK(eyes.head_amount == 1.0f);
    lp_eyes_set_head_amount(&eyes, -3.0f);
    CHECK(eyes.head_amount == 0.0f);
    /* An eye with no size, or a negative one, is no shape: the one before stays */
    const lp_shape before = eyes.shape;
    lp_eyes_set_shape(&eyes, &(lp_shape){.rx = 0.0f, .ry = 50.0f, .gap = 101.0f, .tilt = 0.37f});
    lp_eyes_set_shape(&eyes, &(lp_shape){.rx = 50.0f, .ry = -50.0f, .gap = 101.0f, .tilt = 0.37f});
    CHECK(eyes.shape.rx == before.rx && eyes.shape.ry == before.ry);
    lp_eyes_set_shape(&eyes, &(lp_shape){.rx = 1.0f, .ry = 400.0f, .gap = 400.0f, .tilt = -0.5f});
    CHECK(eyes.shape.rx == 1.0f && eyes.shape.ry == 400.0f && eyes.shape.gap == 400.0f && eyes.shape.tilt == -0.5f);
}

void test_set_shape_is_used_by_the_renderer(void)
{
    /* Narrower eyes cover less of the screen */
    static lp_renderer renderer;
    static uint16_t frame[LP_SCREEN_SIZE * LP_SCREEN_SIZE];
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    lp_renderer_init(&renderer);
    lp_render(&renderer, &eyes, frame);
    const lp_rect wide = lp_render(&renderer, &eyes, frame);

    lp_shape narrow = lp_default_shape();
    narrow.rx = 50.0f;
    lp_eyes_set_shape(&eyes, &narrow);
    CHECK(eyes.shape.rx == 50.0f && eyes.shape.ry == lp_default_shape().ry);
    lp_renderer_init(&renderer);
    lp_render(&renderer, &eyes, frame);
    const lp_rect thin = lp_render(&renderer, &eyes, frame);
    CHECK(thin.x1 - thin.x0 < wide.x1 - wide.x0);
}

void test_bad_input_is_ignored(void)
{
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    lp_eyes_set_pointer(&eyes, true, NAN, 0.0f);
    CHECK(!eyes.pointer.active);
    lp_eyes_set_pointer(&eyes, true, 0.0f, INFINITY);
    CHECK(!eyes.pointer.active);
    lp_eyes_set_gravity(&eyes, true, 0.0f, INFINITY);
    CHECK(!eyes.gravity.active);
    lp_eyes_set_gravity(&eyes, true, NAN, 0.0f);
    CHECK(!eyes.gravity.active);

    const float not_finite[] = {NAN, INFINITY, -INFINITY};
    const lp_shape shape = lp_default_shape();
    for (int i = 0; i < 3; i++) {
        const float bad = not_finite[i];
        lp_eyes_set_head_amount(&eyes, bad);
        CHECK(eyes.head_amount == 0.6f);
        lp_eyes_set_emotion(&eyes, LP_EMOTION_ANGRY, bad);
        CHECK(eyes.emotion == LP_EMOTION_NEUTRAL);
        const lp_shape bad_shapes[] = {
            {bad, shape.ry, shape.gap, shape.tilt},
            {shape.rx, bad, shape.gap, shape.tilt},
            {shape.rx, shape.ry, bad, shape.tilt},
            {shape.rx, shape.ry, shape.gap, bad},
        };
        for (int k = 0; k < 4; k++) {
            lp_eyes_set_shape(&eyes, &bad_shapes[k]);
            CHECK(eyes.shape.rx == shape.rx && eyes.shape.ry == shape.ry);
            CHECK(eyes.shape.gap == shape.gap && eyes.shape.tilt == shape.tilt);
        }
    }

    /* A step that is not positive and finite changes nothing at all, not even the random numbers */
    const lp_eyes before = eyes;
    lp_eyes_update(&eyes, 0.0f);
    lp_eyes_update(&eyes, NAN);
    lp_eyes_update(&eyes, INFINITY);
    lp_eyes_update(&eyes, -1.0f);
    CHECK(same_motion(&eyes, &before));
    for (int i = 0; i < 64; i++)
        lp_eyes_update(&eyes, 1.0f / 64.0f);
    CHECK(isfinite(eyes.gaze.x) && isfinite(eyes.gaze.vx) && isfinite(eyes.head.roll));

    /* A scene with an unknown emotion or blink phase leaves the eyes as they were */
    const float roll = eyes.head.roll;
    lp_scene scene = {.emotion = LP_EMOTION_COUNT, .roll = roll + 1.0f}; /* the roll would show as a change */
    lp_eyes_set_scene(&eyes, &scene, &eyes.look);
    CHECK(eyes.head.roll == roll);
    scene.emotion = LP_EMOTION_NEUTRAL;
    scene.blink_phase = (lp_blink_phase)(LP_BLINK_OPEN + 1); /* the first past the end: no phase would ever leave it */
    lp_eyes_set_scene(&eyes, &scene, &eyes.look);
    CHECK(eyes.head.roll == roll);

    /* So does a scene with a value that is not finite */
    float *const values[] = {&scene.blink_t, &scene.yaw,    &scene.pitch,        &scene.roll,
                             &scene.gaze_x,  &scene.gaze_y, &scene.particle_time};
    scene.blink_phase = LP_BLINK_NONE;
    for (size_t i = 0; i < sizeof values / sizeof values[0]; i++) {
        const float kept = *values[i];
        *values[i] = NAN;
        lp_eyes_set_scene(&eyes, &scene, &eyes.look);
        CHECK(eyes.head.roll == roll);
        *values[i] = kept;
    }

    /* And a time of particles it would take too long to replay, or that went backward */
    const float bad_times[] = {60.5f, 1e7f, -1.0f};
    for (size_t i = 0; i < sizeof bad_times / sizeof bad_times[0]; i++) {
        scene.particle_time = bad_times[i];
        lp_eyes_set_scene(&eyes, &scene, &eyes.look);
        CHECK(eyes.head.roll == roll);
    }
    scene.particle_time = 60.0f; /* the limit itself is a scene */
    lp_eyes_set_scene(&eyes, &scene, &eyes.look);
    CHECK(eyes.head.roll == scene.roll);

    /* An unknown event does nothing */
    const lp_emotion emotion = eyes.emotion;
    lp_eyes_react(&eyes, LP_EVENT_COUNT);
    lp_eyes_react(&eyes, (lp_event)-1);
    CHECK(eyes.emotion == emotion);
}

void test_bad_point_clears_a_pointer_and_a_gravity(void)
{
    /* A sensor that sends garbage points nowhere: the last valid point is not kept */
    const struct {
        float x, y;
    } bad[] = {{NAN, 0.0f}, {0.0f, INFINITY}, {-INFINITY, 0.0f}};
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    for (int i = 0; i < 3; i++) {
        lp_eyes_set_pointer(&eyes, true, 0.5f, 0.5f);
        lp_eyes_set_gravity(&eyes, true, 0.0f, 1.0f);
        CHECK(eyes.pointer.active && eyes.gravity.active);
        lp_eyes_set_pointer(&eyes, true, bad[i].x, bad[i].y);
        lp_eyes_set_gravity(&eyes, true, bad[i].x, bad[i].y);
        CHECK(!eyes.pointer.active && !eyes.gravity.active);
    }
}

void test_pointer_stays_inside_its_range(void)
{
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    lp_eyes_set_pointer(&eyes, true, 1e30f, -1e30f);
    CHECK(eyes.pointer.active);
    CHECK(eyes.pointer.x == 1.0f && eyes.pointer.y == -1.0f);
    lp_eyes_set_pointer(&eyes, true, 0.25f, -0.5f);
    CHECK(eyes.pointer.x == 0.25f && eyes.pointer.y == -0.5f);
}

void test_long_step_counts_as_one_second(void)
{
    /* In love, each second owes particles, spawned one at a time: a huge step must not loop for ever */
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    lp_eyes_set_emotion(&eyes, LP_EMOTION_LOVE, 0.0f);
    lp_eyes second = eyes;
    lp_eyes_update(&eyes, 1e4f);
    lp_eyes_update(&second, 1.0f);
    CHECK(eyes.time == 1.0f);
    CHECK(eyes.particle_count > 0);     /* hearts were owed */
    CHECK(same_motion(&eyes, &second)); /* the very same as one second */
}

void test_scene_starts_over(void)
{
    /* What came before leaves no trace: time, pointer, gravity, shaking and the rest are those of a fresh start */
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    lp_eyes_set_pointer(&eyes, true, 0.5f, 0.5f);
    lp_eyes_set_gravity(&eyes, true, 0.0f, -1.0f);
    lp_eyes_set_shaking(&eyes, true);
    lp_eyes_set_head_amount(&eyes, 0.0f);
    lp_eyes_set_follow(&eyes, false);
    for (int i = 0; i < 64; i++)
        lp_eyes_update(&eyes, 1.0f / 64.0f);

    const lp_look look = {0x123456u, LP_PUPIL_CAT, {0x111111u, 0x222222u}};
    lp_eyes_set_look(&eyes, &look);
    const lp_scene scene = {.emotion = LP_EMOTION_SAD, .yaw = 0.25f, .gaze_x = 0.5f};
    lp_eyes_set_scene(&eyes, &scene, &eyes.look); /* its own look: kept */

    lp_eyes fresh;
    lp_eyes_init(&fresh, scene.particle_seed);
    CHECK(eyes.time == 0.0f && eyes.rng.state == fresh.rng.state);
    CHECK(!eyes.pointer.active && !eyes.gravity.active && !eyes.shaking);
    CHECK(eyes.head_amount == fresh.head_amount && eyes.follow);
    CHECK(eyes.next_blink == fresh.next_blink);
    CHECK(eyes.look.pupil == LP_PUPIL_CAT && eyes.look.sclera == look.sclera);
    CHECK(eyes.emotion == LP_EMOTION_SAD && eyes.mood == LP_EMOTION_SAD);
    CHECK(eyes.head.yaw == 0.25f && eyes.gaze.x == 0.5f);
    CHECK(eyes.blink.hold == 0.2f && eyes.blink.open == 0.2f); /* a blink scene must be able to finish */
}

void test_clock_keeps_running_for_days(void)
{
    /* A float clock left growing stops after about 6 days at 60 frames/s: the engine winds it back, with its deadlines.
     * 20 minutes at 64 frames/s cross the wrap; the eyes keep blinking at their usual rate after it. */
    lp_eyes eyes;
    lp_eyes_init(&eyes, 4);
    int blinks = 0;
    float wound_back = 0.0f;
    for (int i = 0; i < 20 * 60 * 64; i++) {
        const bool open = eyes.blink.phase == LP_BLINK_NONE;
        const float before = eyes.time;
        lp_eyes_update(&eyes, 1.0f / 64.0f);
        if (eyes.time < before)
            wound_back = before + 1.0f / 64.0f - eyes.time;
        if (i >= 19 * 60 * 64 && open && eyes.blink.phase != LP_BLINK_NONE)
            blinks++;
    }
    CHECK(eyes.time < 600.0f); /* wound back once */
    /* By whole breaths (render.c: sin(time * 2.1)): the breathing does not hiccup */
    CHECK(wound_back > 0.0f);
    CHECK_NEAR(remainderf(wound_back * 2.1f, 6.2831853f), 0.0f, 0.01f);
    CHECK(blinks >= 10 && blinks <= 40);
    /* Deadlines wound back with it: each is a few seconds ahead at most */
    CHECK(eyes.next_blink - eyes.time < 8.0f);
    CHECK(eyes.next_saccade - eyes.time < 4.0f);
    CHECK(eyes.next_roll - eyes.time < 7.0f);
}
