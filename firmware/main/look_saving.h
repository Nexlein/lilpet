/*
 * When the look shown is written to flash: once a look came by command, it differs from the one kept and
 * LOOK_SAVE_GAP_US have passed since the last write or try. The look a start shows is never written by itself: after a
 * crash it is the default one, and the flash keeps its own. A sender streaming looks (an editor's slider) does not wear
 * the flash out, the last look is the one kept, and a write that fails is tried again a gap later. Free of ESP-IDF, so
 * the host tests check it (firmware/tests): commands.c passes it the write to flash.
 */
#ifndef LILPET_LOOK_SAVING_H
#define LILPET_LOOK_SAVING_H

#include "lilpet/lilpet.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Microseconds: a new look is written to flash at most this often (read by scripts/check-device.py and its tests) */
#define LOOK_SAVE_GAP_US 2000000

/* Whether the flash would get the same text: a field added to the look counts here without a word */
static inline bool look_same(const lp_look *a, const lp_look *b)
{
    char text_a[LP_LOOK_TEXT_SIZE];
    char text_b[LP_LOOK_TEXT_SIZE];
    return lp_look_format(a, text_a) >= 0 && lp_look_format(b, text_b) >= 0 && strcmp(text_a, text_b) == 0;
}

typedef struct {
    lp_look kept;     /* the last look written to flash, when `known` */
    bool known;       /* false when the start did not read the flash: any look that comes is then written */
    bool sent;        /* a look came by command since the start */
    int64_t tried_at; /* the last write or try, in microseconds */
} look_saving;

/* kept: what the flash holds, the default look that stands for nothing kept, or NULL when the start did not read the
 * flash (after a crash, main.c). The first new look is written at once */
static inline look_saving look_saving_start(const lp_look *kept)
{
    return (look_saving){.kept = kept ? *kept : (lp_look){0}, .known = kept != NULL, .tried_at = -LOOK_SAVE_GAP_US};
}

/* A look came by command: from now on, the look shown is the one to keep */
static inline void look_saving_mark_sent(look_saving *s)
{
    s->sent = true;
}

/* Writes `shown` with `save` once its turn comes; false when `save` fails. now in microseconds */
static inline bool look_save_if_due(look_saving *s, const lp_look *shown, int64_t now, bool (*save)(const lp_look *))
{
    if (!s->sent || now - s->tried_at < LOOK_SAVE_GAP_US || (s->known && look_same(shown, &s->kept)))
        return true;
    s->tried_at = now;
    if (!save(shown))
        return false;
    s->kept = *shown;
    s->known = true;
    return true;
}

#endif /* LILPET_LOOK_SAVING_H */
