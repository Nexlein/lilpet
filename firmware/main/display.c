/*
 * Picks the display driver from the chip's package, as M5Stack's own library does (M5GFX): the AtomS3R carries an
 * ESP32-S3-PICO, the Waveshare board and the M5Stamp S3A a plain ESP32-S3. The pins of one board are never driven
 * on another.
 */
#include "display.h"

#include "display_driver.h"
#include "esp_efuse.h"
#include "soc/efuse_defs.h"

static const display_driver *driver = &DISPLAY_CO5300;

display_info display_init(size_t max_band_bytes)
{
    if (esp_efuse_get_pkg_ver() == EFUSE_PKG_VERSION_ESP32S3PICO)
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
