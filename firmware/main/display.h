/*
 * The board's screen. This header is the interface; each display has its own .c, linked by CMake for the board
 * (board.cmake): the round AMOLED of the Waveshare ESP32-S3-Touch-AMOLED-1.32 (display_co5300.c, also on bare
 * boards, where the bands go out on unconnected pins) or the small LCD of the M5Stack AtomS3R (display_atoms3r.c).
 * Every driver takes bands of RGB565 pixels with the same calls.
 */
#ifndef LILPET_DISPLAY_H
#define LILPET_DISPLAY_H

#include <stddef.h>
#include <stdint.h>

/* Starts the board's display and returns what it is, for the startup log. max_band_bytes is the largest band
 * display_draw will send. */
const char *display_init(size_t max_band_bytes);

/* Waits until the next frame can start. A driver with a TE signal defines it under #ifdef BOARD_LCD_TE, to wait for the
 * signal; display_paced.c gives it to every board without BOARD_LCD_TE */
void display_wait_frame(void);

/*
 * Sends a band: columns x0 to x1 and rows y0 to y1 (excluded, even coordinates), RGB565 high byte first, the order of
 * every controller here (lp_renderer_set_big_endian). Returns once the band is queued: the DMA sends it while the
 * caller draws the next band in another buffer. `pixels` must stay untouched until the next display call.
 */
void display_draw(int x0, int y0, int x1, int y1, const uint16_t *pixels);

/* Waits until every queued band is sent; after the first frame, a panel that waited for it turns on (CO5300) */
void display_flush(void);

#endif /* LILPET_DISPLAY_H */
