/*
 * State and animation of the eyes: emotions, gaze, head, blink, particles.
 * A seed and the same time steps give the same animation: engine/tests/goldens/animation.png replays one, so drawing
 * the random numbers in another order changes it.
 */
#include "internal.h"
#include "lilpet/lilpet.h"

#include <string.h>

lp_shape lp_default_shape(void)
{
    return (lp_shape){.rx = 93.0f, .ry = 109.0f, .gap = 101.0f, .tilt = 0.37f};
}

static float rnd(lp_eyes *e)
{
    return lp_rng_next(&e->rng);
}

void lp_eyes_init(lp_eyes *eyes, uint32_t seed)
{
    memset(eyes, 0, sizeof *eyes);
    lp_rng_seed(&eyes->rng, seed);
    eyes->look = (lp_look){.sclera = 0x6FF5D0, .pupil = LP_PUPIL_ACORN, .pupil_color = {0x106E54, 0x106E54}};
    eyes->emotion = LP_EMOTION_NEUTRAL;
    eyes->cur = lp_emotion_get(LP_EMOTION_NEUTRAL)->expr;
    eyes->shape = lp_default_shape();
    eyes->head_amount = 0.6f;
    eyes->follow = true;
    eyes->next_blink = 1.5f;
}

void lp_eyes_set_look(lp_eyes *eyes, const lp_look *look)
{
    eyes->look = *look;
}

void lp_eyes_set_emotion(lp_eyes *eyes, lp_emotion emotion, float duration)
{
    if ((unsigned)emotion >= LP_EMOTION_COUNT || !isfinite(duration))
        return;
    eyes->emotion = emotion;
    eyes->emotion_timer = duration;
    if (duration <= 0.0f)
        eyes->mood = emotion;
}

/* A NaN would spread through the easing to the whole state, for good: inputs that are not finite are dropped */
void lp_eyes_set_pointer(lp_eyes *eyes, bool active, float x, float y)
{
    eyes->pointer.active = active && isfinite(x) && isfinite(y);
    eyes->pointer.x = lp_clampf(x, 1.0f);
    eyes->pointer.y = lp_clampf(y, 1.0f);
}

#define BURST_GAP 0.5f /* seconds: a burst of sparkles fills the screen if repeated any faster */

void lp_eyes_react(lp_eyes *eyes, lp_event event)
{
    const lp_reaction *r = lp_reaction_get(event);
    if (!r)
        return;
    if (eyes->emotion == r->emotion)
        eyes->emotion_timer = lp_maxf(eyes->emotion_timer, r->duration);
    else
        lp_eyes_set_emotion(eyes, r->emotion, r->duration);
    if (r->sparkles == 0 || eyes->burst_wait > 0.0f)
        return;
    eyes->burst_wait = BURST_GAP;
    for (int i = 0; i < r->sparkles; i++)
        lp_eyes_spawn(eyes, LP_PARTICLE_SPARKLE);
}

void lp_eyes_set_gravity(lp_eyes *eyes, bool active, float x, float y)
{
    eyes->gravity.active = active && isfinite(x) && isfinite(y);
    eyes->gravity.x = x;
    eyes->gravity.y = y;
}

void lp_eyes_set_shaking(lp_eyes *eyes, bool shaking)
{
    eyes->shaking = shaking;
}

void lp_eyes_set_follow(lp_eyes *eyes, bool follow)
{
    eyes->follow = follow;
}

void lp_eyes_set_shape(lp_eyes *eyes, const lp_shape *shape)
{
    if (isfinite(shape->rx) && isfinite(shape->ry) && isfinite(shape->gap) && isfinite(shape->tilt))
        eyes->shape = *shape;
}

void lp_eyes_set_head_amount(lp_eyes *eyes, float amount)
{
    if (isfinite(amount))
        eyes->head_amount = amount;
}

void lp_eyes_blink(lp_eyes *eyes)
{
    if (eyes->blink.phase != LP_BLINK_NONE)
        return;
    const bool sleepy = eyes->emotion == LP_EMOTION_SLEEPY;
    eyes->blink.phase = LP_BLINK_ANTICIP;
    eyes->blink.t = 0.0f;
    eyes->blink.hold = (0.08f + rnd(eyes) * 0.2f) * (sleepy ? 2.5f : 1.0f);
    eyes->blink.open = (0.17f + rnd(eyes) * 0.06f) * (sleepy ? 2.0f : 1.0f);
}

void lp_eyes_spawn(lp_eyes *eyes, lp_particle_type type)
{
    lp_particle p = {.type = type};
    if (type == LP_PARTICLE_HEART) {
        p.x = LP_CENTER + (rnd(eyes) - 0.5f) * 300.0f;
        p.y = (float)LP_SCREEN_SIZE - 40.0f + rnd(eyes) * 30.0f;
        p.vy = -(45.0f + rnd(eyes) * 35.0f);
        p.sway = rnd(eyes) * LP_PI * 2.0f;
        p.size = 12.0f + rnd(eyes) * 7.0f;
        p.life = 2.6f + rnd(eyes) * 1.4f;
    } else if (type == LP_PARTICLE_SPARKLE) {
        /* Sparkles pop around the eyes, never on top of them */
        const float a = rnd(eyes) * LP_PI * 2.0f;
        const float dist = 150.0f + rnd(eyes) * 60.0f;
        p.x = LP_CENTER + cosf(a) * dist;
        p.y = LP_CENTER + sinf(a) * dist * 0.8f;
        p.size = 6.0f + rnd(eyes) * 7.0f;
        p.spin = (rnd(eyes) - 0.5f) * 2.0f;
        p.life = 0.6f + rnd(eyes) * 0.5f;
    } else {
        return;
    }
    if (eyes->particle_count < LP_MAX_PARTICLES)
        eyes->particles[eyes->particle_count++] = p;
}

static void update_particles(lp_eyes *e, float dt)
{
    const lp_emotion_def *def = lp_emotion_get(e->emotion);
    if (def->emit != LP_PARTICLE_NONE) {
        e->emit_debt += def->emit_rate * dt;
        for (; e->emit_debt >= 1.0f - LP_EMIT_EPSILON; e->emit_debt -= 1.0f)
            lp_eyes_spawn(e, def->emit);
    } else {
        e->emit_debt = 0.0f;
    }
    /* Backward, removing expired particles while keeping the drawing order of the others */
    for (int i = e->particle_count - 1; i >= 0; i--) {
        lp_particle *p = &e->particles[i];
        p->age += dt;
        if (p->age > p->life) {
            memmove(p, p + 1, (size_t)(e->particle_count - 1 - i) * sizeof *p);
            e->particle_count--;
            continue;
        }
        if (p->type == LP_PARTICLE_HEART) {
            p->y += p->vy * dt;
            p->x += sinf(p->age * 3.0f + p->sway) * 18.0f * dt;
        }
    }
}

static void next_blink_phase(lp_eyes *e, lp_blink_phase phase)
{
    e->blink.phase = phase;
    e->blink.t = 0.0f;
}

static void update_blink(lp_eyes *e, float dt)
{
    if (e->blink.phase == LP_BLINK_NONE)
        return;
    e->blink.t += dt;
    switch (e->blink.phase) {
    case LP_BLINK_ANTICIP:
        if (e->blink.t > LP_BLINK_ANTICIP_TIME)
            next_blink_phase(e, LP_BLINK_CLOSE);
        break;
    case LP_BLINK_CLOSE:
        if (e->blink.t > LP_BLINK_CLOSE_TIME) {
            next_blink_phase(e, LP_BLINK_CLOSED);
            /* Eyes are hidden: jump to the new gaze without showing the move */
            if (e->pending_gaze.active) {
                e->gaze.tx = e->gaze.x = e->pending_gaze.x;
                e->gaze.ty = e->pending_gaze.y;
                e->gaze.y = e->pending_gaze.y + e->cur.gy;
                e->pending_gaze.active = false;
            }
        }
        break;
    case LP_BLINK_CLOSED:
        if (e->blink.t > e->blink.hold)
            next_blink_phase(e, LP_BLINK_OPEN);
        break;
    case LP_BLINK_OPEN:
        if (e->blink.t > e->blink.open) {
            e->blink.phase = LP_BLINK_NONE;
            if (e->blink.twice) {
                e->blink.twice = false;
                e->next_blink = e->time + 0.1f;
            }
        }
        break;
    case LP_BLINK_NONE:
        break;
    }
}

/* Dizzy: the gaze circles (radius, in gaze units) and the head wobbles (rad), in proportion to spin / max; the spin
 * wears off slower than it comes, so the eyes calm down after the emotion is over */
#define DIZZY_MAX      9.0f
#define DIZZY_RADIUS   0.55f
#define DIZZY_WOBBLE   0.12f
#define DIZZY_WEAR_OFF 1.5f

static void ease_expression(lp_expression *cur, const lp_expression *target, float dt)
{
    const float k = 9.0f;
    cur->tl = lp_approach(cur->tl, target->tl, k, dt);
    cur->ts = lp_approach(cur->ts, target->ts, k, dt);
    cur->bl = lp_approach(cur->bl, target->bl, k, dt);
    cur->sq = lp_approach(cur->sq, target->sq, k, dt);
    cur->cr = lp_approach(cur->cr, target->cr, k, dt);
    cur->rot = lp_approach(cur->rot, target->rot, k, dt);
    cur->hl = lp_approach(cur->hl, target->hl, k, dt);
    cur->ps = lp_approach(cur->ps, target->ps, k, dt);
    cur->es = lp_approach(cur->es, target->es, k, dt);
    cur->gy = lp_approach(cur->gy, target->gy, k, dt);
    cur->spin = lp_approach(cur->spin, target->spin, target->spin < cur->spin ? DIZZY_WEAR_OFF : k, dt);
}

static float dizziness(const lp_eyes *e)
{
    return lp_minf(e->cur.spin / DIZZY_MAX, 1.0f);
}

#define TILT_SLIDE 0.5f /* gaze shift toward the low side, at full tilt */

/* Upside down: gravity toward the top of the screen (in g, -1 fully upside down), held this many seconds */
#define UPSIDE_DOWN_GRAVITY (-0.6f)
#define UPSIDE_DOWN_DELAY   0.6f

/* Held upside down: angry as long as it lasts; shaken: dizzy, even upside down (one reaction at a time). Reacting every
 * frame, each goes on as long as its cause, and its duration after. The table gives them no sparkles, which would burst
 * every BURST_GAP as long as the cause lasts. */
static void update_held_reactions(lp_eyes *e, float dt)
{
    e->upside_down = e->gravity.active && e->gravity.y < UPSIDE_DOWN_GRAVITY ? e->upside_down + dt : 0.0f;
    if (e->shaking)
        lp_eyes_react(e, LP_EVENT_SHAKE);
    else if (e->upside_down >= UPSIDE_DOWN_DELAY)
        lp_eyes_react(e, LP_EVENT_UPSIDE_DOWN);
}

/* The angle equal to target modulo a turn that is nearest to from, so that easing toward it never spins around */
static float nearest_turn(float target, float from)
{
    return target + 2.0f * LP_PI * floorf((from - target) / (2.0f * LP_PI) + 0.5f);
}

static float wrap_angle(float a)
{
    return fabsf(a) > LP_PI ? a - copysignf(2.0f * LP_PI, a) : a;
}

/*
 * Tilting the object turns the face to stay level and slides the eyes toward the low side; returns the slide. The
 * level roll eases in as fast as it can be trusted: it holds lying flat, where the angle is mostly noise, so the face
 * never flips back and forth; without accelerometer it goes back to upright.
 */
static float update_tilt(lp_eyes *e, float dt)
{
    if (!e->gravity.active) {
        e->head.level = wrap_angle(lp_approach(e->head.level, 0.0f, 3.0f, dt));
        return 0.0f;
    }
    /* Trust: 0 lying flat, where little gravity is left in the screen plane, 1 standing up */
    const float w = lp_minf(lp_maxf((hypotf(e->gravity.x, e->gravity.y) - 0.3f) / 0.4f, 0.0f), 1.0f);
    const float a = atan2f(e->gravity.x, e->gravity.y); /* 0 upright, > 0 turned clockwise */
    e->head.level = wrap_angle(lp_approach(e->head.level, nearest_turn(-a, e->head.level), 10.0f * w, dt));
    return sinf(a) * w;
}

typedef struct {
    float x, y;
} gaze_aim;

/*
 * Gaze target: pointer (face tracking) or idle saccades; big jumps are often hidden behind a blink. Dizzy, the gaze
 * circles instead. Returns where the gaze aims, shifted sideways by the tilt slide.
 */
static gaze_aim update_gaze(lp_eyes *e, float dt, float t, float slide)
{
    const bool tracking = e->follow && e->pointer.active;
    if (tracking) {
        e->gaze.tx = e->pointer.x;
        e->gaze.ty = e->pointer.y;
    } else if (t > e->next_saccade) {
        const float r = rnd(e) < 0.3f ? 0.0f : 0.3f + rnd(e) * 0.6f; /* often return to center */
        const float a = rnd(e) * LP_PI * 2.0f;
        const float tx = cosf(a) * r;
        const float ty = sinf(a) * r * 0.7f;
        if (hypotf(tx - e->gaze.tx, ty - e->gaze.ty) > 0.5f && e->blink.phase == LP_BLINK_NONE && rnd(e) < 0.5f) {
            e->pending_gaze.active = true;
            e->pending_gaze.x = tx;
            e->pending_gaze.y = ty;
            lp_eyes_blink(e);
        } else {
            e->gaze.tx = tx;
            e->gaze.ty = ty;
        }
        e->next_saccade = t + 0.6f + rnd(e) * 2.4f;
    }
    float tx = e->gaze.tx;
    float ty = e->gaze.ty;
    const float dizzy = dizziness(e);
    if (dizzy > 0.0f) {
        e->spin_phase += e->cur.spin * dt;
        if (e->spin_phase > 2.0f * LP_PI)
            e->spin_phase -= 2.0f * LP_PI;
        tx += (cosf(e->spin_phase) * DIZZY_RADIUS - tx) * dizzy;
        ty += (sinf(e->spin_phase) * DIZZY_RADIUS * 0.7f - ty) * dizzy;
    }
    const float aim_x = lp_clampf(tx + slide * TILT_SLIDE, 1.0f);
    const float px = e->gaze.x;
    const float py = e->gaze.y;
    const float k = tracking ? 14.0f : 22.0f;
    e->gaze.x = lp_approach(e->gaze.x, aim_x, k, dt);
    e->gaze.y = lp_approach(e->gaze.y, ty + e->cur.gy, k, dt);
    e->gaze.vx = (e->gaze.x - px) / dt;
    e->gaze.vy = (e->gaze.y - py) / dt;
    return (gaze_aim){aim_x, ty};
}

/*
 * The clock goes back by a whole number of breaths (render.c: sin(time * 2.1)) about every 17 minutes, with every
 * deadline: a float clock that keeps growing loses its fractions, runs fast after 3 days and stops after 6 at 60
 * frames/s.
 */
#define CLOCK_WRAP (336.0f * 2.0f * LP_PI / 2.1f)

static void wrap_clock(lp_eyes *e)
{
    if (e->time < CLOCK_WRAP)
        return;
    e->time -= CLOCK_WRAP;
    e->next_saccade -= CLOCK_WRAP;
    e->next_blink -= CLOCK_WRAP;
    e->next_roll -= CLOCK_WRAP;
}

void lp_eyes_update(lp_eyes *eyes, float dt)
{
    if (!(dt > 0.0f) || !isfinite(dt)) /* nothing to advance, and speeds would divide by zero */
        return;
    dt = lp_minf(dt, 1.0f); /* a long pause is not a long animation: particles are spawned one by one */
    lp_eyes *e = eyes;
    e->time += dt;
    wrap_clock(e);
    const float t = e->time;

    ease_expression(&e->cur, &lp_emotion_get(e->emotion)->expr, dt);
    if (e->emotion_timer > 0.0f) {
        e->emotion_timer -= dt;
        if (e->emotion_timer <= 0.0f)
            lp_eyes_set_emotion(e, e->mood, 0.0f);
    }
    e->burst_wait = lp_maxf(e->burst_wait - dt, 0.0f);
    update_held_reactions(e, dt);

    const gaze_aim aim = update_gaze(e, dt, t, update_tilt(e, dt));

    /* Blinks at irregular intervals, sometimes double */
    if (t > e->next_blink && e->blink.phase == LP_BLINK_NONE) {
        lp_eyes_blink(e);
        e->blink.twice = rnd(e) < 0.2f;
        e->next_blink = t + (e->emotion == LP_EMOTION_SLEEPY ? 1.5f : 2.0f) + rnd(e) * 4.0f;
    }
    update_blink(e, dt);
    update_particles(e, dt);

    /* Head follows the gaze target, faster while the eyes are shut (hidden pose change) */
    const float hk = e->blink.phase != LP_BLINK_NONE ? 12.0f : 4.0f;
    e->head.yaw = lp_approach(e->head.yaw, aim.x * 0.55f * e->head_amount, hk, dt);
    e->head.pitch = lp_approach(e->head.pitch, (aim.y + e->cur.gy) * 0.4f * e->head_amount, hk, dt);
    if (t > e->next_roll) {
        e->head.roll_target = (rnd(e) - 0.5f) * 0.4f;
        e->next_roll = t + 2.0f + rnd(e) * 4.0f;
    }
    const float wobble = sinf(e->spin_phase) * DIZZY_WOBBLE * dizziness(e);
    const float roll = e->head.roll_target * e->head_amount - e->head.yaw * 0.4f + e->head.level + wobble;
    e->head.roll = wrap_angle(lp_approach(e->head.roll, nearest_turn(roll, e->head.roll), 3.0f, dt));
}

#define SCENE_MAX_TIME 60.0f /* seconds of particles to replay: a larger time would block for as long */

static bool scene_is_valid(const lp_scene *s)
{
    return (unsigned)s->emotion < LP_EMOTION_COUNT && (unsigned)s->blink_phase <= LP_BLINK_OPEN &&
           isfinite(s->blink_t) && isfinite(s->yaw) && isfinite(s->pitch) && isfinite(s->roll) && isfinite(s->gaze_x) &&
           isfinite(s->gaze_y) && s->particle_time >= 0.0f && s->particle_time <= SCENE_MAX_TIME;
}

void lp_eyes_set_scene(lp_eyes *eyes, const lp_scene *scene, const lp_look *look)
{
    if (!scene_is_valid(scene))
        return;
    const lp_look worn = *look; /* it may be the look of `eyes` */
    lp_eyes_init(eyes, scene->particle_seed);
    lp_eyes *e = eyes;
    e->look = worn;
    e->emotion = scene->emotion;
    e->mood = scene->emotion;
    e->cur = lp_emotion_get(scene->emotion)->expr;
    e->head.yaw = scene->yaw;
    e->head.pitch = scene->pitch;
    e->head.roll = scene->roll;
    e->gaze.x = e->gaze.tx = scene->gaze_x;
    e->gaze.ty = scene->gaze_y;
    e->gaze.y = scene->gaze_y + e->cur.gy;
    e->blink.phase = scene->blink_phase;
    e->blink.t = scene->blink_t;
    e->blink.hold = 0.2f;
    e->blink.open = 0.2f;
    if (scene->has_particles) {
        const int steps = (int)lroundf(scene->particle_time * 60.0f);
        for (int i = 0; i < steps; i++)
            update_particles(e, 1.0f / 60.0f);
    }
}
