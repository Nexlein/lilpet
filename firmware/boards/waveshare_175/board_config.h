/*
 * Waveshare ESP32-S3-Touch-AMOLED-1.75, standard version: 466 px round AMOLED, the final screen board
 * (docs/materiel.md). Pins from Waveshare's hardware reference and the board support package of its ESP-IDF demos. Not
 * tried on the board yet: its QMI8658, CST9217 touch, TCA9554 expander and power button are not driven yet (#54).
 */
#ifndef LILPET_BOARD_CONFIG_H
#define LILPET_BOARD_CONFIG_H

#define BOARD_DISPLAY_SIZE 466
#define BOARD_BENCH        0 /* 1: no screen, plays the benchmark animation */
#define BOARD_CHIP_PICO    0 /* ESP32-S3R8, in the QFN56 package */

/* The shared bus: AXP2101 0x34, QMI8658 0x6B, CST9217 0x5A, TCA9554 0x20, PCF85063 0x51, ES8311 0x18, ES7210 0x40 */
#define BOARD_I2C_SDA 15
#define BOARD_I2C_SCL 14

/* The AXP2101's charge current, in mA: the chosen battery's maximum (a 400 mAh 802525, docs/materiel.md) */
#define BOARD_CHARGE_MA 200

#define BOARD_LCD_CS            12
#define BOARD_LCD_CLK           38
#define BOARD_LCD_D0            4
#define BOARD_LCD_D1            5
#define BOARD_LCD_D2            6
#define BOARD_LCD_D3            7
#define BOARD_LCD_RESET         39
#define BOARD_LCD_TE            13
#define BOARD_LCD_COLUMN_OFFSET 6     /* first visible column of the panel, as the demo's gap */
#define BOARD_LCD_ORIENTATION   0x00u /* the demo's rotation 0 */
/* Vendor commands on page 0x20, which the 1.75"'s demo sends before the 1.32"'s sequence */
#define BOARD_LCD_INIT_FIRST {0xFEu, 1, {0x20u}, 0}, {0x19u, 1, {0x10u}, 0}, {0x1Cu, 1, {0xA0u}, 0},

#endif
