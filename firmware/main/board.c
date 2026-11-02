#include "board.h"

#include "driver/gpio.h"
#include "esp_efuse.h"
#include "soc/efuse_defs.h"

#include <stdio.h>

#define ATOMS3R_SDA GPIO_NUM_45
#define ATOMS3R_SCL GPIO_NUM_0
/* Pressing the screen: low when held, as M5Stack's library reads it (M5Unified, board_M5AtomS3R) */
#define ATOMS3R_BUTTON GPIO_NUM_41

bool board_is_atoms3r(void)
{
    static int atoms3r = -1; /* read once: asked every frame for the button */
    if (atoms3r < 0)
        atoms3r = esp_efuse_get_pkg_ver() == EFUSE_PKG_VERSION_ESP32S3PICO;
    return atoms3r;
}

bool board_button_pressed(void)
{
    static bool ready;
    if (!board_is_atoms3r())
        return false;
    if (!ready) {
        const gpio_config_t input = {
            .pin_bit_mask = 1ULL << ATOMS3R_BUTTON, .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE};
        gpio_config(&input);
        ready = true;
    }
    return gpio_get_level(ATOMS3R_BUTTON) == 0;
}

i2c_master_bus_handle_t board_i2c(void)
{
    static i2c_master_bus_handle_t bus;
    if (bus || !board_is_atoms3r())
        return bus;
    const i2c_master_bus_config_t config = {
        .i2c_port = -1,
        .sda_io_num = ATOMS3R_SDA,
        .scl_io_num = ATOMS3R_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    if (i2c_new_master_bus(&config, &bus) != ESP_OK) {
        printf("cannot start the I2C bus\n");
        bus = NULL;
    }
    return bus;
}
