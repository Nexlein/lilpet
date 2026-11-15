#include "board.h"

#include "board_config.h"
#include "driver/gpio.h"
#include "esp_efuse.h"
#include "esp_err.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/efuse_defs.h"

#include <stdio.h>

void board_init(void)
{
    /* The XIAO's camera outputs sit on the display pins, so nothing touches a pin until the chip package matches. It
     * cannot tell two boards with the same package apart: mise run firmware:flash asks the board first for that. */
    const bool pico = esp_efuse_get_pkg_ver() == EFUSE_PKG_VERSION_ESP32S3PICO;
    while (pico != BOARD_CHIP_PICO) {
        /* Over and over, for a monitor opened later; USB stays up, so the right image can be flashed */
        printf("stopped, no pin touched: this is the image of %s, and the chip is not a%s ESP32-S3%s. "
               "Flash the image of this board.\n",
               BOARD_NAME, BOARD_CHIP_PICO ? "n" : " QFN56", BOARD_CHIP_PICO ? "-PICO" : "");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

#ifdef BOARD_POWER_HOLD
    gpio_set_level(BOARD_POWER_HOLD, 1); /* before it becomes an output: the pin never goes low */
    const gpio_config_t power = {.pin_bit_mask = 1ULL << BOARD_POWER_HOLD, .mode = GPIO_MODE_OUTPUT};
    ESP_ERROR_CHECK(gpio_config(&power));
#endif
#ifdef BOARD_BUTTON
    const gpio_config_t input = {
        .pin_bit_mask = 1ULL << BOARD_BUTTON, .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE};
    gpio_config(&input);
#endif
}

bool board_button_pressed(void)
{
#ifdef BOARD_BUTTON
    return gpio_get_level(BOARD_BUTTON) == 0;
#else
    return false;
#endif
}

#ifdef BOARD_I2C_SDA
/* Drives a pin of the bus by hand, open drain, then waits half a clock (100 kHz) */
static void i2c_pin(int pin, int level)
{
    gpio_set_level(pin, level);
    esp_rom_delay_us(5);
}

/*
 * A chip reset in the middle of a read leaves the device that was answering with SDA held low, which blocks the whole
 * bus: the motion sensor and the backlight. Nine clocks take it to the end of its byte and to the acknowledge, which
 * the released SDA refuses; a STOP then ends the transfer. All nine: a high SDA may be a 1 of the byte. A write cut
 * short gets 0xFF instead, which the drivers write over at boot. By hand, before the driver takes the pins: on the
 * ESP32-S3 the driver's bus clear (i2c_master_bus_reset) does not wait for its end.
 */
static void i2c_free_bus(void)
{
    gpio_set_level(BOARD_I2C_SCL, 1);
    gpio_set_level(BOARD_I2C_SDA, 1);
    const gpio_config_t pins = {.pin_bit_mask = (1ULL << BOARD_I2C_SDA) | (1ULL << BOARD_I2C_SCL),
                                .mode = GPIO_MODE_OUTPUT_OD,
                                .pull_up_en = GPIO_PULLUP_ENABLE};
    gpio_config(&pins);
    for (int clock = 0; clock < 9; clock++) {
        i2c_pin(BOARD_I2C_SCL, 0);
        i2c_pin(BOARD_I2C_SCL, 1);
    }
    i2c_pin(BOARD_I2C_SCL, 0); /* STOP: SDA rises while SCL is high */
    i2c_pin(BOARD_I2C_SDA, 0);
    i2c_pin(BOARD_I2C_SCL, 1);
    i2c_pin(BOARD_I2C_SDA, 1);
    gpio_reset_pin(BOARD_I2C_SDA); /* gpio_config reserved them: the driver reserves them again */
    gpio_reset_pin(BOARD_I2C_SCL);
}

i2c_master_bus_handle_t board_i2c(void)
{
    static i2c_master_bus_handle_t bus;
    if (bus)
        return bus;
    const i2c_master_bus_config_t config = {
        .i2c_port = -1,
        .sda_io_num = BOARD_I2C_SDA,
        .scl_io_num = BOARD_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_free_bus();
    if (i2c_new_master_bus(&config, &bus) != ESP_OK) {
        printf("cannot start the I2C bus\n");
        bus = NULL;
    }
    return bus;
}

/* Every device of the bus runs at the speed of the slowest: one speed for all, 400 kHz, which all of them take */
#define I2C_HZ 400000

i2c_master_dev_handle_t board_i2c_device(uint16_t address)
{
    const i2c_master_bus_handle_t bus = board_i2c();
    const i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = address, .scl_speed_hz = I2C_HZ};
    i2c_master_dev_handle_t device = NULL;
    if (!bus || i2c_master_bus_add_device(bus, &config, &device) != ESP_OK)
        return NULL;
    return device;
}
#endif
