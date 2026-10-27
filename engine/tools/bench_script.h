/*
 * Animation played by the renderer benchmarks, on the host (tools/bench.c) and on the chip (firmware/main/main.c), so
 * their timings compare: one second per emotion, with idle saccades, blinks and particles, then the gaze follows a
 * face moving in a circle.
 */
#ifndef LILPET_BENCH_SCRIPT_H
#define LILPET_BENCH_SCRIPT_H

#include "lilpet/lilpet.h"

#include <math.h>
#include <stdbool.h>

enum { LP_BENCH_FPS = 60, LP_BENCH_FRAMES = (LP_EMOTION_COUNT + 3) * LP_BENCH_FPS };

/* Call before lp_eyes_update(eyes, 1.0f / LP_BENCH_FPS) for frame f, from 0 to LP_BENCH_FRAMES - 1 */
static inline void lp_bench_script(lp_eyes *eyes, int f)
{
    if (f % LP_BENCH_FPS == 0 && f / LP_BENCH_FPS < LP_EMOTION_COUNT)
        lp_eyes_set_emotion(eyes, (lp_emotion)(f / LP_BENCH_FPS), 0.0f);
    if (f >= LP_EMOTION_COUNT * LP_BENCH_FPS) {
        const float a = (float)f * 0.05f;
        lp_eyes_set_pointer(eyes, true, 0.8f * cosf(a), 0.6f * sinf(a));
    }
}

#endif /* LILPET_BENCH_SCRIPT_H */
