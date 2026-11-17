/*
 * When the eyes rest (docs/objet-v1.md): after REST_IDLE_US with nothing happening (a touch, the button, motion, a
 * command), the board looks sleepy for REST_DROWSY_US, the screen fading out at the end, then sleeps: screen off,
 * drawing paused. Whatever happens wakes it: the screen fades in, and the eyes stay sleepy a moment before their mood.
 * Free of ESP-IDF, so the host tests check it (firmware/tests): main.c gives it the time, what happened, and the eyes
 * to show it on.
 */
#ifndef LILPET_REST_H
#define LILPET_REST_H

#include "lilpet/lilpet.h"

#include <stdbool.h>
#include <stdint.h>

#define REST_IDLE_US     120000000 /* microseconds with nothing happening before the sleepy look (proposed: 2 min) */
#define REST_DROWSY_US   10000000  /* microseconds of the sleepy look before the screen goes off */
#define REST_FADE_OUT_US 2000000   /* the end of the sleepy look, while the screen fades out */
#define REST_FADE_IN_US  500000    /* woken, the screen fades in */
#define REST_WAKING_US   2000000   /* woken, the eyes stay sleepy this long before their mood */
/* In g, on one axis, away from the gravity of the last time it moved: the board was handled. More than the sensor's
 * noise, and measured from that reference rather than from the frame before, so that a slow tilt counts too. */
#define REST_MOVED_G 0.15f

typedef enum { REST_AWAKE, REST_DROWSY, REST_ASLEEP } rest_state;

/* How hello names it (docs/protocole.md) */
static inline const char *rest_name(rest_state state)
{
    return state == REST_ASLEEP ? "asleep" : state == REST_DROWSY ? "drowsy" : "awake";
}

typedef struct {
    int64_t last_activity; /* microseconds */
    int64_t woke_at;       /* when it last woke up from sleep, for the screen's fade in */
    float moved_at[3];     /* gravity the last time the board moved, in g */
    bool gravity_known;
    bool waking_press; /* the press that woke it is still held */
} rest;

static inline rest rest_start(int64_t now)
{
    return (rest){.last_activity = now, .woke_at = now - REST_FADE_IN_US}; /* on at start, no fade */
}

/* Something happened: the board stays awake, or wakes up */
static inline void rest_activity(rest *r, int64_t now)
{
    r->last_activity = now;
}

/* Woken from sleep, by a press of the button or by something else (a command, a move), once the screen is back on: the
 * fade in starts `now` */
static inline void rest_woke(rest *r, int64_t now, bool by_press)
{
    r->last_activity = now;
    r->woke_at = now;
    r->waking_press = by_press;
}

/* Whether the button counts as pressed for the eyes: the press that woke the board does not, until it is let go, or it
 * would be a tap (joy and sparkles) on top of waking up */
static inline bool rest_press(rest *r, bool pressed)
{
    if (r->waking_press && !pressed)
        r->waking_press = false;
    return pressed && !r->waking_press;
}

/* Asleep at once, without the sleepy look (`> test sleep`) */
static inline void rest_sleep_now(rest *r, int64_t now)
{
    r->last_activity = now - REST_IDLE_US - REST_DROWSY_US;
}

static inline rest_state rest_state_at(const rest *r, int64_t now)
{
    const int64_t idle = now - r->last_activity;
    if (idle >= (int64_t)REST_IDLE_US + REST_DROWSY_US)
        return REST_ASLEEP;
    return idle >= REST_IDLE_US ? REST_DROWSY : REST_AWAKE;
}

/* The screen's brightness, from 0 (off) to 255 (as the board sets it): down to 0 over the end of the sleepy look, and
 * from 0 again just after waking up */
static inline int rest_brightness(const rest *r, int64_t now)
{
    const int64_t until_sleep = (int64_t)REST_IDLE_US + REST_DROWSY_US - (now - r->last_activity);
    const int64_t since_wake = now - r->woke_at;
    int level = 255;
    if (until_sleep < REST_FADE_OUT_US)
        level = until_sleep > 0 ? (int)(255 * until_sleep / REST_FADE_OUT_US) : 0;
    if (since_wake < REST_FADE_IN_US) {
        const int fading_in = (int)(255 * since_wake / REST_FADE_IN_US);
        level = fading_in < level ? fading_in : level;
    }
    return level;
}

/* Whether gravity, in g, moved away from `reference` enough to count as the board being handled */
static inline bool rest_moved(const float reference[3], const float now[3])
{
    for (int i = 0; i < 3; i++)
        if (now[i] - reference[i] > REST_MOVED_G || reference[i] - now[i] > REST_MOVED_G)
            return true;
    return false;
}

/* Whether the board was handled: gravity, in g, moved away from where it was the last time it moved. The first reading
 * is where it lies, not a move. */
static inline bool rest_handled(rest *r, const float now[3])
{
    const bool known = r->gravity_known;
    if (known && !rest_moved(r->moved_at, now))
        return false;
    for (int i = 0; i < 3; i++)
        r->moved_at[i] = now[i];
    r->gravity_known = true;
    return known;
}

/*
 * The eyes as they go `from` one state `to` another: the sleepy look before sleep, for a second longer than it lasts,
 * so that the mood does not come back before the screen goes off. Woken from sleep, they stay sleepy for
 * REST_WAKING_US, then get their mood back: nothing could react while they slept, and what wakes them (a command, a
 * press) takes over after. Woken during the sleepy look, they get their mood at once, unless a reaction took over.
 */
static inline void rest_show(lp_eyes *eyes, rest_state from, rest_state to)
{
    if (to == REST_DROWSY)
        lp_eyes_set_emotion(eyes, LP_EMOTION_SLEEPY, (float)REST_DROWSY_US * 1e-6f + 1.0f);
    else if (to == REST_AWAKE && from == REST_ASLEEP)
        lp_eyes_set_emotion(eyes, LP_EMOTION_SLEEPY, (float)REST_WAKING_US * 1e-6f);
    else if (to == REST_AWAKE && eyes->emotion == LP_EMOTION_SLEEPY)
        lp_eyes_set_emotion(eyes, eyes->mood, 0.0f); /* a sleepy mood stays as it was */
}

#endif /* LILPET_REST_H */
