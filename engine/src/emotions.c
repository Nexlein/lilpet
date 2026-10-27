#include "lilpet/lilpet.h"

#include <string.h>

/* Same table as EMOTIONS in prototype/eyes.js (tests/test_parity.c checks it against generated values) */
/* clang-format off: one emotion per line keeps the table readable */
static const lp_emotion_def EMOTIONS[LP_EMOTION_COUNT] = {
    /* name, expression {tl, ts, bl, sq, cr, rot, hl, ps, es, gy} (see lp_expression), particles, rate per second */
    [LP_EMOTION_NEUTRAL] = {"neutral",
                            {0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 1.00f, 1.00f, 0.00f},
                            LP_PARTICLE_NONE,
                            0.0f},
    [LP_EMOTION_HAPPY] = {"happy",
                          {0.00f, 0.00f, 0.42f, 0.00f, 0.00f, 0.00f, 0.00f, 1.08f, 1.02f, 0.00f},
                          LP_PARTICLE_SPARKLE,
                          1.2f},
    [LP_EMOTION_LOVE] = {"love",
                         {0.00f, 0.00f, 0.45f, 0.00f, 0.00f, 0.00f, 1.00f, 1.10f, 1.04f, -0.45f},
                         LP_PARTICLE_HEART,
                         3.0f},
    [LP_EMOTION_ANGRY] = {"angry",
                          {0.25f, 0.70f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.60f, 0.96f, 0.15f},
                          LP_PARTICLE_NONE,
                          0.0f},
    [LP_EMOTION_SQUINT] = {"squint",
                           {0.12f, 0.00f, 0.00f, 0.68f, 0.00f, 0.37f, 0.00f, 0.80f, 1.04f, 0.00f},
                           LP_PARTICLE_NONE,
                           0.0f},
    [LP_EMOTION_FROWN] = {"frown",
                          {0.00f, 0.00f, 0.00f, 0.00f, 0.72f, 0.65f, 0.00f, 0.80f, 1.04f, 0.00f},
                          LP_PARTICLE_NONE,
                          0.0f},
    [LP_EMOTION_SAD] = {"sad",
                        {0.20f, -0.50f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 1.15f, 0.98f, 0.45f},
                        LP_PARTICLE_NONE,
                        0.0f},
    [LP_EMOTION_SURPRISED] = {"surprised",
                              {0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.60f, 1.12f, 0.00f},
                              LP_PARTICLE_NONE,
                              0.0f},
    [LP_EMOTION_SLEEPY] = {"sleepy",
                           {0.55f, -0.10f, 0.05f, 0.00f, 0.00f, 0.00f, 0.00f, 1.00f, 1.00f, 0.35f},
                           LP_PARTICLE_SPARKLE,
                           0.4f},
};
/* clang-format on */

const lp_emotion_def *lp_emotion_get(lp_emotion emotion)
{
    if ((unsigned)emotion >= LP_EMOTION_COUNT)
        return NULL;
    return &EMOTIONS[emotion];
}

lp_emotion lp_emotion_from_name(const char *name)
{
    for (int i = 0; i < LP_EMOTION_COUNT; i++)
        if (strcmp(EMOTIONS[i].name, name) == 0)
            return (lp_emotion)i;
    return LP_EMOTION_COUNT;
}
