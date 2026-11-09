/* Behavior of the eyes with literal values: mood, reactions, bad input, random numbers, gaze, tilt, blink, particles */
#include "../src/internal.h"
#include "lilpet/lilpet.h"
#include "test.h"

#include <math.h>

#define STEP        (1.0f / 64.0f) /* exact in float */
#define FULL_TURN   (2.0f * LP_PI)
#define TOL_SETTLED 1e-4f /* an easing given seconds to settle: what is left of the distance, and float rounding */
#define TOL_ANGLE   1e-3f /* the face after a long turn: rounding summed over hundreds of frames */

/* Plays `seconds` of animation, a STEP at a time */
static void play(lp_eyes *eyes, float seconds)
{
    for (int i = 0; i < (int)(seconds / STEP); i++)
        lp_eyes_update(eyes, STEP);
}

void test_emotion_without_a_positive_duration_is_the_mood(void)
{
    const float for_good[] = {0.0f, -1.0f};
    for (int i = 0; i < 2; i++) {
        lp_eyes eyes;
        lp_eyes_init(&eyes, 1);
        lp_eyes_set_emotion(&eyes, LP_EMOTION_SAD, for_good[i]);
        CHECK(eyes.mood == LP_EMOTION_SAD);
        play(&eyes, 10.0f);
        CHECK(eyes.emotion == LP_EMOTION_SAD);
    }
    /* A positive duration is for a while: the mood stays */
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    lp_eyes_set_emotion(&eyes, LP_EMOTION_SAD, 1.0f);
    CHECK(eyes.emotion == LP_EMOTION_SAD && eyes.mood == LP_EMOTION_NEUTRAL);
}

void test_duration_that_is_not_finite_leaves_the_mood(void)
{
    const float not_finite[] = {NAN, INFINITY, -INFINITY};
    for (int i = 0; i < 3; i++) {
        lp_eyes eyes;
        lp_eyes_init(&eyes, 1);
        lp_eyes_set_emotion(&eyes, LP_EMOTION_SAD, 0.0f);
        lp_eyes_set_emotion(&eyes, LP_EMOTION_ANGRY, not_finite[i]);
        CHECK(eyes.emotion == LP_EMOTION_SAD && eyes.mood == LP_EMOTION_SAD);
    }
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

void test_mood_that_is_the_reaction_stays_after_it(void)
{
    const lp_reaction *tap = lp_reaction_get(LP_EVENT_TAP);
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    lp_eyes_set_emotion(&eyes, tap->emotion, 0.0f);
    lp_eyes_react(&eyes, LP_EVENT_TAP);
    CHECK(eyes.emotion_timer == tap->duration);
    play(&eyes, tap->duration + 0.25f);
    CHECK(eyes.emotion == tap->emotion && eyes.mood == tap->emotion);
    CHECK(eyes.emotion_timer == 0.0f); /* for good again */
}

void test_reaction_after_a_scene_returns_to_its_emotion(void)
{
    const lp_reaction *tap = lp_reaction_get(LP_EVENT_TAP);
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    const lp_scene scene = {.emotion = LP_EMOTION_SAD};
    lp_eyes_set_scene(&eyes, &scene, &eyes.look);
    lp_eyes_react(&eyes, LP_EVENT_TAP);
    CHECK(eyes.emotion == tap->emotion);
    play(&eyes, tap->duration + 0.25f);
    CHECK(eyes.emotion == LP_EMOTION_SAD);
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

void test_clamp_with_a_negative_limit_gives_its_opposite(void)
{
    CHECK(lp_clampf(0.25f, 1.0f) == 0.25f);
    CHECK(lp_clampf(2.0f, 1.0f) == 1.0f);
    CHECK(lp_clampf(-2.0f, 1.0f) == -1.0f);
    /* A pupil bigger than its eye makes the limit negative: max(-lim, min(lim, v)) is then -lim, whatever v */
    CHECK(lp_clampf(33.5f, -6.75f) == 6.75f);
    CHECK(lp_clampf(-33.5f, -6.75f) == 6.75f);
    CHECK(lp_clampf(0.0f, -6.75f) == 6.75f);
}

void test_every_emotion_and_event_is_found_by_its_name(void)
{
    for (int i = 0; i < LP_EMOTION_COUNT; i++)
        CHECK(lp_emotion_from_name(lp_emotion_get((lp_emotion)i)->name) == (lp_emotion)i);
    for (int i = 0; i < LP_EVENT_COUNT; i++)
        CHECK(lp_event_from_name(lp_reaction_get((lp_event)i)->name) == (lp_event)i);
    /* Names the protocol sends (notes/app.md) */
    CHECK(lp_emotion_from_name("love") == LP_EMOTION_LOVE);
    CHECK(lp_event_from_name("gesture_like") == LP_EVENT_GESTURE_LIKE);
}

void test_turning_keeps_the_face_level_across_the_wrap(void)
{
    /* Gravity turning at 2 rad/s for 8 s (16 rad: past +-pi three times), then held: the face turns back as much, a
     * frame at a time (2 rad/s / 64 = 0.031 rad), and ends level. Easing the long way round at the wrap would step
     * about 0.3 rad a frame and leave a full turn missing. */
    lp_eyes eyes;
    lp_eyes_init(&eyes, 19);
    lp_eyes_set_head_amount(&eyes, 0.0f); /* no idle roll: only the tilt turns the face */
    float a = 0.0f;
    float turned = 0.0f;
    float max_step = 0.0f;
    for (int i = 0; i < (int)(12.0f / STEP); i++) {
        if (i < (int)(8.0f / STEP))
            a = 2.0f * (float)(i + 1) * STEP;
        lp_eyes_set_gravity(&eyes, true, sinf(a), cosf(a));
        const float before = eyes.head.roll;
        lp_eyes_update(&eyes, STEP);
        const float step = remainderf(eyes.head.roll - before, FULL_TURN);
        turned += step;
        max_step = fmaxf(max_step, fabsf(step));
    }
    CHECK(max_step < 0.05f);
    CHECK_NEAR(turned, -16.0f, TOL_ANGLE);
    CHECK_NEAR(remainderf(eyes.head.roll + a, FULL_TURN), 0.0f, TOL_ANGLE);
}

void test_gaze_reaches_a_held_pointer(void)
{
    lp_eyes eyes;
    lp_eyes_init(&eyes, 5);
    lp_eyes_set_pointer(&eyes, true, 0.5f, -0.3f);
    play(&eyes, 2.0f);
    CHECK_NEAR(eyes.gaze.x, 0.5f, TOL_SETTLED);
    CHECK_NEAR(eyes.gaze.y, -0.3f, TOL_SETTLED); /* neutral: no gaze bias */
    /* A pointer beyond its range holds the gaze at the edge of its own, -1..1 */
    lp_eyes_set_pointer(&eyes, true, 3.0f, -3.0f);
    play(&eyes, 2.0f);
    CHECK_NEAR(eyes.gaze.x, 1.0f, TOL_SETTLED);
    CHECK_NEAR(eyes.gaze.y, -1.0f, TOL_SETTLED);
}

void test_tilt_slides_the_eyes_toward_the_low_side(void)
{
    /* Turned 30 degrees clockwise, gravity toward the screen's right: the right side is low and the gaze slides right,
     * by half the sine of the angle (TILT_SLIDE in eyes.c). Turned the other way, left. */
    const float sides[] = {1.0f, -1.0f};
    for (int i = 0; i < 2; i++) {
        lp_eyes eyes;
        lp_eyes_init(&eyes, 1);
        lp_eyes_set_pointer(&eyes, true, 0.0f, 0.0f); /* looking straight ahead, with no saccade */
        lp_eyes_set_gravity(&eyes, true, sides[i] * 0.5f, 0.8660254f);
        play(&eyes, 2.0f);
        CHECK_NEAR(eyes.gaze.x, sides[i] * 0.25f, TOL_SETTLED);
    }
}

/* Each field of lp_expression is checked below: a new one must be added there */
_Static_assert(sizeof(lp_expression) == 11 * sizeof(float), "check every field of lp_expression");

void test_every_emotion_settles_on_its_expression(void)
{
    for (int i = 0; i < LP_EMOTION_COUNT; i++) {
        const lp_emotion_def *def = lp_emotion_get((lp_emotion)i);
        lp_eyes eyes;
        lp_eyes_init(&eyes, 1);
        lp_eyes_set_emotion(&eyes, (lp_emotion)i, 0.0f);
        play(&eyes, 3.0f);
        const int before = lp_test_failures;
        const lp_expression *cur = &eyes.cur;
        const lp_expression *want = &def->expr;
        CHECK_NEAR(cur->tl, want->tl, TOL_SETTLED);
        CHECK_NEAR(cur->ts, want->ts, TOL_SETTLED);
        CHECK_NEAR(cur->bl, want->bl, TOL_SETTLED);
        CHECK_NEAR(cur->sq, want->sq, TOL_SETTLED);
        CHECK_NEAR(cur->cr, want->cr, TOL_SETTLED);
        CHECK_NEAR(cur->rot, want->rot, TOL_SETTLED);
        CHECK_NEAR(cur->hl, want->hl, TOL_SETTLED);
        CHECK_NEAR(cur->ps, want->ps, TOL_SETTLED);
        CHECK_NEAR(cur->es, want->es, TOL_SETTLED);
        CHECK_NEAR(cur->gy, want->gy, TOL_SETTLED);
        CHECK_NEAR(cur->spin, want->spin, TOL_SETTLED);
        if (lp_test_failures != before)
            fprintf(stderr, "  in %s\n", def->name);
    }
}

void test_blink_pose_stops_at_the_end_of_its_phase(void)
{
    /* A scene may freeze a phase past its length: the lid and the eye's scale stay at the end of the phase */
    const struct {
        lp_blink_phase phase;
        float t, lid, sx, sy;
        bool closed;
    } poses[] = {
        {LP_BLINK_ANTICIP, 0.5f * LP_BLINK_ANTICIP_TIME, 0.05f, 1.025f, 0.985f, false}, /* halfway */
        {LP_BLINK_ANTICIP, 1.0f, 0.1f, 1.05f, 0.97f, false},
        {LP_BLINK_CLOSE, 1.0f, 1.0f, 1.05f, 0.97f, false},
        {LP_BLINK_CLOSED, 1.0f, 1.0f, 1.04f, 1.0f, true},
        {LP_BLINK_OPEN, 1.0f, 0.0f, 1.0f, 1.0f, false},
        {LP_BLINK_NONE, 1.0f, 0.0f, 1.0f, 1.0f, false},
    };
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    for (size_t i = 0; i < sizeof poses / sizeof poses[0]; i++) {
        eyes.blink.phase = poses[i].phase;
        eyes.blink.t = poses[i].t;
        eyes.blink.open = 0.2f;
        const lp_blink_pose pose = lp_eyes_blink_pose(&eyes);
        CHECK_NEAR(pose.lid, poses[i].lid, 1e-6f);
        CHECK_NEAR(pose.sx, poses[i].sx, 1e-6f);
        CHECK_NEAR(pose.sy, poses[i].sy, 1e-6f);
        CHECK(pose.closed == poses[i].closed);
    }
}

void test_love_sends_hearts_at_its_rate_until_they_fade(void)
{
    const lp_emotion_def *love = lp_emotion_get(LP_EMOTION_LOVE);
    CHECK(love->emit == LP_PARTICLE_HEART);
    lp_eyes eyes;
    lp_eyes_init(&eyes, 11);
    lp_eyes_set_emotion(&eyes, LP_EMOTION_LOVE, 3.0f);
    /* 2.5 s, shorter than a heart lives (2.6 s at least): every heart born is still there */
    play(&eyes, 2.5f);
    CHECK(eyes.particle_count == (int)(love->emit_rate * 2.5f));
    for (int i = 0; i < eyes.particle_count; i++)
        CHECK(eyes.particles[i].type == LP_PARTICLE_HEART);
    /* Neutral again at 3 s: the last hearts are gone 4 s later at most */
    play(&eyes, 5.5f);
    CHECK(eyes.emotion == LP_EMOTION_NEUTRAL);
    CHECK(eyes.particle_count == 0);
}
