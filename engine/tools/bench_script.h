/*
 * Animation played by the renderer benchmarks, on the host (tools/bench.c) and on the chip (firmware/main/main.c), so
 * their timings compare: one second per emotion, with idle saccades, blinks and particles, then love held long enough
 * for its hearts to pile up, as they live up to 4 s, then the gaze follows a face moving in a circle.
 */
#ifndef LILPET_BENCH_SCRIPT_H
#define LILPET_BENCH_SCRIPT_H

#include "lilpet/lilpet.h"

#include <math.h>
#include <stdbool.h>

/* The look the benchmark's eyes wear on both, as its pupil type changes what is drawn: cherry with a dot pupil. Its
 * own, not lp_default_look(): a new default look must not change what the measurements compare. */
static const lp_look LP_BENCH_LOOK = {0xAE0002u, LP_PUPIL_DOT, {0xFF5252u, 0xFF5252u}};

/* Each pass starts from the same eyes: seed 1 and the benchmark's look */
static inline void lp_bench_start(lp_eyes *eyes)
{
    lp_eyes_init(eyes, 1);
    lp_eyes_set_look(eyes, &LP_BENCH_LOOK);
}

enum {
    LP_BENCH_FPS = 60,
    LP_BENCH_LOVE = LP_EMOTION_COUNT * LP_BENCH_FPS,  /* first frame of love held */
    LP_BENCH_FACE = LP_BENCH_LOVE + 5 * LP_BENCH_FPS, /* first frame of the face */
    LP_BENCH_FRAMES = LP_BENCH_FACE + 3 * LP_BENCH_FPS,
};

/* Call before lp_eyes_update(eyes, 1.0f / LP_BENCH_FPS) for frame f, from 0 to LP_BENCH_FRAMES - 1 */
static inline void lp_bench_script(lp_eyes *eyes, int f)
{
    if (f == 0) /* played in a loop on the chip: the face of the previous round leaves */
        lp_eyes_set_pointer(eyes, false, 0.0f, 0.0f);
    if (f % LP_BENCH_FPS == 0 && f < LP_BENCH_LOVE)
        lp_eyes_set_emotion(eyes, (lp_emotion)(f / LP_BENCH_FPS), 0.0f);
    if (f == LP_BENCH_LOVE)
        lp_eyes_set_emotion(eyes, LP_EMOTION_LOVE, 0.0f);
    if (f >= LP_BENCH_FACE) {
        const float a = (float)f * 0.05f;
        lp_eyes_set_pointer(eyes, true, 0.8f * cosf(a), 0.6f * sinf(a));
    }
}

#endif /* LILPET_BENCH_SCRIPT_H */
