/* What sets the boards apart, for the one firmware that runs on all of them */
#ifndef LILPET_BOARD_H
#define LILPET_BOARD_H

#include "driver/i2c_master.h"

#include <stdbool.h>

/*
 * The AtomS3R carries an ESP32-S3-PICO, the Waveshare board and the M5Stamp S3A a plain ESP32-S3: the package tells
 * them apart, as M5Stack's own library does (M5GFX). The pins of one board are never driven on another.
 * A plain ESP32-S3 is taken for the Waveshare board (or a bare one): its display pins are driven. Never flash this
 * firmware on the XIAO ESP32S3 Sense camera board, whose camera outputs sit on those pins: it gets its own app.
 */
bool board_is_atoms3r(void);

/* The AtomS3R's internal I2C bus (backlight driver, motion sensor), started on first use; NULL on other boards */
i2c_master_bus_handle_t board_i2c(void);

/* The user button is held down (the AtomS3R's screen is one); false on boards without one */
bool board_button_pressed(void);

#endif
