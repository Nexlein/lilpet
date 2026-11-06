/* Motion sensor: one .c per sensor (AtomS3R: imu_bmi270.c), imu_none.c on boards without one (board.cmake) */
#ifndef LILPET_IMU_H
#define LILPET_IMU_H

#include <stdbool.h>

/* Finds and starts the sensor; false on a board without one */
bool imu_init(void);

/*
 * Gravity in g, in screen coordinates: x right, y down, z out of the screen toward the viewer (so 0, 1, 0 standing
 * upright, 0, 0, -1 lying face up). False without a sensor or on a read error. Safe from any task.
 */
bool imu_gravity(float out[3]);

#endif
