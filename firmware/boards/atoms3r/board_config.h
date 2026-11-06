/* M5Stack AtomS3R: 128 px LCD, BMI270 motion sensor, the screen is a button. Values from M5Stack's library (M5GFX). */
#ifndef LILPET_BOARD_CONFIG_H
#define LILPET_BOARD_CONFIG_H

#define BOARD_DISPLAY_SIZE 128
#define BOARD_BENCH        0 /* 1: no screen, plays the benchmark animation */
#define BOARD_CAMERA       0
#define BOARD_CHIP_PICO    1 /* ESP32-S3-PICO-1 */

/* Internal I2C bus: backlight driver, motion sensor */
#define BOARD_I2C_SDA 45
#define BOARD_I2C_SCL 0

/* Pressing the screen: low when held, as M5Stack's library reads it (M5Unified, board_M5AtomS3R) */
#define BOARD_BUTTON 41

#define BOARD_LCD_SCLK  15
#define BOARD_LCD_MOSI  21 /* also read back: the controller answers on the same wire (3-wire SPI) */
#define BOARD_LCD_DC    42
#define BOARD_LCD_CS    14
#define BOARD_LCD_RESET 48

#define BOARD_BACKLIGHT_ADDR 0x30u

#define BOARD_IMU_ADDR 0x68u
/*
 * Sensor axes to screen axes: which sensor axis, and its sign. Measured on the board (USB port down, then on the
 * left): the sensor's x points to the port, its y to the right. It measures the reaction to gravity (+1 g up when
 * lying still), so the signs also turn it into gravity.
 */
#define BOARD_IMU_AXES {{1, -1.0f}, {0, -1.0f}, {2, -1.0f}}

#endif
