/* Picks the display driver for the board (board.h) */
#include "display.h"

#include "board.h"
#include "display_driver.h"

static const display_driver *driver = &DISPLAY_CO5300;

display_info display_init(size_t max_band_bytes)
{
    if (board_is_atoms3r())
        driver = &DISPLAY_ATOMS3R;
    return driver->init(max_band_bytes);
}

bool display_wait_refresh(int timeout_ms)
{
    return driver->wait_refresh(timeout_ms);
}

void display_draw(int x0, int y0, int x1, int y1, uint16_t *pixels)
{
    driver->draw(x0, y0, x1, y1, pixels);
}

void display_flush(void)
{
    driver->flush();
}

void display_brightness(uint8_t level)
{
    driver->brightness(level);
}
