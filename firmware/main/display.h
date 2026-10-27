/*
 * Driver of the round AMOLED of the Waveshare ESP32-S3-Touch-AMOLED-1.32: 466x466, CO5300 controller, QSPI. Pins,
 * init sequence, 180-degree rotation and 6-column offset come from Waveshare's ESP-IDF demo (07_LVGL_V9_Test,
 * components/lvgl_port_bsp); to check on the board: orientation, offset, colors and tearing.
 */
#ifndef LILPET_DISPLAY_H
#define LILPET_DISPLAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Keeps the board powered, resets and starts the display. max_band_bytes is the largest band display_draw will
 * send. Returns whether the display's refresh signal (TE) was seen: false without a display, e.g. on a bare board.
 */
bool display_init(size_t max_band_bytes);

/* Waits for the start of the next display refresh (TE signal), at most timeout_ms; false on timeout */
bool display_wait_refresh(int timeout_ms);

/*
 * Sends a band: columns x0 to x1 and rows y0 to y1 (excluded, even coordinates), RGB565 in native byte order,
 * converted in place for the controller. Returns once the band is queued: the DMA sends it while the caller draws
 * the next band in another buffer. `pixels` must stay untouched until the next display call.
 */
void display_draw(int x0, int y0, int x1, int y1, uint16_t *pixels);

/* Waits until every queued band is sent */
void display_flush(void);

/* Brightness, 0 to 255 */
void display_brightness(uint8_t level);

#endif /* LILPET_DISPLAY_H */
