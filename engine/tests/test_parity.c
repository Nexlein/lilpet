/* The C engine against values computed by the JS prototype (js_vectors.h, from scripts/gen-engine-vectors.mjs) */
#include "js_vectors.h"
#include "lilpet/lilpet.h"
#include "test.h"

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

/* JS runs in double, the engine in float: continuous values agree within these tolerances */
#define TOL_EXACT    1e-6f
#define TOL_STATE    2e-3f
#define TOL_POSITION 5e-2f /* sums of particle positions, in pixels */

void test_rng_matches_prototype(void)
{
    for (int s = 0; s < COUNT(LPV_RNG_SEEDS); s++) {
        lp_rng rng;
        lp_rng_seed(&rng, LPV_RNG_SEEDS[s]);
        for (int i = 0; i < COUNT(LPV_RNG_K[s]); i++) {
            const float v = lp_rng_next(&rng);
            CHECK(v == (float)LPV_RNG_K[s][i] / 16777216.0f);
        }
    }
}

void test_emotions_match_prototype(void)
{
    CHECK(COUNT(LPV_EMOTIONS) == LP_EMOTION_COUNT);
    for (int i = 0; i < LP_EMOTION_COUNT; i++) {
        const lp_emotion_def *got = lp_emotion_get((lp_emotion)i);
        const lp_emotion_def *want = &LPV_EMOTIONS[i];
        CHECK_STR_EQ(got->name, want->name);
        CHECK(lp_emotion_from_name(want->name) == (lp_emotion)i);
        CHECK(got->emit == want->emit);
        CHECK_NEAR(got->emit_rate, want->emit_rate, TOL_EXACT);
        /* lp_expression is 10 floats: compare them as an array */
        float g[10];
        float w[10];
        _Static_assert(sizeof g == sizeof(lp_expression), "lp_expression must be 10 floats");
        memcpy(g, &got->expr, sizeof g);
        memcpy(w, &want->expr, sizeof w);
        for (int k = 0; k < 10; k++)
            CHECK_NEAR(g[k], w[k], TOL_EXACT);
    }
}

void test_blink_poses_match_prototype(void)
{
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    for (int i = 0; i < COUNT(LPV_BLINKS); i++) {
        const lpv_blink *b = &LPV_BLINKS[i];
        eyes.blink.phase = b->phase;
        eyes.blink.t = b->t;
        eyes.blink.open = b->open;
        const lp_blink_pose p = lp_eyes_blink_pose(&eyes);
        CHECK_NEAR(p.lid, b->pose.lid, TOL_EXACT);
        CHECK_NEAR(p.sx, b->pose.sx, TOL_EXACT);
        CHECK_NEAR(p.sy, b->pose.sy, TOL_EXACT);
        CHECK(p.closed == b->pose.closed);
    }
}

void test_eye_poses_match_prototype(void)
{
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    for (int i = 0; i < COUNT(LPV_EYES); i++) {
        const lpv_eye *e = &LPV_EYES[i];
        eyes.head.yaw = e->yaw;
        eyes.head.pitch = e->pitch;
        eyes.head.roll = e->roll;
        const lp_eye_pose p = lp_eyes_eye_pose(&eyes, e->side);
        CHECK_NEAR(p.x, e->pose.x, TOL_STATE);
        CHECK_NEAR(p.y, e->pose.y, TOL_STATE);
        CHECK_NEAR(p.persp, e->pose.persp, TOL_EXACT);
        CHECK_NEAR(p.fx, e->pose.fx, TOL_EXACT);
        CHECK_NEAR(p.fy, e->pose.fy, TOL_EXACT);
    }
}

static float sum_x(const lp_eyes *e)
{
    float s = 0.0f;
    for (int i = 0; i < e->particle_count; i++)
        s += e->particles[i].x;
    return s;
}

static float sum_y(const lp_eyes *e)
{
    float s = 0.0f;
    for (int i = 0; i < e->particle_count; i++)
        s += e->particles[i].y;
    return s;
}

/* Compares the engine state with a JS snapshot; `what` names the case in failure messages */
static void check_state(const lp_eyes *e, const lpv_state *want, const char *what)
{
    const int before = lp_test_failures;
    CHECK_NEAR(e->gaze.x, want->gaze_x, TOL_STATE);
    CHECK_NEAR(e->gaze.y, want->gaze_y, TOL_STATE);
    CHECK_NEAR(e->gaze.tx, want->gaze_tx, TOL_STATE);
    CHECK_NEAR(e->gaze.ty, want->gaze_ty, TOL_STATE);
    CHECK_NEAR(e->head.yaw, want->yaw, TOL_STATE);
    CHECK_NEAR(e->head.pitch, want->pitch, TOL_STATE);
    CHECK_NEAR(e->head.roll, want->roll, TOL_STATE);
    CHECK_NEAR(e->cur.tl, want->tl, TOL_STATE);
    CHECK_NEAR(e->cur.bl, want->bl, TOL_STATE);
    CHECK_NEAR(e->cur.gy, want->gy, TOL_STATE);
    CHECK_NEAR(e->cur.ps, want->ps, TOL_STATE);
    CHECK(e->blink.phase == want->blink);
    CHECK(e->particle_count == want->particles);
    CHECK_NEAR(sum_x(e), want->particles_x, TOL_POSITION);
    CHECK_NEAR(sum_y(e), want->particles_y, TOL_POSITION);
    CHECK(e->emotion == want->emotion);
    if (lp_test_failures != before)
        fprintf(stderr, "  in %s\n", what);
}

void test_simulations_match_prototype(void)
{
    for (int s = 0; s < COUNT(LPV_SCENARIOS); s++) {
        const lpv_scenario *sc = &LPV_SCENARIOS[s];
        lp_eyes eyes;
        lp_eyes_init(&eyes, sc->seed);
        if (sc->emotion != LP_EMOTION_NEUTRAL)
            lp_eyes_set_emotion(&eyes, sc->emotion, sc->emotion_duration);
        for (int i = 0; i < sc->sparkles; i++)
            lp_eyes_spawn(&eyes, LP_PARTICLE_SPARKLE);
        lp_eyes_set_pointer(&eyes, sc->pointer, sc->pointer_x, sc->pointer_y);

        int next = sc->first_state;
        for (int step = 1; step <= sc->steps; step++) {
            lp_eyes_update(&eyes, LPV_DT);
            if (step % sc->every == 0) {
                char what[64];
                snprintf(what, sizeof what, "scenario %s, step %d", sc->name, step);
                CHECK_NEAR(eyes.time, LPV_STATES[next].time, TOL_EXACT);
                check_state(&eyes, &LPV_STATES[next], what);
                next++;
            }
        }
        CHECK(next == sc->first_state + sc->state_count);
    }
}

void test_scenes_match_prototype(void)
{
    for (int i = 0; i < COUNT(LPV_SCENES); i++) {
        lp_eyes eyes;
        lp_eyes_init(&eyes, 1);
        lp_eyes_set_scene(&eyes, &LPV_SCENES[i].scene, &LPV_LOOKS[LPV_SCENES[i].look]);
        check_state(&eyes, &LPV_SCENES[i].expected, LPV_SCENES[i].id);
    }
}
