/*
 * M5Stamp S3A, bare: no screen. It measures the engine on the chip: the CO5300 driver sends the bands at 466 px on the
 * Waveshare's pins, unconnected here, so the timings stay meaningful. No TE pin: frames are paced.
 */
#ifndef LILPET_BOARD_CONFIG_H
#define LILPET_BOARD_CONFIG_H

#define BOARD_NAME         "stamp_s3a"
#define BOARD_DISPLAY_SIZE 466
#define BOARD_BENCH        1 /* 1: no screen, plays the benchmark animation */
#define BOARD_CAMERA       0
#define BOARD_MOTION       0 /* 1: has a motion sensor, which must answer */
#define BOARD_CHIP_PICO    0 /* ESP32-S3FN8, in the QFN56 package */

#define BOARD_LCD_CS            10
#define BOARD_LCD_CLK           11
#define BOARD_LCD_D0            12
#define BOARD_LCD_D1            13
#define BOARD_LCD_D2            14
#define BOARD_LCD_D3            15
#define BOARD_LCD_RESET         8
#define BOARD_LCD_COLUMN_OFFSET 6 /* first visible column of the panel */

#endif
