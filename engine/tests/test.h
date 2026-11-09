/*
 * Minimal test runner for the engine, run on the host (no dependency).
 * Add a test: write `void test_x(void)` in a C file of this folder, then list it in LP_TESTS below (a new file goes in
 * engine/CMakeLists.txt too).
 */
#ifndef LILPET_TEST_H
#define LILPET_TEST_H

#include "lilpet/lilpet.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* Every test function, one per line */
#define LP_TESTS(X)                                                                                                    \
    X(test_check_near_fails_on_nan)                                                                                    \
    X(test_version_matches_macros)                                                                                     \
    X(test_eyes_start_with_the_default_look_and_shape)                                                                 \
    X(test_unknown_emotion_is_rejected)                                                                                \
    X(test_particle_pool_drops_extra_particles)                                                                        \
    X(test_particles_keep_the_random_sequence_when_dropped)                                                            \
    X(test_timed_emotion_returns_to_neutral)                                                                           \
    X(test_senses_smooth_gravity)                                                                                      \
    X(test_senses_start_over_when_a_sensor_comes_back)                                                                 \
    X(test_senses_forget_a_sensor_that_stopped)                                                                        \
    X(test_senses_stop_a_shake_when_the_sensor_stops)                                                                  \
    X(test_senses_skip_a_bad_sample)                                                                                   \
    X(test_senses_skip_a_bad_step)                                                                                     \
    X(test_senses_debounce_the_button)                                                                                 \
    X(test_senses_react_to_a_press_not_a_release)                                                                      \
    X(test_senses_let_go_of_a_face)                                                                                    \
    X(test_senses_leave_alone_a_pointer_they_did_not_set)                                                              \
    X(test_senses_greet_only_a_new_face)                                                                               \
    X(test_tilt_holds_upside_down_on_noise)                                                                            \
    X(test_tilt_holds_lying_flat)                                                                                      \
    X(test_lying_flat_does_not_slide_the_eyes)                                                                         \
    X(test_clock_keeps_running_for_days)                                                                               \
    X(test_bad_input_is_ignored)                                                                                       \
    X(test_scene_starts_over)                                                                                          \
    X(test_pointer_stays_inside_its_range)                                                                             \
    X(test_long_step_counts_as_one_second)                                                                             \
    X(test_tap_makes_it_happy_for_a_moment)                                                                            \
    X(test_repeated_reaction_goes_on_without_a_new_burst)                                                              \
    X(test_another_emotion_reacts_at_once_without_a_new_burst)                                                         \
    X(test_reaction_without_sparkles_leaves_the_wait_alone)                                                            \
    X(test_chatty_events_never_fill_the_screen)                                                                        \
    X(test_scene_forgets_the_last_burst)                                                                               \
    X(test_upside_down_makes_it_angry_until_righted)                                                                   \
    X(test_holding_keeps_a_longer_emotion)                                                                             \
    X(test_shake_is_told_from_tilt_and_knocks)                                                                         \
    X(test_shake_limits)                                                                                               \
    X(test_shake_skips_bad_samples)                                                                                    \
    X(test_shaking_makes_it_dizzy_until_a_bit_after)                                                                   \
    X(test_shake_makes_it_dizzy_then_calm)                                                                             \
    X(test_blink_does_not_restart_while_running)                                                                       \
    X(test_follow_off_ignores_the_pointer)                                                                             \
    X(test_head_amount_zero_keeps_the_head_still)                                                                      \
    X(test_shape_and_head_amount_stay_in_their_ranges)                                                                 \
    X(test_set_shape_is_used_by_the_renderer)                                                                          \
    X(test_emotion_without_a_positive_duration_is_the_mood)                                                            \
    X(test_duration_that_is_not_finite_leaves_the_mood)                                                                \
    X(test_bad_point_clears_a_pointer_and_a_gravity)                                                                   \
    X(test_mood_that_is_the_reaction_stays_after_it)                                                                   \
    X(test_reaction_after_a_scene_returns_to_its_emotion)                                                              \
    X(test_rng_gives_the_mulberry32_sequence)                                                                          \
    X(test_clamp_with_a_negative_limit_gives_zero)                                                                     \
    X(test_every_emotion_and_event_is_found_by_its_name)                                                               \
    X(test_turning_keeps_the_face_level_across_the_wrap)                                                               \
    X(test_gaze_reaches_a_held_pointer)                                                                                \
    X(test_tilt_slides_the_eyes_toward_the_low_side)                                                                   \
    X(test_every_emotion_settles_on_its_expression)                                                                    \
    X(test_blink_pose_stops_at_the_end_of_its_phase)                                                                   \
    X(test_love_sends_hearts_at_its_rate_until_they_fade)                                                              \
    X(test_particles_owed_by_one_emitter_are_not_owed_by_another)                                                      \
    X(test_particles_owed_at_one_rate_are_not_owed_at_another)                                                         \
    X(test_look_text_round_trips)                                                                                      \
    X(test_every_pupil_type_has_a_name)                                                                                \
    X(test_malformed_look_text_is_rejected)                                                                            \
    X(test_command_line_ends_on_lf_or_cr)                                                                              \
    X(test_command_line_limit)                                                                                         \
    X(test_command_line_drops_what_does_not_fit)                                                                       \
    X(test_command_reads_every_command)                                                                                \
    X(test_command_refuses_bad_input)                                                                                  \
    X(test_command_ignores_other_lines)                                                                                \
    X(test_rgb565_conversion)                                                                                          \
    X(test_render_first_frame_is_full_screen)                                                                          \
    X(test_render_puts_colors_where_they_belong)                                                                       \
    X(test_pixels_differing_in_one_channel_stay_apart)                                                                 \
    X(test_render_glint_is_white)                                                                                      \
    X(test_closed_eye_shows_only_the_stroke)                                                                           \
    X(test_box_hugs_the_shape)                                                                                         \
    X(test_lid_level_with_the_rows)                                                                                    \
    X(test_render_is_deterministic)                                                                                    \
    X(test_dirty_region_redraws_everything_that_changed)                                                               \
    X(test_dirty_region_stays_inside_the_screen)                                                                       \
    X(test_band_rendering_matches_whole_frame)                                                                         \
    X(test_reference_scenes_with_particles_show_some)                                                                  \
    X(test_big_endian_swaps_the_bytes_of_every_pixel)                                                                  \
    X(test_dirty_region_has_even_coordinates)                                                                          \
    X(test_extreme_poses_render_inside_the_frame)                                                                      \
    X(test_unknown_pupil_renders_as_a_circle)                                                                          \
    X(test_cutting_spans)                                                                                              \
    X(test_polygon_edges_reach_every_sub_row)                                                                          \
    X(test_line_too_far_misses_an_ellipse)                                                                             \
    X(test_extreme_shapes_stay_in_the_buffers)                                                                         \
    X(test_small_output_is_the_frame_scaled_down)                                                                      \
    X(test_new_size_redraws_everything)                                                                                \
    X(test_small_output_regions_stay_inside)                                                                           \
    X(test_pupil_bigger_than_its_eye_stays_centered)                                                                   \
    X(test_lcd_id_tells_the_two_controllers_apart)                                                                     \
    X(test_imu_axes_give_gravity_on_the_screen)                                                                        \
    X(test_a_look_is_written_at_once_then_at_most_every_gap)                                                           \
    X(test_frame_splits_bands_and_measures_the_redrawn_share)

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

#define LP_TEST_STEP    (1.0f / 64.0f) /* a time step of the animation, exact in float */
#define LP_TEST_SETTLED 1e-4f /* what an easing given seconds to settle leaves of its distance, with float rounding */

/* Plays `seconds` of animation, LP_TEST_STEP at a time */
static inline void lp_test_play(lp_eyes *eyes, float seconds)
{
    for (int i = 0; i < (int)(seconds / LP_TEST_STEP); i++)
        lp_eyes_update(eyes, LP_TEST_STEP);
}

#endif /* LILPET_TEST_H */
