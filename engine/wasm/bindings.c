#include "bindings.h"

#include "lilpet/lilpet.h"

#include <emscripten/emscripten.h>

#define PIXELS (LP_SCREEN_SIZE * LP_SCREEN_SIZE)

static lp_eyes eyes;
static lp_renderer renderer;
static uint16_t frame[PIXELS];
static uint8_t rgba[PIXELS * 4];

EMSCRIPTEN_KEEPALIVE int lpw_screen_size(void)
{
    return LP_SCREEN_SIZE;
}

EMSCRIPTEN_KEEPALIVE int lpw_emotion_count(void)
{
    return LP_EMOTION_COUNT;
}

EMSCRIPTEN_KEEPALIVE const char *lpw_emotion_name(int emotion)
{
    const lp_emotion_def *def = lp_emotion_get((lp_emotion)emotion);
    return def ? def->name : "";
}

EMSCRIPTEN_KEEPALIVE void lpw_init(uint32_t seed)
{
    lp_eyes_init(&eyes, seed);
    lp_renderer_init(&renderer);
}

EMSCRIPTEN_KEEPALIVE void lpw_set_look(uint32_t sclera, int pupil, uint32_t left, uint32_t right, int highlight)
{
    const lp_look look = {
        .sclera = sclera, .pupil = (lp_pupil)pupil, .pupil_color = {left, right}, .highlight = highlight != 0};
    lp_eyes_set_look(&eyes, &look);
}

EMSCRIPTEN_KEEPALIVE void lpw_set_emotion(int emotion, float duration)
{
    lp_eyes_set_emotion(&eyes, (lp_emotion)emotion, duration);
}

EMSCRIPTEN_KEEPALIVE void lpw_blink(void)
{
    lp_eyes_blink(&eyes);
}

EMSCRIPTEN_KEEPALIVE void lpw_set_pointer(int active, float x, float y)
{
    lp_eyes_set_pointer(&eyes, active != 0, x, y);
}

EMSCRIPTEN_KEEPALIVE void lpw_update(float dt)
{
    lp_eyes_update(&eyes, dt);
}

EMSCRIPTEN_KEEPALIVE void lpw_set_scene(int emotion, int blink_phase, float blink_t, float yaw, float pitch, float roll,
                                        float gaze_x, float gaze_y, int has_particles, uint32_t particle_seed,
                                        float particle_time)
{
    const lp_scene scene = {
        .emotion = (lp_emotion)emotion,
        .blink_phase = (lp_blink_phase)blink_phase,
        .blink_t = blink_t,
        .yaw = yaw,
        .pitch = pitch,
        .roll = roll,
        .gaze_x = gaze_x,
        .gaze_y = gaze_y,
        .has_particles = has_particles != 0,
        .particle_seed = particle_seed,
        .particle_time = particle_time,
    };
    const lp_look look = eyes.look;
    lp_eyes_set_scene(&eyes, &scene, &look);
}

EMSCRIPTEN_KEEPALIVE const uint8_t *lpw_render(void)
{
    lp_render(&renderer, &eyes, frame);
    for (int i = 0; i < PIXELS; i++) {
        const uint32_t p = frame[i];
        rgba[4 * i] = (uint8_t)(((p >> 11u) & 0x1Fu) * 255u / 31u);
        rgba[4 * i + 1] = (uint8_t)(((p >> 5u) & 0x3Fu) * 255u / 63u);
        rgba[4 * i + 2] = (uint8_t)((p & 0x1Fu) * 255u / 31u);
        rgba[4 * i + 3] = 255u;
    }
    return rgba;
}
