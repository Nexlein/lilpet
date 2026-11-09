/* Emotions and reactions: the mood, emotions for a while, reactions and their bursts of sparkles, particles */
#include "lilpet/lilpet.h"
#include "test.h"

#include <math.h>

void test_emotion_without_a_positive_duration_is_the_mood(void)
{
    const float for_good[] = {0.0f, -1.0f};
    for (int i = 0; i < 2; i++) {
        lp_eyes eyes;
        lp_eyes_init(&eyes, 1);
        lp_eyes_set_emotion(&eyes, LP_EMOTION_SAD, for_good[i]);
        CHECK(eyes.mood == LP_EMOTION_SAD);
        lp_test_play(&eyes, 10.0f);
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

/* Each field of lp_expression is checked below: a new one must be added there */
_Static_assert(sizeof(lp_expression) == 11 * sizeof(float), "check every field of lp_expression");

void test_every_emotion_settles_on_its_expression(void)
{
    for (int i = 0; i < LP_EMOTION_COUNT; i++) {
        const lp_emotion_def *def = lp_emotion_get((lp_emotion)i);
        lp_eyes eyes;
        lp_eyes_init(&eyes, 1);
        lp_eyes_set_emotion(&eyes, (lp_emotion)i, 0.0f);
        lp_test_play(&eyes, 3.0f);
        const int before = lp_test_failures;
        const lp_expression *cur = &eyes.cur;
        const lp_expression *want = &def->expr;
        CHECK_NEAR(cur->tl, want->tl, LP_TEST_SETTLED);
        CHECK_NEAR(cur->ts, want->ts, LP_TEST_SETTLED);
        CHECK_NEAR(cur->bl, want->bl, LP_TEST_SETTLED);
        CHECK_NEAR(cur->sq, want->sq, LP_TEST_SETTLED);
        CHECK_NEAR(cur->cr, want->cr, LP_TEST_SETTLED);
        CHECK_NEAR(cur->rot, want->rot, LP_TEST_SETTLED);
        CHECK_NEAR(cur->hl, want->hl, LP_TEST_SETTLED);
        CHECK_NEAR(cur->ps, want->ps, LP_TEST_SETTLED);
        CHECK_NEAR(cur->es, want->es, LP_TEST_SETTLED);
        CHECK_NEAR(cur->gy, want->gy, LP_TEST_SETTLED);
        CHECK_NEAR(cur->spin, want->spin, LP_TEST_SETTLED);
        if (lp_test_failures != before)
            fprintf(stderr, "  in %s\n", def->name);
    }
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

void test_mood_that_is_the_reaction_stays_after_it(void)
{
    const lp_reaction *tap = lp_reaction_get(LP_EVENT_TAP);
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    lp_eyes_set_emotion(&eyes, tap->emotion, 0.0f);
    lp_eyes_react(&eyes, LP_EVENT_TAP);
    CHECK(eyes.emotion_timer == tap->duration);
    lp_test_play(&eyes, tap->duration + 0.25f);
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
    lp_test_play(&eyes, tap->duration + 0.25f);
    CHECK(eyes.emotion == LP_EMOTION_SAD);
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
    /* A full pool still draws the particle's random numbers, so later animation does not depend on the pool */
    lp_eyes full;
    lp_eyes room;
    lp_eyes_init(&full, 9);
    lp_eyes_init(&room, 9);
    full.particle_count = LP_MAX_PARTICLES;
    lp_eyes_spawn(&full, LP_PARTICLE_SPARKLE);
    lp_eyes_spawn(&room, LP_PARTICLE_SPARKLE);
    CHECK(full.rng.state == room.rng.state);
}

void test_love_sends_hearts_at_its_rate_until_they_fade(void)
{
    const lp_emotion_def *love = lp_emotion_get(LP_EMOTION_LOVE);
    CHECK(love->emit == LP_PARTICLE_HEART);
    lp_eyes eyes;
    lp_eyes_init(&eyes, 11);
    lp_eyes_set_emotion(&eyes, LP_EMOTION_LOVE, 3.0f);
    /* 2.5 s, shorter than a heart lives (2.6 s at least): every heart born is still there */
    lp_test_play(&eyes, 2.5f);
    CHECK(eyes.particle_count == (int)(love->emit_rate * 2.5f));
    for (int i = 0; i < eyes.particle_count; i++)
        CHECK(eyes.particles[i].type == LP_PARTICLE_HEART);
    /* Neutral again at 3 s: the last hearts are gone 4 s later at most */
    lp_test_play(&eyes, 5.5f);
    CHECK(eyes.emotion == LP_EMOTION_NEUTRAL);
    CHECK(eyes.particle_count == 0);
}
