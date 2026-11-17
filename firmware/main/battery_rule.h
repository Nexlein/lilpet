/*
 * When the battery counts as low, and what the eyes do then (docs/objet-v1.md: they look tired). Below BATTERY_LOW_MV
 * it is low, and it stays low until it is back above BATTERY_OK_MV: a voltage that wavers around one threshold does not
 * flicker the eyes between two moods. Free of ESP-IDF, so the host tests check it (firmware/tests).
 */
#ifndef LILPET_BATTERY_RULE_H
#define LILPET_BATTERY_RULE_H

#include "lilpet/lilpet.h"

#include <stdbool.h>

#define BATTERY_LOW_MV 3500 /* a LiPo cell near its last tenth (proposed) */
#define BATTERY_OK_MV  3600

/* millivolts: -1 for no battery, which is never low */
static inline bool battery_low(int millivolts, bool was_low)
{
    return millivolts >= 0 && millivolts < (was_low ? BATTERY_OK_MV : BATTERY_LOW_MV);
}

/* The eyes as the battery goes low or comes back: tired while it is low, then the mood from before (kept in *before),
 * unless the PC app picked another meanwhile */
static inline void battery_show(lp_eyes *eyes, bool was_low, bool low, lp_emotion *before)
{
    if (low && !was_low) {
        *before = eyes->mood;
        lp_eyes_set_emotion(eyes, LP_EMOTION_TIRED, 0.0f);
    } else if (!low && was_low && eyes->mood == LP_EMOTION_TIRED) {
        lp_eyes_set_emotion(eyes, *before, 0.0f);
    }
}

#endif /* LILPET_BATTERY_RULE_H */
