/* Emotions and reactions by number or name, from the tables generated from eyes/emotions.json (emotions_table.h) */
#include "emotions_table.h"
#include "internal.h"
#include "lilpet/lilpet.h"

#include <string.h>

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
