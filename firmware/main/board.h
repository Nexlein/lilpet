/*
 * The board this image is built for: its pins and mounting are in boards/<board>/board_config.h, picked at build time.
 * Flash the image of the board at hand: another board's pins would be driven. Never flash this firmware on the XIAO
 * ESP32S3 Sense camera board, whose camera outputs sit on the display pins: it gets its own app.
 */
#ifndef LILPET_BOARD_H
#define LILPET_BOARD_H

#include "driver/i2c_master.h"

#include <stdbool.h>

/* Sets the pins of the board itself: power hold, button. First thing at startup. On a chip package that is not the
 * board's, it prints why and never returns, with no pin touched. */
void board_init(void);

/* The user button is held down (the AtomS3R's screen is one); false on boards without one */
bool board_button_pressed(void);

/* The board's internal I2C bus (backlight driver, motion sensor), started on first use; only on boards with one */
i2c_master_bus_handle_t board_i2c(void);

/* How long a request on that bus waits for its device */
#define BOARD_I2C_TIMEOUT_MS 50

/* A device of that bus at its 7-bit address, at the bus's one speed; NULL when there is no bus or it cannot be added */
i2c_master_dev_handle_t board_i2c_device(uint16_t address);

#endif
