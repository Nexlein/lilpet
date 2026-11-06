#include "lilpet/lilpet.h"
#include "test.h"

#define STEP (1.0f / 60.0f)

static void start(lp_eyes *eyes, lp_senses *senses)
{
    lp_eyes_init(eyes, 1);
    lp_senses_init(senses);
}

/* Runs `seconds` of frames, each with the same input */
static void run_frames(lp_senses *senses, lp_eyes *eyes, const float *accel, bool pressed, float seconds)
{
    for (int i = 0; (float)i * STEP < seconds; i++) {
        lp_senses_update(senses, eyes, accel, pressed, STEP);
        lp_eyes_update(eyes, STEP);
    }
}

void test_senses_smooth_gravity(void)
{
    lp_eyes eyes;
    lp_senses senses;
    start(&eyes, &senses);
    const float flat_left[3] = {-1.0f, 0.0f, 0.0f};
    lp_senses_update(&senses, &eyes, flat_left, false, STEP);
    CHECK(eyes.gravity.active);
    CHECK(eyes.gravity.x > -0.5f); /* smoothed: one sample does not jump there */
    run_frames(&senses, &eyes, flat_left, false, 1.0f);
    CHECK_NEAR(eyes.gravity.x, -1.0f, 0.01f);
}

void test_senses_forget_a_sensor_that_stopped(void)
{
    lp_eyes eyes;
    lp_senses senses;
    start(&eyes, &senses);
    const float upside_down[3] = {0.0f, -1.0f, 0.0f};
    run_frames(&senses, &eyes, upside_down, false, 1.5f);
    CHECK(eyes.emotion == LP_EMOTION_ANGRY);

    run_frames(&senses, &eyes, NULL, false, 0.1f); /* a few failed reads change nothing */
    CHECK(eyes.gravity.active);
    run_frames(&senses, &eyes, NULL, false, 2.0f);
    CHECK(!eyes.gravity.active);
    CHECK(eyes.upside_down == 0.0f);
    CHECK(eyes.emotion == LP_EMOTION_NEUTRAL);
}

void test_senses_stop_a_shake_when_the_sensor_stops(void)
{
    lp_eyes eyes;
    lp_senses senses;
    start(&eyes, &senses);
    for (int i = 0; i < 60; i++) {
        const float sample[3] = {i % 2 ? 6.0f : -6.0f, 1.0f, 0.0f};
        lp_senses_update(&senses, &eyes, sample, false, STEP);
        lp_eyes_update(&eyes, STEP);
    }
    CHECK(eyes.shaking);
    run_frames(&senses, &eyes, NULL, false, 0.5f);
    CHECK(!eyes.shaking);
}

void test_senses_skip_a_bad_sample(void)
{
    lp_eyes eyes;
    lp_senses senses;
    start(&eyes, &senses);
    const float good[3] = {0.0f, 1.0f, 0.0f};
    const float bad[3] = {NAN, 1.0f, 0.0f};
    run_frames(&senses, &eyes, good, false, 0.5f);
    lp_senses_update(&senses, &eyes, bad, false, STEP);
    CHECK(isfinite(senses.gravity[0]));
    CHECK(isfinite(senses.shake.energy));
    CHECK(eyes.gravity.active);
}

void test_senses_skip_a_bad_step(void)
{
    lp_eyes eyes;
    lp_senses senses;
    start(&eyes, &senses);
    const float good[3] = {0.0f, 1.0f, 0.0f};
    run_frames(&senses, &eyes, good, false, 0.5f);
    lp_senses_update(&senses, &eyes, good, false, NAN);
    lp_senses_update(&senses, &eyes, good, false, INFINITY);
    CHECK(isfinite(senses.gravity[0]) && isfinite(senses.gravity[1]));
    CHECK(isfinite(senses.motion_age) && isfinite(senses.button_age) && isfinite(senses.face_age));
}

void test_senses_debounce_the_button(void)
{
    lp_eyes eyes;
    lp_senses senses;
    start(&eyes, &senses);
    run_frames(&senses, &eyes, NULL, false, 0.1f);
    lp_senses_update(&senses, &eyes, NULL, true, STEP);
    CHECK(eyes.emotion == LP_EMOTION_HAPPY);

    lp_eyes_set_emotion(&eyes, LP_EMOTION_NEUTRAL, 0.0f);
    lp_senses_update(&senses, &eyes, NULL, false, STEP); /* the contact bounces: released for one frame */
    lp_senses_update(&senses, &eyes, NULL, true, STEP);
    CHECK(eyes.emotion == LP_EMOTION_NEUTRAL);

    run_frames(&senses, &eyes, NULL, false, 0.1f); /* a real release, then a real press */
    lp_senses_update(&senses, &eyes, NULL, true, STEP);
    CHECK(eyes.emotion == LP_EMOTION_HAPPY);
}

void test_senses_react_to_a_press_not_a_release(void)
{
    lp_eyes eyes;
    lp_senses senses;
    start(&eyes, &senses);
    run_frames(&senses, &eyes, NULL, false, 0.1f);
    run_frames(&senses, &eyes, NULL, true, 1.0f); /* a long press, past the settling time */
    CHECK(eyes.emotion == LP_EMOTION_HAPPY);
    lp_eyes_set_emotion(&eyes, LP_EMOTION_NEUTRAL, 0.0f);
    lp_senses_update(&senses, &eyes, NULL, false, STEP);
    CHECK(eyes.emotion == LP_EMOTION_NEUTRAL);
}

void test_senses_let_go_of_a_face(void)
{
    lp_eyes eyes;
    lp_senses senses;
    start(&eyes, &senses);
    lp_senses_face(&senses, &eyes, true, 0.5f, -0.25f);
    CHECK(eyes.pointer.active);
    CHECK_NEAR(eyes.pointer.x, 0.5f, 0.0001f);
    run_frames(&senses, &eyes, NULL, false, 0.3f);
    CHECK(eyes.pointer.active);
    run_frames(&senses, &eyes, NULL, false, 0.3f); /* the camera went quiet */
    CHECK(!eyes.pointer.active);

    lp_senses_face(&senses, &eyes, true, 0.0f, 0.0f);
    lp_senses_face(&senses, &eyes, false, 0.0f, 0.0f); /* the camera says it is gone */
    CHECK(!eyes.pointer.active);
    lp_senses_face(&senses, &eyes, true, NAN, 0.0f);
    CHECK(!eyes.pointer.active);
}

void test_senses_greet_only_a_new_face(void)
{
    lp_eyes eyes;
    lp_senses senses;
    start(&eyes, &senses);
    const lp_emotion seen = lp_reaction_get(LP_EVENT_FACE_SEEN)->emotion;
    lp_senses_face(&senses, &eyes, true, NAN, 0.0f); /* not a position: nobody to greet */
    CHECK(eyes.emotion == LP_EMOTION_NEUTRAL);
    lp_senses_face(&senses, &eyes, true, 0.0f, 0.0f);
    CHECK(eyes.emotion == seen);

    lp_eyes_set_emotion(&eyes, LP_EMOTION_NEUTRAL, 0.0f);
    run_frames(&senses, &eyes, NULL, false, 1.0f); /* lost for a moment: detection flickers */
    lp_senses_face(&senses, &eyes, true, 0.0f, 0.0f);
    CHECK(eyes.emotion == LP_EMOTION_NEUTRAL);

    run_frames(&senses, &eyes, NULL, false, 3.5f); /* really gone, then back */
    lp_senses_face(&senses, &eyes, true, 0.0f, 0.0f);
    CHECK(eyes.emotion == seen);
}
