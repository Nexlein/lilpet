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

void test_clock_keeps_running_for_days(void)
{
    /* A float clock left growing stops after about 6 days at 60 frames/s: the engine winds it back, with its deadlines.
     * 20 minutes at 64 frames/s cross the wrap; the eyes keep blinking at their usual rate after it. */
    lp_eyes eyes;
    lp_eyes_init(&eyes, 4);
    int blinks = 0;
    for (int i = 0; i < 20 * 60 * 64; i++) {
        const bool open = eyes.blink.phase == LP_BLINK_NONE;
        lp_eyes_update(&eyes, 1.0f / 64.0f);
        if (i >= 19 * 60 * 64 && open && eyes.blink.phase != LP_BLINK_NONE)
            blinks++;
    }
    CHECK(eyes.time < 600.0f); /* wound back once */
    CHECK(blinks >= 10 && blinks <= 40);
    /* Deadlines wound back with it: each is a few seconds ahead at most */
    CHECK(eyes.next_blink - eyes.time < 8.0f);
    CHECK(eyes.next_saccade - eyes.time < 4.0f);
    CHECK(eyes.next_roll - eyes.time < 7.0f);
}

void test_bad_input_is_ignored(void)
{
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    lp_eyes_set_pointer(&eyes, true, NAN, 0.0f);
    CHECK(!eyes.pointer.active);
    lp_eyes_set_gravity(&eyes, true, 0.0f, INFINITY);
    CHECK(!eyes.gravity.active);
    const float time = eyes.time;
    lp_eyes_update(&eyes, 0.0f);
    lp_eyes_update(&eyes, NAN);
    lp_eyes_update(&eyes, -1.0f);
    CHECK(eyes.time == time);
    for (int i = 0; i < 64; i++)
        lp_eyes_update(&eyes, 1.0f / 64.0f);
    CHECK(isfinite(eyes.gaze.x) && isfinite(eyes.gaze.vx) && isfinite(eyes.head.roll));

    /* A scene with an unknown emotion or blink phase leaves the eyes as they were */
    lp_scene scene = {.emotion = LP_EMOTION_COUNT};
    const float roll = eyes.head.roll;
    lp_eyes_set_scene(&eyes, &scene, &eyes.look);
    scene.emotion = LP_EMOTION_NEUTRAL;
    scene.blink_phase = (lp_blink_phase)9;
    lp_eyes_set_scene(&eyes, &scene, &eyes.look);
    CHECK(eyes.head.roll == roll);
}

void test_tap_makes_it_happy_for_a_moment(void)
{
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    lp_eyes_react(&eyes, LP_EVENT_TAP);
    CHECK(eyes.emotion == LP_EMOTION_HAPPY);
    CHECK(eyes.particle_count == 4); /* a burst of sparkles */
    for (int i = 0; i < 112; i++)    /* 1.75 s */
        lp_eyes_update(&eyes, 1.0f / 64.0f);
    CHECK(eyes.emotion == LP_EMOTION_HAPPY);
    for (int i = 0; i < 8; i++) /* past 1.8 s */
        lp_eyes_update(&eyes, 1.0f / 64.0f);
    CHECK(eyes.emotion == LP_EMOTION_NEUTRAL);
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
    /* Back to its mood after 2 s, and the spinning dies down soon after */
    for (int i = 0; i < 3 * 64; i++)
        lp_eyes_update(&eyes, 1.0f / 64.0f);
    CHECK(eyes.emotion == LP_EMOTION_NEUTRAL);
    CHECK(eyes.cur.spin < 0.5f);
}
