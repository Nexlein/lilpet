/* What sets the boards apart, for the one firmware that runs on all of them */
#ifndef LILPET_BOARD_H
#define LILPET_BOARD_H

#include "driver/i2c_master.h"

#include <stdbool.h>

/*
 * The AtomS3R carries an ESP32-S3-PICO, the Waveshare board and the M5Stamp S3A a plain ESP32-S3: the package tells
 * them apart, as M5Stack's own library does (M5GFX). The pins of one board are never driven on another.
 */
bool board_is_atoms3r(void);

/* The AtomS3R's internal I2C bus (backlight driver, motion sensor), started on first use; NULL on other boards */
i2c_master_bus_handle_t board_i2c(void);

#endif
