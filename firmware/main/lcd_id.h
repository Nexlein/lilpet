/*
 * Which controller drives the AtomS3R's LCD, from its answer to the read-ID command, as M5GFX tells them apart. Pure,
 * so the host tests check it (firmware/tests): the ST7735S variant has never run on a board.
 */
#ifndef LILPET_LCD_ID_H
#define LILPET_LCD_ID_H

#include <stddef.h>
#include <stdint.h>

typedef enum { LCD_ID_UNKNOWN, LCD_ID_ST7735S, LCD_ID_GC9107 } lcd_id;

#define LCD_ID_ANSWER 6 /* bytes read after the command */

/* The answer may start after a dummy bit or byte, so every alignment of the bits read is tried */
static inline lcd_id lcd_id_match(const uint8_t answer[LCD_ID_ANSWER])
{
    uint64_t bits = 0;
    for (size_t i = 0; i < LCD_ID_ANSWER; i++)
        bits = bits << 8u | answer[i];
    for (unsigned shift = 0; shift <= 16u; shift++) {
        const uint32_t word = (uint32_t)(bits >> (16u - shift)); /* 4 bytes as they came, first byte highest */
        const uint32_t id = __builtin_bswap32(word);             /* first byte lowest, as M5GFX compares them */
        if ((id & 0xFFFFu) == 0x7683u || (id & 0xFFFFu) == 0x897Cu)
            return LCD_ID_ST7735S;
        if ((id & 0xFFFFFFu) == 0x079100u)
            return LCD_ID_GC9107;
    }
    return LCD_ID_UNKNOWN;
}

#endif /* LILPET_LCD_ID_H */
