/* Waveshare ESP32-S3-Touch-AMOLED-1.32: 466 px round AMOLED, no motion sensor. Pins from Waveshare's ESP-IDF demo. */
#ifndef LILPET_BOARD_CONFIG_H
#define LILPET_BOARD_CONFIG_H

#define BOARD_DISPLAY_SIZE 466
#define BOARD_BENCH        0 /* 1: no screen, plays the benchmark animation */
#define BOARD_CHIP_PICO    1 /* ESP32-S3-PICO-1 */

#define BOARD_POWER_HOLD 18 /* keeps the board on when running on battery, once the power button is released */

#define BOARD_LCD_CS            10
#define BOARD_LCD_CLK           11
#define BOARD_LCD_D0            12
#define BOARD_LCD_D1            13
#define BOARD_LCD_D2            14
#define BOARD_LCD_D3            15
#define BOARD_LCD_RESET         8
#define BOARD_LCD_TE            9
#define BOARD_LCD_COLUMN_OFFSET 6 /* first visible column of the panel */

#endif
