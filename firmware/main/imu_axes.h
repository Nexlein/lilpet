/*
 * From the raw sample of a motion sensor to gravity on the screen axes (x right, y down, z toward the viewer), with
 * the mounting of the board (BOARD_IMU_AXES in board_config.h). Pure, so the host tests check it (firmware/tests).
 */
#ifndef LILPET_IMU_AXES_H
#define LILPET_IMU_AXES_H

#include <stdint.h>

/* Which sensor axis gives a screen axis, and its sign */
typedef struct {
    int axis;
    float sign;
} imu_axis;

/* raw: x, y, z as signed 16-bit values, low byte first; lsb_per_g: the sensor's counts for 1 g */
static inline void imu_axes_to_screen(const uint8_t raw[6], const imu_axis map[3], float lsb_per_g, float out[3])
{
    float sensor[3];
    for (int i = 0; i < 3; i++)
        sensor[i] = (float)(int16_t)(uint16_t)(raw[2 * i] | (unsigned)raw[2 * i + 1] << 8u) / lsb_per_g;
    for (int i = 0; i < 3; i++)
        out[i] = map[i].sign * sensor[map[i].axis];
}

#endif /* LILPET_IMU_AXES_H */
