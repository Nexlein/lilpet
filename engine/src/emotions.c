#include "lilpet/lilpet.h"

#include <string.h>

/* Same table as EMOTIONS in prototype/eyes.js (tests/test_parity.c checks it against generated values) */
/* clang-format off: one emotion per line keeps the table readable */
static const lp_emotion_def EMOTIONS[LP_EMOTION_COUNT] = {
    /* name, expression {tl, ts, bl, sq, cr, rot, hl, ps, es, gy, spin} (see lp_expression), particles, rate/s */
    [LP_EMOTION_NEUTRAL] = {"neutral",
                            {0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 1.00f, 1.00f, 0.00f, 0.00f},
                            LP_PARTICLE_NONE,
                            0.0f},
    [LP_EMOTION_HAPPY] = {"happy",
                          {0.00f, 0.00f, 0.42f, 0.00f, 0.00f, 0.00f, 0.00f, 1.08f, 1.02f, 0.00f, 0.00f},
                          LP_PARTICLE_SPARKLE,
                          1.2f},
    [LP_EMOTION_LOVE] = {"love",
                         {0.00f, 0.00f, 0.45f, 0.00f, 0.00f, 0.00f, 1.00f, 1.10f, 1.04f, -0.45f, 0.00f},
                         LP_PARTICLE_HEART,
                         3.0f},
    [LP_EMOTION_ANGRY] = {"angry",
                          {0.25f, 0.70f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.60f, 0.96f, 0.15f, 0.00f},
                          LP_PARTICLE_NONE,
                          0.0f},
    [LP_EMOTION_SQUINT] = {"squint",
                           {0.12f, 0.00f, 0.00f, 0.68f, 0.00f, 0.37f, 0.00f, 0.80f, 1.04f, 0.00f, 0.00f},
                           LP_PARTICLE_NONE,
                           0.0f},
    [LP_EMOTION_FROWN] = {"frown",
                          {0.00f, 0.00f, 0.00f, 0.00f, 0.72f, 0.65f, 0.00f, 0.80f, 1.04f, 0.00f, 0.00f},
                          LP_PARTICLE_NONE,
                          0.0f},
    [LP_EMOTION_SAD] = {"sad",
                        {0.20f, -0.50f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 1.15f, 0.98f, 0.45f, 0.00f},
                        LP_PARTICLE_NONE,
                        0.0f},
    [LP_EMOTION_SURPRISED] = {"surprised",
                              {0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.60f, 1.12f, 0.00f, 0.00f},
                              LP_PARTICLE_NONE,
                              0.0f},
    [LP_EMOTION_SLEEPY] = {"sleepy",
                           {0.55f, -0.10f, 0.05f, 0.00f, 0.00f, 0.00f, 0.00f, 1.00f, 1.00f, 0.35f, 0.00f},
                           LP_PARTICLE_SPARKLE,
                           0.4f},
    [LP_EMOTION_DIZZY] = {"dizzy",
                          {0.30f, -0.20f, 0.12f, 0.00f, 0.00f, 0.00f, 0.00f, 0.75f, 1.00f, 0.00f, 9.00f},
                          LP_PARTICLE_NONE,
                          0.0f},
};
/* clang-format on */

/* Same table as REACTIONS in prototype/eyes.js (checked like the emotions) */
static const lp_reaction REACTIONS[LP_EVENT_COUNT] = {
    [LP_EVENT_TAP] = {"tap", LP_EMOTION_HAPPY, 1.8f, 4},                 /* pressed, or petted */
    [LP_EVENT_SHAKE] = {"shake", LP_EMOTION_DIZZY, 1.5f, 0},             /* shaken (lp_shake_update) */
    [LP_EVENT_UPSIDE_DOWN] = {"upside_down", LP_EMOTION_ANGRY, 0.5f, 0}, /* renewed while it stays upside down */
    [LP_EVENT_FACE_SEEN] = {"face_seen", LP_EMOTION_SURPRISED, 0.8f, 0}, /* someone comes into view */
    [LP_EVENT_GESTURE_LIKE] = {"gesture_like", LP_EMOTION_HAPPY, 2.5f, 4},
    [LP_EVENT_GESTURE_DISLIKE] = {"gesture_dislike", LP_EMOTION_SAD, 2.5f, 0},
    [LP_EVENT_GESTURE_OK] = {"gesture_ok", LP_EMOTION_HAPPY, 1.8f, 2},
    [LP_EVENT_GESTURE_FIVE] = {"gesture_five", LP_EMOTION_SURPRISED, 1.2f, 0}, /* an open hand */
    [LP_EVENT_GESTURE_CALL] = {"gesture_call", LP_EMOTION_LOVE, 2.0f, 0},
    [LP_EVENT_EXPRESSION_HAPPY] = {"expression_happy", LP_EMOTION_HAPPY, 2.0f, 2}, /* the person's face, mirrored */
    [LP_EVENT_EXPRESSION_SAD] = {"expression_sad", LP_EMOTION_SAD, 2.0f, 0},
    [LP_EVENT_EXPRESSION_ANGRY] = {"expression_angry", LP_EMOTION_FROWN, 1.5f, 0},
    [LP_EVENT_EXPRESSION_SURPRISED] = {"expression_surprised", LP_EMOTION_SURPRISED, 1.2f, 0},
};

const lp_reaction *lp_reaction_get(lp_event event)
{
    if ((unsigned)event >= LP_EVENT_COUNT)
        return NULL;
    return &REACTIONS[event];
}

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

lp_event lp_event_from_name(const char *name)
{
    for (int i = 0; i < LP_EVENT_COUNT; i++)
        if (strcmp(REACTIONS[i].name, name) == 0)
            return (lp_event)i;
    return LP_EVENT_COUNT;
}
