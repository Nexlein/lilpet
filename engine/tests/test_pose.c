/* Blinks: when a new one starts, and the pose of each phase */
#include "../src/internal.h"
#include "lilpet/lilpet.h"
#include "test.h"

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

void test_blink_pose_stops_at_the_end_of_its_phase(void)
{
    /* A scene may freeze a phase past its length: the lid and the eye's scale stay at the end of the phase */
    const struct {
        lp_blink_phase phase;
        float t, lid, sx, sy;
        bool closed;
    } poses[] = {
        {LP_BLINK_ANTICIP, 0.5f * LP_BLINK_ANTICIP_TIME, 0.05f, 1.025f, 0.985f, false}, /* halfway */
        {LP_BLINK_ANTICIP, 1.0f, 0.1f, 1.05f, 0.97f, false},
        {LP_BLINK_CLOSE, 1.0f, 1.0f, 1.05f, 0.97f, false},
        {LP_BLINK_CLOSED, 1.0f, 1.0f, 1.04f, 1.0f, true},
        {LP_BLINK_OPEN, 1.0f, 0.0f, 1.0f, 1.0f, false},
        {LP_BLINK_NONE, 1.0f, 0.0f, 1.0f, 1.0f, false},
    };
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    for (size_t i = 0; i < sizeof poses / sizeof poses[0]; i++) {
        eyes.blink.phase = poses[i].phase;
        eyes.blink.t = poses[i].t;
        eyes.blink.open = 0.2f;
        const lp_blink_pose pose = lp_eyes_blink_pose(&eyes);
        CHECK_NEAR(pose.lid, poses[i].lid, 1e-6f);
        CHECK_NEAR(pose.sx, poses[i].sx, 1e-6f);
        CHECK_NEAR(pose.sy, poses[i].sy, 1e-6f);
        CHECK(pose.closed == poses[i].closed);
    }
}
