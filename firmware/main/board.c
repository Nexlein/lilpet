#include "board.h"

#include "board_config.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>

void board_init(void)
{
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
    if (i2c_new_master_bus(&config, &bus) != ESP_OK) {
        printf("cannot start the I2C bus\n");
        bus = NULL;
    }
    return bus;
}
#endif

#ifdef BOARD_BACKLIGHT_ADDR
#define BACKLIGHT_HZ 400000
#define BRIGHTNESS   160u /* out of 255 */

static i2c_master_dev_handle_t backlight;

static void set_backlight(uint8_t reg, uint8_t value)
{
    const uint8_t bytes[2] = {reg, value};
    if (backlight && i2c_master_transmit(backlight, bytes, sizeof bytes, 50) != ESP_OK)
        printf("backlight: no answer from the driver at I2C 0x%02X\n", BOARD_BACKLIGHT_ADDR);
}

/* The backlight goes through a LED driver on I2C, not a GPIO: without it the screen draws but stays dark */
void board_backlight_on(void)
{
    const i2c_master_bus_handle_t bus = board_i2c(); /* shared with the motion sensor */
    const i2c_device_config_t device = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = BOARD_BACKLIGHT_ADDR, .scl_speed_hz = BACKLIGHT_HZ};
    if (!bus || i2c_master_bus_add_device(bus, &device, &backlight) != ESP_OK) {
        printf("backlight: cannot reach its driver on the I2C bus\n");
        return;
    }
    /* Register writes of M5GFX's Light_M5StackAtomS3R */
    set_backlight(0x00u, 0x40u); /* chip enabled */
    vTaskDelay(pdMS_TO_TICKS(1));
    set_backlight(0x08u, 0x01u); /* internal clock */
    set_backlight(0x70u, 0x00u); /* outputs driven by the brightness registers */
    set_backlight(0x0Eu, BRIGHTNESS);
}
#endif
