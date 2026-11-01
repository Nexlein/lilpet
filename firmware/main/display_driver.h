/* What each display driver provides to display.c, which picks one for the board */
#ifndef LILPET_DISPLAY_DRIVER_H
#define LILPET_DISPLAY_DRIVER_H

#include "display.h"

typedef struct {
    display_info (*init)(size_t max_band_bytes);
    bool (*wait_refresh)(int timeout_ms);
    void (*draw)(int x0, int y0, int x1, int y1, uint16_t *pixels);
    void (*flush)(void);
    void (*brightness)(uint8_t level);
} display_driver;

extern const display_driver DISPLAY_CO5300;  /* Waveshare round AMOLED, also used on boards without a display */
extern const display_driver DISPLAY_ATOMS3R; /* M5Stack AtomS3R */

#endif /* LILPET_DISPLAY_DRIVER_H */
