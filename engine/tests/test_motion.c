/* Motion: the gaze and the head, tilt and gravity, held upside down, shaking */
#include "../src/internal.h"
#include "lilpet/lilpet.h"
#include "test.h"

#include <math.h>

#define FULL_TURN (2.0f * LP_PI)
#define TOL_ANGLE 1e-3f /* the face after a long turn: rounding summed over hundreds of frames */

void test_gaze_reaches_a_held_pointer(void)
{
    lp_eyes eyes;
    lp_eyes_init(&eyes, 5);
    lp_eyes_set_pointer(&eyes, true, 0.5f, -0.3f);
    lp_test_play(&eyes, 2.0f);
    CHECK_NEAR(eyes.gaze.x, 0.5f, LP_TEST_SETTLED);
    CHECK_NEAR(eyes.gaze.y, -0.3f, LP_TEST_SETTLED); /* neutral: no gaze bias */
    /* A pointer beyond its range holds the gaze at the edge of its own, -1..1 */
    lp_eyes_set_pointer(&eyes, true, 3.0f, -3.0f);
    lp_test_play(&eyes, 2.0f);
    CHECK_NEAR(eyes.gaze.x, 1.0f, LP_TEST_SETTLED);
    CHECK_NEAR(eyes.gaze.y, -1.0f, LP_TEST_SETTLED);
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
        lp_test_play(&eyes, 2.0f);
        CHECK_NEAR(eyes.gaze.x, sides[i] * 0.25f, LP_TEST_SETTLED);
    }
}

void test_tilt_holds_upside_down_on_noise(void)
{
    /* Upside down and leaned back, gravity noise flips the angle across half a turn every frame: the face holds */
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    lp_eyes_set_head_amount(&eyes, 0.0f); /* no idle roll: only the tilt turns the face */
    float max_step = 0.0f;
    for (int i = 0; i < 640; i++) {
        lp_eyes_set_gravity(&eyes, true, i % 2 ? 0.02f : -0.02f, -0.5f);
        const float before = eyes.head.roll;
        lp_eyes_update(&eyes, 1.0f / 64.0f);
        const float step = fabsf(remainderf(eyes.head.roll - before, 6.2831853f));
        if (i >= 320 && step > max_step)
            max_step = step;
    }
    CHECK(fabsf(eyes.head.roll) > 3.0f); /* level, half a turn from the screen's upright */
    CHECK(max_step < 0.02f);
}

void test_tilt_holds_lying_flat(void)
{
    /* Turned a quarter turn, then laid flat: the face keeps its last level angle, then goes upright without sensor */
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    lp_eyes_set_head_amount(&eyes, 0.0f);
    lp_eyes_set_gravity(&eyes, true, 1.0f, 0.0f);
    for (int i = 0; i < 320; i++)
        lp_eyes_update(&eyes, 1.0f / 64.0f);
    CHECK_NEAR(eyes.head.level, -1.5707963f, 0.01f);
    lp_eyes_set_gravity(&eyes, true, 0.05f, 0.1f);
    for (int i = 0; i < 320; i++)
        lp_eyes_update(&eyes, 1.0f / 64.0f);
    CHECK_NEAR(eyes.head.level, -1.5707963f, 0.01f);
    lp_eyes_set_gravity(&eyes, false, 0.0f, 0.0f);
    for (int i = 0; i < 320; i++)
        lp_eyes_update(&eyes, 1.0f / 64.0f);
    CHECK_NEAR(eyes.head.level, 0.0f, 0.01f);
}

void test_lying_flat_does_not_slide_the_eyes(void)
{
    /* Almost no gravity left in the screen plane: mostly sensor noise, so the eyes stay where they look */
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    lp_eyes_set_head_amount(&eyes, 0.0f);
    lp_eyes_set_pointer(&eyes, true, 0.0f, 0.0f);
    lp_eyes_set_gravity(&eyes, true, 0.2f, 0.0f);
    for (int i = 0; i < 320; i++)
        lp_eyes_update(&eyes, 1.0f / 64.0f);
    CHECK_NEAR(eyes.gaze.x, 0.0f, 0.001f);
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
    for (int i = 0; i < (int)(12.0f / LP_TEST_STEP); i++) {
        if (i < (int)(8.0f / LP_TEST_STEP))
            a = 2.0f * (float)(i + 1) * LP_TEST_STEP;
        lp_eyes_set_gravity(&eyes, true, sinf(a), cosf(a));
        const float before = eyes.head.roll;
        lp_eyes_update(&eyes, LP_TEST_STEP);
        const float step = remainderf(eyes.head.roll - before, FULL_TURN);
        turned += step;
        max_step = fmaxf(max_step, fabsf(step));
    }
    CHECK(max_step < 0.05f);
    CHECK_NEAR(turned, -16.0f, TOL_ANGLE);
    CHECK_NEAR(remainderf(eyes.head.roll + a, FULL_TURN), 0.0f, TOL_ANGLE);
}

static void hold(lp_eyes *eyes, float gx, float gy, float seconds)
{
    lp_eyes_set_gravity(eyes, true, gx, gy);
    for (int i = 0; i < (int)(seconds * 64.0f); i++)
        lp_eyes_update(eyes, 1.0f / 64.0f);
}

void test_upside_down_makes_it_angry_until_righted(void)
{
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    hold(&eyes, 0.0f, -1.0f, 0.4f); /* turned over for a moment: no anger */
    hold(&eyes, 0.0f, 1.0f, 0.5f);
    CHECK(eyes.emotion == LP_EMOTION_NEUTRAL);
    hold(&eyes, 0.1f, -0.9f, 3.0f); /* held upside down: angry as long as it lasts */
    CHECK(eyes.emotion == LP_EMOTION_ANGRY);
    hold(&eyes, 0.0f, 1.0f, 1.0f); /* righted: calms down */
    CHECK(eyes.emotion == LP_EMOTION_NEUTRAL);
    hold(&eyes, 0.0f, 0.0f, 2.0f); /* lying flat is not upside down */
    CHECK(eyes.emotion == LP_EMOTION_NEUTRAL);

    /* An emotion picked for good (no duration) stays once the object is righted */
    lp_eyes_set_emotion(&eyes, LP_EMOTION_SAD, 0.0f);
    hold(&eyes, 0.0f, -1.0f, 2.0f);
    CHECK(eyes.emotion == LP_EMOTION_ANGRY);
    hold(&eyes, 0.0f, 1.0f, 1.0f);
    CHECK(eyes.emotion == LP_EMOTION_SAD);

    /* Shaken while upside down: dizzy, then angry again once the shake is over */
    lp_eyes_set_shaking(&eyes, true);
    hold(&eyes, 0.0f, -1.0f, 1.0f);
    CHECK(eyes.emotion == LP_EMOTION_DIZZY);
    lp_eyes_set_shaking(&eyes, false);
    hold(&eyes, 0.0f, -1.0f, 1.0f);
    CHECK(eyes.emotion == LP_EMOTION_ANGRY);
}

void test_holding_keeps_a_longer_emotion(void)
{
    /* Already angry for 3 s: held upside down renews 0.5 s at most, it does not cut the 3 s short */
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    lp_eyes_set_emotion(&eyes, LP_EMOTION_ANGRY, 3.0f);
    hold(&eyes, 0.0f, -1.0f, 1.0f);
    hold(&eyes, 0.0f, 1.0f, 1.0f);
    CHECK(eyes.emotion == LP_EMOTION_ANGRY);
    hold(&eyes, 0.0f, 1.0f, 1.5f);
    CHECK(eyes.emotion == LP_EMOTION_NEUTRAL);
}

/* Feeds `seconds` of shaking along x, +-amplitude g at 6 Hz; returns how long the detector said "shaken" */
static float shake_for(lp_shake *shake, float amplitude, float seconds)
{
    const float dt = 1.0f / 64.0f;
    float shaken = 0.0f;
    for (int i = 0; i < (int)(seconds * 64.0f); i++) {
        const float x = amplitude * sinf(2.0f * 3.14159265f * 6.0f * (float)i * dt);
        shaken += lp_shake_update(shake, x, 1.0f, 0.0f, dt) ? dt : 0.0f;
    }
    return shaken;
}

void test_shake_is_told_from_tilt_and_knocks(void)
{
    const float dt = 1.0f / 64.0f;
    lp_shake shake = {0};
    /* At rest, then turned slowly: always 1 g, never shaken */
    for (int i = 0; i < 640; i++) {
        const float a = (float)i * 0.01f;
        CHECK(!lp_shake_update(&shake, sinf(a), cosf(a), 0.0f, dt));
    }
    /* A knock, two 10 g samples (measured: a shake reaches 11 g), then a flick of 0.15 s: not a shake */
    CHECK(!lp_shake_update(&shake, 0.0f, 10.0f, 0.0f, dt));
    CHECK(!lp_shake_update(&shake, 0.0f, -10.0f, 0.0f, dt));
    CHECK(shake_for(&shake, 8.0f, 0.15f) == 0.0f);
    CHECK(shake_for(&shake, 0.0f, 1.0f) == 0.0f);
    /* A 2 s shake (measured: about 4 g from 1 g on average, 8 to 10 g peaks, 6 Hz): told from about 0.4 s in */
    const float shaken = shake_for(&shake, 8.0f, 2.0f);
    CHECK(shaken > 1.4f && shaken < 1.7f);
    CHECK(lp_shake_update(&shake, 0.0f, 1.0f, 0.0f, dt)); /* still the end of the shake */
    CHECK(shake_for(&shake, 0.0f, 0.75f) < 0.6f);         /* over soon after it stops */
    CHECK(!lp_shake_update(&shake, 0.0f, 1.0f, 0.0f, dt));
}

/* Feeds `seconds` of a steady acceleration of `g`; returns whether the detector ended up saying "shaken" */
static bool steady(lp_shake *shake, float g, float seconds)
{
    bool shaken = shake->active;
    for (int i = 0; i < (int)(seconds * 64.0f); i++)
        shaken = lp_shake_update(shake, g, 0.0f, 0.0f, 1.0f / 64.0f);
    return shaken;
}

void test_shake_limits(void)
{
    /* The values tuned by hand on the AtomS3R, measured from either side. Starts above 1.5 g away from 1 g... */
    lp_shake shake = {0};
    CHECK(!steady(&shake, 2.4f, 5.0f));
    shake = (lp_shake){0};
    CHECK(steady(&shake, 3.0f, 5.0f));

    /* ...and lasts until the energy is below 0.5 g */
    CHECK(steady(&shake, 1.75f, 5.0f));
    CHECK(!steady(&shake, 1.4f, 5.0f));

    /* 3 g away, the energy passes 1.5 g in 6 samples at 8 per second, then counts for 0.3 s: 20 samples more */
    shake = (lp_shake){0};
    CHECK(!steady(&shake, 4.0f, 24.0f / 64.0f));
    CHECK(steady(&shake, 4.0f, 1.0f / 64.0f));
}

void test_shake_skips_bad_samples(void)
{
    const float dt = 1.0f / 64.0f;
    lp_shake shake = {0};
    lp_shake_update(&shake, 3.0f, 0.0f, 0.0f, dt);
    const lp_shake before = shake;
    /* No time passed, no time known, then samples that are not numbers or overflow once squared */
    lp_shake_update(&shake, 3.0f, 0.0f, 0.0f, 0.0f);
    lp_shake_update(&shake, 3.0f, 0.0f, 0.0f, -dt);
    lp_shake_update(&shake, 3.0f, 0.0f, 0.0f, NAN);
    lp_shake_update(&shake, 3.0f, 0.0f, 0.0f, INFINITY);
    lp_shake_update(&shake, NAN, 0.0f, 0.0f, dt);
    lp_shake_update(&shake, 0.0f, INFINITY, 0.0f, dt);
    lp_shake_update(&shake, 0.0f, 0.0f, 1e30f, dt);
    CHECK(shake.energy == before.energy && shake.strong == before.strong && shake.active == before.active);
    CHECK(shake_for(&shake, 8.0f, 2.0f) > 1.0f); /* a real shake is still told */
}

void test_shaking_makes_it_dizzy_until_a_bit_after(void)
{
    lp_eyes eyes;
    lp_eyes_init(&eyes, 2);
    lp_eyes_set_shaking(&eyes, true);
    for (int i = 0; i < 4 * 64; i++) /* longer than the reaction itself: renewed while shaken */
        lp_eyes_update(&eyes, 1.0f / 64.0f);
    CHECK(eyes.emotion == LP_EMOTION_DIZZY);
    lp_eyes_set_shaking(&eyes, false);
    for (int i = 0; i < 64; i++)
        lp_eyes_update(&eyes, 1.0f / 64.0f);
    CHECK(eyes.emotion == LP_EMOTION_DIZZY); /* a moment after */
    for (int i = 0; i < 64; i++)
        lp_eyes_update(&eyes, 1.0f / 64.0f);
    CHECK(eyes.emotion == LP_EMOTION_NEUTRAL);
}

void test_shake_makes_it_dizzy_then_calm(void)
{
    lp_eyes eyes;
    lp_eyes_init(&eyes, 2);
    lp_eyes_set_head_amount(&eyes, 0.0f);
    lp_eyes_react(&eyes, LP_EVENT_SHAKE);
    CHECK(eyes.emotion == LP_EMOTION_DIZZY);
    /* The pupils go round: over a second the gaze visits left and right, up and down */
    float min_x = 1.0f;
    float max_x = -1.0f;
    float min_y = 1.0f;
    float max_y = -1.0f;
    for (int i = 0; i < 64; i++) {
        lp_eyes_update(&eyes, 1.0f / 64.0f);
        min_x = eyes.gaze.x < min_x ? eyes.gaze.x : min_x;
        max_x = eyes.gaze.x > max_x ? eyes.gaze.x : max_x;
        min_y = eyes.gaze.y < min_y ? eyes.gaze.y : min_y;
        max_y = eyes.gaze.y > max_y ? eyes.gaze.y : max_y;
    }
    CHECK(min_x < -0.3f && max_x > 0.3f && min_y < -0.1f && max_y > 0.2f);
    CHECK(fabsf(eyes.head.roll) <= 0.2f); /* the head wobbles a little */
    /* Back to its mood when the reaction is over, and the spinning dies down soon after */
    for (int i = 0; i < 3 * 64; i++)
        lp_eyes_update(&eyes, 1.0f / 64.0f);
    CHECK(eyes.emotion == LP_EMOTION_NEUTRAL);
    CHECK(eyes.cur.spin < 0.5f);
}
