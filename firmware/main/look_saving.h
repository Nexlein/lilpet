/*
 * When the look shown is written to flash: once it differs from the one kept and LOOK_SAVE_GAP_US have passed since the
 * last write or try. A sender streaming looks (an editor's slider) does not wear the flash out, the last look is the
 * one kept, and a write that fails is tried again a gap later. Free of ESP-IDF, so the host tests check it
 * (firmware/tests): commands.c passes it the write to flash.
 */
#ifndef LILPET_LOOK_SAVING_H
#define LILPET_LOOK_SAVING_H

#include "lilpet/lilpet.h"

#include <stdbool.h>
#include <stdint.h>

/* Microseconds: a new look is written to flash at most this often (read by scripts/check-device.py and its tests) */
#define LOOK_SAVE_GAP_US 2000000

static inline bool look_same(const lp_look *a, const lp_look *b)
{
    return a->sclera == b->sclera && a->pupil == b->pupil && a->pupil_color[0] == b->pupil_color[0] &&
           a->pupil_color[1] == b->pupil_color[1];
}

typedef struct {
    lp_look kept;     /* the last look written to flash */
    int64_t tried_at; /* the last write or try, in microseconds */
} look_saving;

/* kept: what the flash holds, or the default look that stands for nothing kept. The first new look is written at once
 */
static inline look_saving look_saving_start(const lp_look *kept)
{
    return (look_saving){.kept = *kept, .tried_at = -LOOK_SAVE_GAP_US};
}

/* Writes `shown` with `save` once its turn comes; false when `save` fails. now in microseconds */
static inline bool look_save_if_due(look_saving *s, const lp_look *shown, int64_t now, bool (*save)(const lp_look *))
{
    if (look_same(shown, &s->kept) || now - s->tried_at < LOOK_SAVE_GAP_US)
        return true;
    s->tried_at = now;
    if (!save(shown))
        return false;
    s->kept = *shown;
    return true;
}

#endif /* LILPET_LOOK_SAVING_H */
