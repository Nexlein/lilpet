/*
 * When the look shown is written to flash: once it differs from the one kept and LOOK_SAVE_GAP_US have passed since the
 * last write or try. A sender streaming looks (an editor's slider) does not wear the flash out, the last look is the
 * one kept, and a write that fails is tried again a gap later. Pure, so the host tests check it (firmware/tests);
 * commands.c does the writing.
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

/* now and tried_at (the last write or try) in microseconds */
static inline bool look_save_due(const lp_look *shown, const lp_look *kept, int64_t now, int64_t tried_at)
{
    return !look_same(shown, kept) && now - tried_at >= LOOK_SAVE_GAP_US;
}

#endif /* LILPET_LOOK_SAVING_H */
