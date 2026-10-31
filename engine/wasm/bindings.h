/*
 * JavaScript bindings of the engine for the WebAssembly build (used through engine/wasm/engine.mjs).
 * The module holds one eyes state, one renderer and one frame; plain scalar arguments keep the JS side simple.
 */
#ifndef LILPET_WASM_BINDINGS_H
#define LILPET_WASM_BINDINGS_H

#include <stdint.h>

int lpw_screen_size(void);
int lpw_emotion_count(void);
const char *lpw_emotion_name(int emotion);

void lpw_init(uint32_t seed);
void lpw_set_look(uint32_t sclera, int pupil, uint32_t left, uint32_t right);
void lpw_set_emotion(int emotion, float duration);
void lpw_blink(void);
void lpw_set_pointer(int active, float x, float y);
void lpw_set_follow(int follow);
void lpw_set_shape(float rx, float ry, float gap, float tilt);
void lpw_set_head_amount(float amount);
/* type: 1 heart, 2 sparkle (lp_particle_type) */
void lpw_spawn(int type);
/* Current emotion, which changes on its own when a timed one ends */
int lpw_emotion(void);
void lpw_update(float dt);
void lpw_set_scene(int emotion, int blink_phase, float blink_t, float yaw, float pitch, float roll, float gaze_x,
                   float gaze_y, int has_particles, uint32_t particle_seed, float particle_time);
/* Renders and returns the frame as RGBA bytes (LP_SCREEN_SIZE x LP_SCREEN_SIZE x 4); only the dirty region is converted
 */
const uint8_t *lpw_render(void);

#endif /* LILPET_WASM_BINDINGS_H */
