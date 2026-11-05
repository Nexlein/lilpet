/*
 * The board this image is built for: its pins and mounting are in boards/<board>/board_config.h, picked at build time.
 * Flash the image of the board at hand: another board's pins would be driven. Never flash this firmware on the XIAO
 * ESP32S3 Sense camera board, whose camera outputs sit on the display pins: it gets its own app.
 */
#ifndef LILPET_BOARD_H
#define LILPET_BOARD_H

#include "driver/i2c_master.h"

#include <stdbool.h>

/* Sets the pins of the board itself: power hold, button. First thing at startup. */
void board_init(void);

/* The user button is held down (the AtomS3R's screen is one); false on boards without one */
bool board_button_pressed(void);

/* The board's internal I2C bus (backlight driver, motion sensor), started on first use; only on boards with one */
i2c_master_bus_handle_t board_i2c(void);

/* Turns the backlight on, once the panel is ready to show something; only on boards with a backlight driver */
void board_backlight_on(void);

#endif
