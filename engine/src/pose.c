/* Geometry derived from the state: blink lid and eye scale, eye position on the virtual 3D face */
#include "internal.h"
#include "lilpet/lilpet.h"

/* Virtual 3D face: eye depth, camera distance, eyes facing slightly outward */
#define FACE_DEPTH   130.0f
#define FACE_CAMERA  600.0f
#define FACE_OUTWARD 0.35f

lp_blink_pose lp_eyes_blink_pose(const lp_eyes *eyes)
{
    const float t = eyes->blink.t;
    switch (eyes->blink.phase) {
    case LP_BLINK_ANTICIP: {
        const float p = lp_minf(t / LP_BLINK_ANTICIP_TIME, 1.0f);
        return (lp_blink_pose){.lid = 0.1f * p, .sx = 1.0f + 0.05f * p, .sy = 1.0f - 0.03f * p, .closed = false};
    }
    case LP_BLINK_CLOSE: {
        const float p = lp_ease_in(lp_minf(t / LP_BLINK_CLOSE_TIME, 1.0f));
        return (lp_blink_pose){.lid = 0.1f + 0.9f * p, .sx = 1.05f, .sy = 0.97f, .closed = false};
    }
    case LP_BLINK_CLOSED:
        return (lp_blink_pose){.lid = 1.0f, .sx = 1.04f, .sy = 1.0f, .closed = true};
    case LP_BLINK_OPEN: {
        const float p = lp_ease_out(lp_minf(t / eyes->blink.open, 1.0f));
        return (lp_blink_pose){.lid = 1.0f - p, .sx = 1.04f - 0.04f * p, .sy = 0.97f + 0.03f * p, .closed = false};
    }
    case LP_BLINK_NONE:
    default:
        return (lp_blink_pose){.lid = 0.0f, .sx = 1.0f, .sy = 1.0f, .closed = false};
    }
}

/* Yaw then pitch rotation of a 3D vector */
static void rotate_head(const lp_eyes *e, float v[3])
{
    const float cy = cosf(e->head.yaw);
    const float sy = sinf(e->head.yaw);
    const float cp = cosf(e->head.pitch);
    const float sp = sinf(e->head.pitch);
    const float x1 = v[0] * cy + v[2] * sy;
    const float z1 = -v[0] * sy + v[2] * cy;
    const float y = v[1];
    v[0] = x1;
    v[1] = y * cp + z1 * sp;
    v[2] = -y * sp + z1 * cp;
}

lp_eye_pose lp_eyes_eye_pose(const lp_eyes *eyes, int side)
{
    const float s = (float)side;
    float pos[3] = {s * eyes->shape.gap, 0.0f, FACE_DEPTH};
    rotate_head(eyes, pos);
    const float persp =
        FACE_CAMERA / (FACE_CAMERA - pos[2]) / (FACE_CAMERA / (FACE_CAMERA - FACE_DEPTH)); /* 1 at rest */

    /* Eye surface normal: flatter on screen as it turns away from the viewer */
    const float n = hypotf(FACE_OUTWARD, 1.0f);
    float normal[3] = {s * FACE_OUTWARD / n, 0.0f, 1.0f / n};
    rotate_head(eyes, normal);
    const float rest_x = FACE_OUTWARD / n;
    const float fx = sqrtf(1.0f - normal[0] * normal[0]) / sqrtf(1.0f - rest_x * rest_x);
    const float fy = sqrtf(1.0f - normal[1] * normal[1]);

    /* Roll turns the whole face around the screen center */
    const float cr = cosf(eyes->head.roll);
    const float sr = sinf(eyes->head.roll);
    const float px = pos[0] * persp;
    const float py = pos[1] * persp;
    return (lp_eye_pose){
        .x = LP_CENTER + px * cr - py * sr, .y = LP_CENTER + px * sr + py * cr, .persp = persp, .fx = fx, .fy = fy};
}
