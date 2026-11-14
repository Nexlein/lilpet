/*
 * A frame on the board (main.c): the rows of a band each core draws, and the share of the screen a frame redraws, for
 * the timing reports. Without ESP-IDF, so the host tests check it (firmware/tests).
 */
#ifndef LILPET_FRAME_H
#define LILPET_FRAME_H

#include "lilpet/lilpet.h"

#include <stdint.h>

/* A band of the frame being drawn: rows y0 to y1 (excluded) of its dirty region, each `width` pixels */
typedef struct {
    int y0, y1, width;
    uint16_t *pixels;
} frame_band;

/*
 * Draws the rows of a band that core `core` (0 or 1) draws, from the frame `r` prepared: every other row, from the
 * band's first row plus `core`, so that each core draws half of every shape, whatever its height. Core 0 draws with the
 * renderer's own scratch (lp_render_rows), core 1 with `second`, its own.
 */
static inline void frame_draw_core_rows(lp_renderer *r, int core, lp_row_scratch *second, const frame_band *band)
{
    for (int y = band->y0 + core; y < band->y1; y += 2) {
        uint16_t *row = &band->pixels[(y - band->y0) * band->width];
        if (core == 0)
            lp_render_rows(r, y, y + 1, row);
        else
            lp_render_rows_with(r, second, y, y + 1, row);
    }
}

/* The share of a size x size screen that a region covers, in thousandths; an empty region ({size, size, 0, 0}) is 0 */
static inline int64_t frame_area_permille(lp_rect r, int size)
{
    if (r.x1 <= r.x0 || r.y1 <= r.y0)
        return 0;
    return (int64_t)(r.x1 - r.x0) * (r.y1 - r.y0) * 1000 / ((int64_t)size * size);
}

#endif /* LILPET_FRAME_H */
