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

    const float time = eyes.time;
    lp_eyes_update(&eyes, 0.0f);
    lp_eyes_update(&eyes, NAN);
    lp_eyes_update(&eyes, INFINITY);
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

    /* So does a scene with a value that is not finite */
    float *const values[] = {&scene.blink_t, &scene.yaw,    &scene.pitch,        &scene.roll,
                             &scene.gaze_x,  &scene.gaze_y, &scene.particle_time};
    scene.blink_phase = LP_BLINK_NONE;
    scene.roll = roll + 1.0f; /* would show as a change */
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
    lp_eyes_update(&eyes, 1e4f);
    CHECK(eyes.time == 1.0f);
    CHECK(eyes.particle_count <= 3);
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

void test_repeated_reaction_goes_on_without_a_new_burst(void)
{
    const lp_reaction *like = lp_reaction_get(LP_EVENT_GESTURE_LIKE);
    const lp_reaction *tap = lp_reaction_get(LP_EVENT_TAP);
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    lp_eyes_react(&eyes, LP_EVENT_GESTURE_LIKE);
    CHECK(eyes.particle_count == like->sparkles);

    /* Almost half a second later: it goes on, with no more sparkles and no random draw */
    for (int i = 0; i < 31; i++)
        lp_eyes_update(&eyes, 1.0f / 64.0f);
    const uint32_t random = eyes.rng.state;
    lp_eyes_react(&eyes, LP_EVENT_GESTURE_LIKE);
    CHECK(eyes.particle_count == like->sparkles);
    CHECK(eyes.emotion_timer == like->duration);
    CHECK(eyes.rng.state == random);

    /* Another event of the same emotion is the same reaction, and a shorter one cuts nothing short */
    lp_eyes_react(&eyes, LP_EVENT_TAP);
    CHECK(eyes.particle_count == like->sparkles);
    CHECK(eyes.emotion_timer == like->duration);

    /* Half a second after the burst: a new one, which still cuts nothing short */
    lp_eyes_update(&eyes, 1.0f / 64.0f);
    const float timer = eyes.emotion_timer;
    CHECK(timer > tap->duration);
    lp_eyes_react(&eyes, LP_EVENT_TAP);
    CHECK(eyes.particle_count == like->sparkles + tap->sparkles);
    CHECK(eyes.emotion_timer == timer);
}

void test_another_emotion_reacts_at_once_without_a_new_burst(void)
{
    const lp_reaction *like = lp_reaction_get(LP_EVENT_GESTURE_LIKE);
    const lp_reaction *dislike = lp_reaction_get(LP_EVENT_GESTURE_DISLIKE);
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    lp_eyes_react(&eyes, LP_EVENT_GESTURE_LIKE);
    lp_eyes_react(&eyes, LP_EVENT_GESTURE_DISLIKE);
    CHECK(eyes.emotion == dislike->emotion);
    CHECK(eyes.emotion_timer == dislike->duration);
    lp_eyes_react(&eyes, LP_EVENT_GESTURE_LIKE);
    CHECK(eyes.emotion == like->emotion);
    CHECK(eyes.emotion_timer == like->duration);
    CHECK(eyes.particle_count == like->sparkles);
}

void test_reaction_without_sparkles_leaves_the_wait_alone(void)
{
    const lp_reaction *like = lp_reaction_get(LP_EVENT_GESTURE_LIKE);
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);

    /* It starts no wait: the sparkles right after it burst */
    lp_eyes_react(&eyes, LP_EVENT_GESTURE_DISLIKE);
    CHECK(eyes.burst_wait == 0.0f);
    lp_eyes_react(&eyes, LP_EVENT_GESTURE_LIKE);
    CHECK(eyes.particle_count == like->sparkles);

    /* It does not start the wait over: half a second after the burst, the next one comes */
    for (int i = 0; i < 31; i++)
        lp_eyes_update(&eyes, 1.0f / 64.0f);
    const float wait = eyes.burst_wait;
    CHECK(wait > 0.0f);
    lp_eyes_react(&eyes, LP_EVENT_GESTURE_DISLIKE);
    CHECK(eyes.burst_wait == wait);
    lp_eyes_update(&eyes, 1.0f / 64.0f);
    lp_eyes_react(&eyes, LP_EVENT_GESTURE_LIKE);
    CHECK(eyes.particle_count == 2 * like->sparkles);
}

/* 10 events a second for 10 s, taking turns; returns the most particles at once */
static int chatter(lp_eyes *eyes, lp_event first, lp_event second)
{
    int most = 0;
    for (int i = 0; i < 10 * 60; i++) {
        if (i % 6 == 0)
            lp_eyes_react(eyes, i % 12 == 0 ? first : second);
        lp_eyes_update(eyes, 1.0f / 60.0f);
        most = eyes->particle_count > most ? eyes->particle_count : most;
    }
    return most;
}

void test_chatty_events_never_fill_the_screen(void)
{
    /* Bursts are half a second apart, and sparkles last 1.1 s at most */
    const int sparkles = lp_reaction_get(LP_EVENT_GESTURE_LIKE)->sparkles;
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    CHECK(chatter(&eyes, LP_EVENT_GESTURE_LIKE, LP_EVENT_GESTURE_LIKE) <= 3 * sparkles);
    CHECK(eyes.emotion == LP_EMOTION_HAPPY);

    /* Whatever the emotion in between, which still follows each event */
    lp_eyes_init(&eyes, 1);
    CHECK(chatter(&eyes, LP_EVENT_GESTURE_LIKE, LP_EVENT_EXPRESSION_SAD) <= 3 * sparkles);
    CHECK(eyes.emotion == LP_EMOTION_SAD);
}

void test_scene_forgets_the_last_burst(void)
{
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    lp_eyes_react(&eyes, LP_EVENT_TAP);
    const lp_scene scene = {.emotion = LP_EMOTION_HAPPY};
    lp_eyes_set_scene(&eyes, &scene, &eyes.look);
    lp_eyes_react(&eyes, LP_EVENT_TAP);
    CHECK(eyes.particle_count == lp_reaction_get(LP_EVENT_TAP)->sparkles);
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
