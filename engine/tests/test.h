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
    X(test_check_near_fails_on_nan)                                                                                    \
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
    X(test_senses_smooth_gravity)                                                                                      \
    X(test_senses_forget_a_sensor_that_stopped)                                                                        \
    X(test_senses_stop_a_shake_when_the_sensor_stops)                                                                  \
    X(test_senses_skip_a_bad_sample)                                                                                   \
    X(test_senses_skip_a_bad_step)                                                                                     \
    X(test_senses_debounce_the_button)                                                                                 \
    X(test_senses_let_go_of_a_face)                                                                                    \
    X(test_senses_greet_only_a_new_face)                                                                               \
    X(test_tilt_holds_upside_down_on_noise)                                                                            \
    X(test_tilt_holds_lying_flat)                                                                                      \
    X(test_clock_keeps_running_for_days)                                                                               \
    X(test_bad_input_is_ignored)                                                                                       \
    X(test_pointer_stays_inside_its_range)                                                                             \
    X(test_long_step_counts_as_one_second)                                                                             \
    X(test_tap_makes_it_happy_for_a_moment)                                                                            \
    X(test_upside_down_makes_it_angry_until_righted)                                                                   \
    X(test_shake_is_told_from_tilt_and_knocks)                                                                         \
    X(test_shake_skips_bad_samples)                                                                                    \
    X(test_shaking_makes_it_dizzy_until_a_bit_after)                                                                   \
    X(test_shake_makes_it_dizzy_then_calm)                                                                             \
    X(test_blink_does_not_restart_while_running)                                                                       \
    X(test_follow_off_ignores_the_pointer)                                                                             \
    X(test_head_amount_zero_keeps_the_head_still)                                                                      \
    X(test_set_shape_is_used_by_the_renderer)                                                                          \
    X(test_look_text_round_trips)                                                                                      \
    X(test_malformed_look_text_is_rejected)                                                                            \
    X(test_rgb565_conversion)                                                                                          \
    X(test_render_first_frame_is_full_screen)                                                                          \
    X(test_render_puts_colors_where_they_belong)                                                                       \
    X(test_render_glint_is_white)                                                                                      \
    X(test_closed_eye_shows_only_the_stroke)                                                                           \
    X(test_render_is_deterministic)                                                                                    \
    X(test_dirty_region_redraws_everything_that_changed)                                                               \
    X(test_dirty_region_stays_inside_the_screen)                                                                       \
    X(test_band_rendering_matches_whole_frame)                                                                         \
    X(test_dirty_region_has_even_coordinates)                                                                          \
    X(test_extreme_poses_render_inside_the_frame)                                                                      \
    X(test_eyes_off_the_screen_render_nothing)                                                                         \
    X(test_small_output_is_the_frame_scaled_down)                                                                      \
    X(test_small_output_regions_stay_inside)

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
        if (!(fabsf(a_ - e_) <= (tolerance)))                                                                          \
            LP_FAIL("%s is %g, expected %g", #actual, (double)a_, (double)e_);                                         \
    } while (0)

#endif /* LILPET_TEST_H */
