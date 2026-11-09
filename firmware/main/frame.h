/*
 * The arithmetic of a frame on the board (main.c): which rows of a band each core draws, and the share of the screen a
 * frame redraws, for the timing reports. Pure, so the host tests check it (firmware/tests).
 */
#ifndef LILPET_FRAME_H
#define LILPET_FRAME_H

#include "lilpet/lilpet.h"

#include <stdint.h>

/* A band holds rows y0 to y1 (excluded): this core draws up to the row returned, the second core from there */
static inline int frame_band_half(int y0, int y1)
{
    return y0 + (y1 - y0) / 2;
}

/* The share of a size x size screen that a region covers, in thousandths; an empty region ({size, size, 0, 0}) is 0 */
static inline int64_t frame_area_permille(lp_rect r, int size)
{
    if (r.x1 <= r.x0 || r.y1 <= r.y0)
        return 0;
    return (int64_t)(r.x1 - r.x0) * (r.y1 - r.y0) * 1000 / ((int64_t)size * size);
}

#endif /* LILPET_FRAME_H */
