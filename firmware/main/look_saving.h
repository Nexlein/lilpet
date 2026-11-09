/*
 * When a look just shown is written to flash: at once, then at most every LOOK_SAVE_GAP_US, so that a sender streaming
 * looks (an editor's slider) does not wear the flash out, and the last look is the one kept. Pure, so the host tests
 * check it (firmware/tests); commands.c does the writing.
 */
#ifndef LILPET_LOOK_SAVING_H
#define LILPET_LOOK_SAVING_H

#include "lilpet/lilpet.h"

#include <stdbool.h>
#include <stdint.h>

/* Microseconds: a new look is written to flash at most this often (read by scripts/check-device.py and its tests) */
#define LOOK_SAVE_GAP_US 2000000

typedef enum {
    LOOK_KEPT,       /* the flash holds it already */
    LOOK_SAVE_NOW,   /* the last write is old enough */
    LOOK_SAVE_LATER, /* written once the gap has passed since the last write, unless another look comes first */
} look_save;

static inline bool look_same(const lp_look *a, const lp_look *b)
{
    return a->sclera == b->sclera && a->pupil == b->pupil && a->pupil_color[0] == b->pupil_color[0] &&
           a->pupil_color[1] == b->pupil_color[1];
}

/* now and saved_at (the last write) in microseconds */
static inline look_save look_save_plan(const lp_look *look, const lp_look *kept, int64_t now, int64_t saved_at)
{
    if (look_same(look, kept))
        return LOOK_KEPT;
    return now - saved_at >= LOOK_SAVE_GAP_US ? LOOK_SAVE_NOW : LOOK_SAVE_LATER;
}

/* Whether a look left for later is due now */
static inline bool look_save_due(bool pending, int64_t now, int64_t saved_at)
{
    return pending && now - saved_at >= LOOK_SAVE_GAP_US;
}

#endif /* LILPET_LOOK_SAVING_H */
