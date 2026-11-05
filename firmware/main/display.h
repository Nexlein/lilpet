/*
 * The board's screen. This header is the interface; each display has its own .c, linked by CMake for the board
 * (main/CMakeLists.txt): the round AMOLED of the Waveshare ESP32-S3-Touch-AMOLED-1.32 (display_co5300.c, also on bare
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

/* Waits until the next frame can start: the display's refresh signal (TE), or display_paced.c without one */
void display_wait_frame(void);

/* Every controller here takes the high byte first */
static inline void display_swap_bytes(uint16_t *pixels, size_t count)
{
    for (size_t i = 0; i < count; i++)
        pixels[i] = __builtin_bswap16(pixels[i]);
}

/*
 * Sends a band: columns x0 to x1 and rows y0 to y1 (excluded, even coordinates), RGB565 in native byte order,
 * converted in place for the controller. Returns once the band is queued: the DMA sends it while the caller draws
 * the next band in another buffer. `pixels` must stay untouched until the next display call.
 */
void display_draw(int x0, int y0, int x1, int y1, uint16_t *pixels);

/* Waits until every queued band is sent */
void display_flush(void);

#endif /* LILPET_DISPLAY_H */
