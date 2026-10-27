/*
 * Minimal test runner for the engine, run on the host (no dependency).
 * Add a test: write `void test_x(void)` in a C file of this folder, then list it in LP_TESTS below.
 */
#ifndef LILPET_TEST_H
#define LILPET_TEST_H

#include <math.h>
#include <stdio.h>
#include <string.h>

/* Every test function, one per line */
#define LP_TESTS(X)                                                                                                    \
    X(test_version_matches_macros)                                                                                     \
    X(test_rng_matches_prototype)                                                                                      \
    X(test_emotions_match_prototype)                                                                                   \
    X(test_blink_poses_match_prototype)                                                                                \
    X(test_eye_poses_match_prototype)                                                                                  \
    X(test_simulations_match_prototype)                                                                                \
    X(test_scenes_match_prototype)                                                                                     \
    X(test_unknown_emotion_is_rejected)                                                                                \
    X(test_particle_pool_drops_extra_particles)                                                                        \
    X(test_particles_keep_the_random_sequence_when_dropped)                                                            \
    X(test_timed_emotion_returns_to_neutral)                                                                           \
    X(test_blink_does_not_restart_while_running)                                                                       \
    X(test_rgb565_conversion)                                                                                          \
    X(test_render_first_frame_is_full_screen)                                                                          \
    X(test_render_puts_colors_where_they_belong)                                                                       \
    X(test_render_glint_is_white)                                                                                      \
    X(test_closed_eye_shows_only_the_stroke)                                                                           \
    X(test_render_is_deterministic)                                                                                    \
    X(test_dirty_region_redraws_everything_that_changed)                                                               \
    X(test_dirty_region_stays_inside_the_screen)                                                                       \
    X(test_extreme_poses_render_inside_the_frame)

#define LP_DECLARE_TEST(fn) void fn(void);
LP_TESTS(LP_DECLARE_TEST)

extern int lp_test_failures;

#define LP_FAIL(...)                                                                                                   \
    do {                                                                                                               \
        fprintf(stderr, "%s:%d: ", __FILE__, __LINE__);                                                                \
        fprintf(stderr, __VA_ARGS__);                                                                                  \
        fputc('\n', stderr);                                                                                           \
        lp_test_failures++;                                                                                            \
    } while (0)

#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond))                                                                                                   \
            LP_FAIL("CHECK failed: %s", #cond);                                                                        \
    } while (0)

#define CHECK_STR_EQ(actual, expected)                                                                                 \
    do {                                                                                                               \
        const char *a_ = (actual);                                                                                     \
        const char *e_ = (expected);                                                                                   \
        if (strcmp(a_, e_) != 0)                                                                                       \
            LP_FAIL("%s is \"%s\", expected \"%s\"", #actual, a_, e_);                                                 \
    } while (0)

#define CHECK_NEAR(actual, expected, tolerance)                                                                        \
    do {                                                                                                               \
        float a_ = (actual);                                                                                           \
        float e_ = (expected);                                                                                         \
        if (fabsf(a_ - e_) > (tolerance))                                                                              \
            LP_FAIL("%s is %g, expected %g", #actual, (double)a_, (double)e_);                                         \
    } while (0)

#endif /* LILPET_TEST_H */
