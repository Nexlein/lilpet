/*
 * When a look just shown is written to flash: at once, then at most every SAVE_GAP_US, so that a sender streaming
 * looks (an editor's slider) does not wear the flash out, and the last look is the one kept. Pure, so the host tests
 * check it (firmware/tests); commands.c does the writing.
 */
#ifndef LILPET_LOOK_SAVING_H
#define LILPET_LOOK_SAVING_H

#include "lilpet/lilpet.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    LOOK_KEPT,       /* the flash holds it already */
    LOOK_SAVE_NOW,   /* the last write is old enough */
    LOOK_SAVE_LATER, /* written once `gap` has passed since the last write, unless another look comes first */
} look_save;

static inline bool look_same(const lp_look *a, const lp_look *b)
{
    return a->sclera == b->sclera && a->pupil == b->pupil && a->pupil_color[0] == b->pupil_color[0] &&
           a->pupil_color[1] == b->pupil_color[1];
}

/* now, saved_at (the last write) and gap in microseconds */
static inline look_save look_save_plan(const lp_look *look, const lp_look *kept, int64_t now, int64_t saved_at,
                                       int64_t gap)
{
    if (look_same(look, kept))
        return LOOK_KEPT;
    return now - saved_at >= gap ? LOOK_SAVE_NOW : LOOK_SAVE_LATER;
}

/* Whether a look left for later is due now */
static inline bool look_save_due(bool pending, int64_t now, int64_t saved_at, int64_t gap)
{
    return pending && now - saved_at >= gap;
}

#endif /* LILPET_LOOK_SAVING_H */
