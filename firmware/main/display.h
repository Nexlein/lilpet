/*
 * The board's screen, found at startup (display.c): the round AMOLED of the Waveshare ESP32-S3-Touch-AMOLED-1.32
 * (display_co5300.c), the small LCD of the M5Stack AtomS3R (display_atoms3r.c), or none on a bare board. Every
 * driver takes bands of RGB565 pixels with the same calls.
 */
#ifndef LILPET_DISPLAY_H
#define LILPET_DISPLAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *name; /* what was found, for the startup log */
    int size;         /* side of the square screen in pixels: what the engine draws (lp_renderer_set_size) */
    bool present;     /* a screen was found; on a bare board, the bands go out on unconnected pins */
    bool synced;      /* frames can start on the display's refresh signal (TE); otherwise the caller paces them */
} display_info;

/* Finds and starts the board's display. max_band_bytes is the largest band display_draw will send. */
display_info display_init(size_t max_band_bytes);

/* Waits for the start of the next display refresh (TE signal), at most timeout_ms; false on timeout or without TE */
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
