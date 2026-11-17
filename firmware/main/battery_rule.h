/*
 * When the battery counts as low (docs/objet-v1.md: the eyes look tired then). Below BATTERY_LOW_MV it is low, and it
 * stays low until it is back above BATTERY_OK_MV: a voltage that wavers around one threshold does not flicker the eyes
 * between two moods. Free of ESP-IDF, so the host tests check it (firmware/tests).
 */
#ifndef LILPET_BATTERY_RULE_H
#define LILPET_BATTERY_RULE_H

#include <stdbool.h>

#define BATTERY_LOW_MV 3500 /* a LiPo cell near its last tenth (proposed) */
#define BATTERY_OK_MV  3600

static inline bool battery_low(int millivolts, bool was_low)
{
    return millivolts < (was_low ? BATTERY_OK_MV : BATTERY_LOW_MV);
}

#endif /* LILPET_BATTERY_RULE_H */
