/*
 * What both display drivers send: the standard MIPI DCS commands their controllers take (GC9107 and ST7735S of the
 * AtomS3R, CO5300 of the round screen), and a range of columns or rows in the form those commands take it. Pure, so the
 * host tests check it (firmware/tests).
 */
#ifndef LILPET_DCS_H
#define LILPET_DCS_H

#include <stdint.h>

#define DCS_NOP          0x00u
#define DCS_READ_ID      0x04u
#define DCS_SLEEP_OUT    0x11u
#define DCS_INVERT_OFF   0x20u
#define DCS_INVERT_ON    0x21u
#define DCS_DISPLAY_ON   0x29u
#define DCS_COLUMNS      0x2Au /* followed by a range (dcs_range) */
#define DCS_ROWS         0x2Bu /* same */
#define DCS_WRITE_RAM    0x2Cu
#define DCS_TE_ON        0x35u
#define DCS_ORIENTATION  0x36u
#define DCS_PIXEL_FORMAT 0x3Au
#define DCS_BRIGHTNESS   0x51u

/* First to last, both included, in the four big-endian bytes of DCS_COLUMNS and DCS_ROWS */
static inline void dcs_range(int first, int last, uint8_t out[4])
{
    out[0] = (uint8_t)(first >> 8);
    out[1] = (uint8_t)first;
    out[2] = (uint8_t)(last >> 8);
    out[3] = (uint8_t)last;
}

#endif /* LILPET_DCS_H */
