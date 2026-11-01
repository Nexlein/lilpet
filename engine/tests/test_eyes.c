/* Engine behavior the parity vectors do not cover: limits, invalid input and the setters used by the live view */
#include "lilpet/lilpet.h"
#include "test.h"

#include <math.h>

void test_unknown_emotion_is_rejected(void)
{
    CHECK(lp_emotion_from_name("furious") == LP_EMOTION_COUNT);
    CHECK(lp_emotion_get(LP_EMOTION_COUNT) == NULL);

    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    lp_eyes_set_emotion(&eyes, LP_EMOTION_COUNT, 0.0f);
    CHECK(eyes.emotion == LP_EMOTION_NEUTRAL);
}

void test_particle_pool_drops_extra_particles(void)
{
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    for (int i = 0; i < LP_MAX_PARTICLES + 8; i++)
        lp_eyes_spawn(&eyes, LP_PARTICLE_HEART);
    CHECK(eyes.particle_count == LP_MAX_PARTICLES);
}

void test_particles_keep_the_random_sequence_when_dropped(void)
{
    /* A full pool still draws the particle's random numbers, so later animation matches the prototype */
    lp_eyes full;
    lp_eyes room;
    lp_eyes_init(&full, 9);
    lp_eyes_init(&room, 9);
    full.particle_count = LP_MAX_PARTICLES;
    lp_eyes_spawn(&full, LP_PARTICLE_SPARKLE);
    lp_eyes_spawn(&room, LP_PARTICLE_SPARKLE);
    CHECK(full.rng.state == room.rng.state);
}

void test_timed_emotion_returns_to_neutral(void)
{
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    lp_eyes_set_emotion(&eyes, LP_EMOTION_HAPPY, 0.5f);
    for (int i = 0; i < 31; i++)
        lp_eyes_update(&eyes, 1.0f / 64.0f);
    CHECK(eyes.emotion == LP_EMOTION_HAPPY);
    for (int i = 0; i < 2; i++)
        lp_eyes_update(&eyes, 1.0f / 64.0f);
    CHECK(eyes.emotion == LP_EMOTION_NEUTRAL);
}

void test_blink_does_not_restart_while_running(void)
{
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    lp_eyes_blink(&eyes);
    const uint32_t state = eyes.rng.state;
    lp_eyes_update(&eyes, 0.07f); /* into the closing phase */
    const lp_blink_phase phase = eyes.blink.phase;
    eyes.rng.state = state;
    lp_eyes_blink(&eyes);
    CHECK(eyes.blink.phase == phase);
    CHECK(eyes.rng.state == state);
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
