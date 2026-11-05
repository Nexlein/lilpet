/*
 * From what the hardware reports to the state of the eyes, the same way on every board. The firmware only reads its
 * sensors and hands the values over, so all of this runs in the host tests.
 */
#include "internal.h"
#include "lilpet/lilpet.h"

#define GRAVITY_RATE 12.0f /* per second: smooths the sensor noise, which would shake the eyes */
#define MOTION_LOST  0.25f /* seconds without a sample: the sensor stopped answering */
#define BUTTON_SETTLE                                                                                                  \
    0.03f              /* seconds: a contact bounce is shorter, so a press after a shorter release is the same one */
#define FACE_LOST 0.5f /* seconds without a position: the camera no longer sees the face */
#define FACE_NEW  3.0f /* seconds away before a face counts as new: detection flickers, which is not an arrival */

void lp_senses_init(lp_senses *senses)
{
    *senses = (lp_senses){.gravity = {0.0f, 1.0f}, .motion_age = MOTION_LOST, .face_age = FACE_NEW};
}

static void update_motion(lp_senses *s, lp_eyes *eyes, const float *accel, float dt)
{
    if (accel && isfinite(accel[0]) && isfinite(accel[1]) && isfinite(accel[2])) {
        s->motion_age = 0.0f;
        /* Raw sample: smoothing hides a shake */
        lp_eyes_set_shaking(eyes, lp_shake_update(&s->shake, accel[0], accel[1], accel[2], dt));
        for (int i = 0; i < 2; i++)
            s->gravity[i] = lp_approach(s->gravity[i], accel[i], GRAVITY_RATE, dt);
        lp_eyes_set_gravity(eyes, true, s->gravity[0], s->gravity[1]);
        return;
    }
    s->motion_age += dt;
    if (s->motion_age >= MOTION_LOST) {
        lp_eyes_set_shaking(eyes, false);
        lp_eyes_set_gravity(eyes, false, 0.0f, 0.0f);
    }
}

static void update_button(lp_senses *s, lp_eyes *eyes, bool pressed, float dt)
{
    s->button_age += dt;
    if (pressed == s->pressed)
        return;
    s->pressed = pressed;
    if (pressed && s->button_age > BUTTON_SETTLE)
        lp_eyes_react(eyes, LP_EVENT_TAP);
    s->button_age = 0.0f;
}

void lp_senses_update(lp_senses *senses, lp_eyes *eyes, const float *accel, bool pressed, float dt)
{
    update_motion(senses, eyes, accel, dt);
    update_button(senses, eyes, pressed, dt);
    senses->face_age += dt;
    if (senses->face && senses->face_age >= FACE_LOST) {
        senses->face = false;
        lp_eyes_set_pointer(eyes, false, 0.0f, 0.0f);
    }
}

void lp_senses_face(lp_senses *senses, lp_eyes *eyes, bool seen, float x, float y)
{
    if (!seen) {
        senses->face = false;
        lp_eyes_set_pointer(eyes, false, 0.0f, 0.0f);
        return;
    }
    if (!isfinite(x) || !isfinite(y))
        return;
    if (senses->face_age >= FACE_NEW)
        lp_eyes_react(eyes, LP_EVENT_FACE_SEEN);
    senses->face = true;
    senses->face_age = 0.0f;
    lp_eyes_set_pointer(eyes, true, x, y);
}
