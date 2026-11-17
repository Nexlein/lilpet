/*
 * The board this image is built for: its pins and mounting are in boards/<board>/board_config.h, picked at build time.
 * Flash the image of the board at hand: another board's pins would be driven. Never flash this firmware on the XIAO
 * ESP32S3 Sense camera board, whose camera outputs sit on the display pins: it gets its own app.
 */
#ifndef LILPET_BOARD_H
#define LILPET_BOARD_H

#include "driver/i2c_master.h"

#include <stdbool.h>
#include <stdint.h>

/* Sets the pins of the board itself: power hold, button. First thing at startup. On a chip that is not the board's
 * (chip.h: package, flash, PSRAM), it prints why and never returns, with no pin touched. */
void board_init(void);

/* Whether the board has a user button (the AtomS3R's screen is one), which can wake it up */
bool board_has_button(void);

/* The user button is held down; false on boards without one */
bool board_button_pressed(void);

/* Light sleep for up to `us` microseconds, or until the button leaves the state `button_held` says: pressed, or let go
 * while it is held. Only with no USB host: light sleep turns the chip's USB pad off, and a PC would lose the port. */
void board_light_sleep(uint64_t us, bool button_held);

/* The board's internal I2C bus (backlight driver, motion sensor, power chip), started on first use; only on boards with
 * one */
i2c_master_bus_handle_t board_i2c(void);

/* How long a request on that bus waits for its device */
#define BOARD_I2C_TIMEOUT_MS 50

/* A device of that bus at its 7-bit address, at the bus's one speed; NULL when there is no bus or it cannot be added */
i2c_master_dev_handle_t board_i2c_device(uint16_t address);

/* Writes `len` bytes to the register `reg` of a device of that bus; false when it does not answer within
 * BOARD_I2C_TIMEOUT_MS */
bool board_i2c_write(i2c_master_dev_handle_t device, uint8_t reg, const uint8_t *data, size_t len);

/* Reads `len` bytes from the register `reg` of a device of that bus; false when it does not answer */
bool board_i2c_read(i2c_master_dev_handle_t device, uint8_t reg, uint8_t *data, size_t len);

#endif
