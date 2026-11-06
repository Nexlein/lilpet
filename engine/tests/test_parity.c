/* The C engine against values computed by the JS prototype (js_vectors.h, from scripts/gen-engine-vectors.mjs) */
#include "../src/internal.h"
#include "js_vectors.h"
#include "lilpet/lilpet.h"
#include "test.h"

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

/* JS runs in double, the engine in float: continuous values agree within these tolerances */
#define TOL_EXACT    1e-6f
#define TOL_STATE    2e-3f
#define TOL_POSITION 5e-2f /* sums of particle positions, in pixels */
#define TOL_SPEED    5e-2f /* gaze speed (squash and stretch): a position step divided by dt */

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

/* The tables come from eyes/emotions.json (emotions_table.h): what is left to check is the enums, written by hand */
void test_emotions_match_prototype(void)
{
    CHECK(COUNT(LPV_EMOTION_NAMES) == LP_EMOTION_COUNT);
    for (int i = 0; i < COUNT(LPV_EMOTION_NAMES); i++) {
        CHECK_STR_EQ(lp_emotion_get((lp_emotion)i)->name, LPV_EMOTION_NAMES[i]);
        CHECK(lp_emotion_from_name(LPV_EMOTION_NAMES[i]) == (lp_emotion)i);
    }
    CHECK(COUNT(LPV_EVENT_NAMES) == LP_EVENT_COUNT);
    for (int i = 0; i < COUNT(LPV_EVENT_NAMES); i++) {
        CHECK_STR_EQ(lp_reaction_get((lp_event)i)->name, LPV_EVENT_NAMES[i]);
        CHECK(lp_event_from_name(LPV_EVENT_NAMES[i]) == (lp_event)i);
    }
}

void test_clamp_matches_prototype(void)
{
    for (int i = 0; i < COUNT(LPV_CLAMPS); i++)
        CHECK(lp_clampf(LPV_CLAMPS[i].v, LPV_CLAMPS[i].lim) == LPV_CLAMPS[i].clamped);
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

static lp_particle sum_particles(const lp_eyes *e)
{
    lp_particle s = {0};
    for (int i = 0; i < e->particle_count; i++) {
        const lp_particle *p = &e->particles[i];
        s.x += p->x;
        s.y += p->y;
        s.size += p->size;
        s.spin += p->spin;
        s.age += p->age;
        s.life += p->life;
    }
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
    CHECK_NEAR(e->gaze.vx, want->gaze_vx, TOL_SPEED);
    CHECK_NEAR(e->gaze.vy, want->gaze_vy, TOL_SPEED);
    CHECK_NEAR(e->head.yaw, want->yaw, TOL_STATE);
    CHECK_NEAR(e->head.pitch, want->pitch, TOL_STATE);
    CHECK_NEAR(e->head.roll, want->roll, TOL_STATE);
    CHECK_NEAR(e->cur.tl, want->cur.tl, TOL_STATE);
    CHECK_NEAR(e->cur.ts, want->cur.ts, TOL_STATE);
    CHECK_NEAR(e->cur.bl, want->cur.bl, TOL_STATE);
    CHECK_NEAR(e->cur.sq, want->cur.sq, TOL_STATE);
    CHECK_NEAR(e->cur.cr, want->cur.cr, TOL_STATE);
    CHECK_NEAR(e->cur.rot, want->cur.rot, TOL_STATE);
    CHECK_NEAR(e->cur.hl, want->cur.hl, TOL_STATE);
    CHECK_NEAR(e->cur.ps, want->cur.ps, TOL_STATE);
    CHECK_NEAR(e->cur.es, want->cur.es, TOL_STATE);
    CHECK_NEAR(e->cur.gy, want->cur.gy, TOL_STATE);
    CHECK_NEAR(e->cur.spin, want->cur.spin, TOL_STATE);
    CHECK(e->blink.phase == want->blink);
    CHECK_NEAR(e->blink.hold, want->blink_hold, TOL_STATE);
    CHECK_NEAR(e->blink.open, want->blink_open, TOL_STATE);
    CHECK(e->particle_count == want->particles);
    const lp_particle sums = sum_particles(e);
    CHECK_NEAR(sums.x, want->particles_x, TOL_POSITION);
    CHECK_NEAR(sums.y, want->particles_y, TOL_POSITION);
    CHECK_NEAR(sums.size, want->particles_size, TOL_STATE);
    CHECK_NEAR(sums.spin, want->particles_spin, TOL_STATE);
    CHECK_NEAR(sums.age, want->particles_age, TOL_STATE);
    CHECK_NEAR(sums.life, want->particles_life, TOL_STATE);
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
            lp_eyes_set_emotion(&eyes, sc->emotion, 0.0f);
        if (sc->react >= 0)
            lp_eyes_react(&eyes, (lp_event)sc->react);
        for (int i = 0; i < sc->sparkles; i++)
            lp_eyes_spawn(&eyes, LP_PARTICLE_SPARKLE);
        lp_eyes_set_pointer(&eyes, sc->pointer, sc->pointer_x, sc->pointer_y);
        lp_eyes_set_gravity(&eyes, sc->gravity, sc->gravity_x, sc->gravity_y);
        lp_eyes_set_shaking(&eyes, sc->shaking);

        int next = sc->first_state;
        for (int step = 1; step <= sc->steps; step++) {
            if (sc->spin != 0.0f) {
                const float a = sc->spin * (eyes.time + LPV_DT);
                lp_eyes_set_gravity(&eyes, true, sinf(a), cosf(a));
            }
            if (sc->react_every && step % sc->react_every == 0)
                lp_eyes_react(&eyes, (lp_event)sc->react);
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
        for (int step = 0; step < LPV_SCENE_PLAY; step++)
            lp_eyes_update(&eyes, LPV_DT);
        check_state(&eyes, &LPV_SCENES[i].played, LPV_SCENES[i].id);
    }
}
