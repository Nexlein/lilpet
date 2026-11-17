/*
 * When the eyes rest (docs/objet-v1.md): after REST_IDLE_US with nothing happening (a touch, the button, motion, a
 * command), the board looks sleepy for REST_DROWSY_US, then sleeps: screen off, drawing paused. Whatever happens wakes
 * it. Free of ESP-IDF, so the host tests check it (firmware/tests): main.c gives it the time and what happened.
 */
#ifndef LILPET_REST_H
#define LILPET_REST_H

#include <stdbool.h>
#include <stdint.h>

#define REST_IDLE_US   120000000 /* microseconds with nothing happening before the sleepy look (proposed: 2 min) */
#define REST_DROWSY_US 5000000   /* microseconds of the sleepy look before the screen goes off */
/* In g, on one axis, away from the gravity of the last time it moved: the board was handled. More than the sensor's
 * noise, and measured from that reference rather than from the frame before, so that a slow tilt counts too. */
#define REST_MOVED_G 0.15f

typedef enum { REST_AWAKE, REST_DROWSY, REST_ASLEEP } rest_state;

typedef struct {
    int64_t last_activity; /* microseconds */
} rest;

static inline rest rest_start(int64_t now)
{
    return (rest){.last_activity = now};
}

/* Something happened: the board stays awake, or wakes up */
static inline void rest_activity(rest *r, int64_t now)
{
    r->last_activity = now;
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

/* Whether gravity, in g, moved away from `reference` enough to count as the board being handled */
static inline bool rest_moved(const float reference[3], const float now[3])
{
    for (int i = 0; i < 3; i++)
        if (now[i] - reference[i] > REST_MOVED_G || reference[i] - now[i] > REST_MOVED_G)
            return true;
    return false;
}

#endif /* LILPET_REST_H */
