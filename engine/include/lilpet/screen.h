/* The screen, shared by lilpet.h and render_state.h: its size, the particles it shows, a region of it. */
#ifndef LILPET_SCREEN_H
#define LILPET_SCREEN_H

#define LP_SCREEN_SIZE   466
#define LP_MAX_PARTICLES 32

/* Pixel rectangle, x1 and y1 excluded; empty when x0 >= x1 or y0 >= y1 */
typedef struct {
    int x0, y0, x1, y1;
} lp_rect;

#endif /* LILPET_SCREEN_H */
