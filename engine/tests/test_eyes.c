/* Engine behavior that the prototype does not cover: limits and invalid input */
#include "lilpet/lilpet.h"
#include "test.h"

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
