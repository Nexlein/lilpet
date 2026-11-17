/*
 * Which chip the board's image is for: its package, its flash and its PSRAM, all of which the chip reports before any
 * pin is touched (board_init). The package alone tells the PICO boards from the QFN56 ones, but the XIAO ESP32S3 Sense
 * (the camera board, an ESP32-S3R8 with 8 MB of flash) shares its package with the 1.75" and the M5Stamp S3A: its
 * flash tells it from the first, its PSRAM from the second. Free of ESP-IDF, so the host tests check it
 * (firmware/tests).
 */
#ifndef LILPET_CHIP_H
#define LILPET_CHIP_H

#include <stdbool.h>

typedef struct {
    bool pico;    /* ESP32-S3-PICO-1 package, else QFN56 */
    int flash_mb; /* the flash's size, read from the flash chip itself */
    int psram_mb; /* PSRAM inside the package, from its eFuse; 0 for none */
} chip;

/* The PSRAM_CAP eFuse's value in MB, as ESP-IDF's eFuse table lists it (esp32s3/esp_efuse_table.csv) */
static inline int chip_psram_mb(unsigned cap)
{
    switch (cap) {
    case 1:
        return 8;
    case 2:
        return 2;
    case 3:
        return 16;
    case 4:
        return 4;
    default:
        return 0;
    }
}

static inline bool chip_same(chip a, chip b)
{
    return a.pico == b.pico && a.flash_mb == b.flash_mb && a.psram_mb == b.psram_mb;
}

#endif /* LILPET_CHIP_H */
