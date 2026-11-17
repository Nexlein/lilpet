#include "board.h"

#include "board_config.h"
#include "chip.h"
#include "driver/gpio.h"
#include "esp_efuse.h"
#include "esp_efuse_table.h"
#include "esp_err.h"
#include "esp_flash.h"
#include "esp_rom_sys.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/efuse_defs.h"

#include <stdio.h>

void board_init(void)
{
    /* The XIAO's camera outputs sit on the display pins, so nothing touches a pin until the chip matches (chip.h). Two
     * boards with the same chip cannot be told apart: mise run firmware:flash asks the board first for that. */
    uint32_t flash_bytes = 0;
    uint8_t psram_cap = 0;
    (void)esp_flash_get_physical_size(NULL, &flash_bytes); /* a failure reads 0 MB, which no board has */
    (void)esp_efuse_read_field_blob(ESP_EFUSE_PSRAM_CAP, &psram_cap, 3);
    const chip_info found = {.pico = esp_efuse_get_pkg_ver() == EFUSE_PKG_VERSION_ESP32S3PICO,
                             .flash_mb = (int)(flash_bytes >> 20),
                             .psram_mb = chip_psram_mb(psram_cap)};
    const chip_info image = {.pico = BOARD_CHIP_PICO, .flash_mb = BOARD_FLASH_MB, .psram_mb = BOARD_PSRAM_MB};
    while (!chip_same(found, image)) {
        /* Over and over, for a monitor opened later; USB stays up, so the right image can be flashed */
        printf("stopped, no pin touched: this is the image of %s, for an ESP32-S3%s with %d MB of flash and %d MB of "
               "PSRAM, and this chip is an ESP32-S3%s with %d MB and %d MB. Flash the image of this board.\n",
               BOARD_NAME, image.pico ? "-PICO" : " in QFN56", image.flash_mb, image.psram_mb,
               found.pico ? "-PICO" : " in QFN56", found.flash_mb, found.psram_mb);
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

bool board_has_button(void)
{
#ifdef BOARD_BUTTON
    return true;
#else
    return false;
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

/*
 * In light sleep, ESP-IDF lets every pin float, against a reset that a small electrostatic pulse could cause
 * (CONFIG_ESP_SLEEP_GPIO_RESET_WORKAROUND). These keep driving or pulling as they do awake; the button is kept by
 * gpio_wakeup_enable, armed whether it is held or not.
 */
static const int PINS_KEPT_ASLEEP[] = {
#ifdef BOARD_POWER_HOLD
    BOARD_POWER_HOLD, /* or the board would cut its own power */
#endif
#ifdef BOARD_LCD_RESET
    BOARD_LCD_RESET, /* low, the panel would forget its setup */
#endif
#ifdef BOARD_LCD_CS
    BOARD_LCD_CS, /* high, the panel ignores its other lines, left floating */
#endif
#ifdef BOARD_I2C_SDA
    BOARD_I2C_SDA,
    BOARD_I2C_SCL, /* the bus stays idle for its devices: motion sensor, backlight, power chip */
#endif
    -1};

void board_light_sleep(uint64_t us, bool button_held)
{
    for (int i = 0; PINS_KEPT_ASLEEP[i] >= 0; i++)
        ESP_ERROR_CHECK(gpio_sleep_sel_dis(PINS_KEPT_ASLEEP[i]));
#ifdef BOARD_BUTTON
    /* Any pin wakes the ESP32-S3 from light sleep, not only the RTC ones: a press, which is low, or while it is held (a
     * board pressed in a pocket) its release, else each sleep would end at once. The caller then sees the release, and
     * the next press wakes the board. */
    ESP_ERROR_CHECK(gpio_wakeup_enable(BOARD_BUTTON, button_held ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL));
    ESP_ERROR_CHECK(esp_sleep_enable_gpio_wakeup());
#else
    (void)button_held;
#endif
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(us));
    (void)esp_light_sleep_start(); /* refused when a wake-up is already pending: the caller looks again */
#ifdef BOARD_BUTTON
    ESP_ERROR_CHECK(gpio_wakeup_disable(BOARD_BUTTON));
#endif
}

#ifdef BOARD_I2C_SDA
/* Drives a pin of the bus by hand, open drain, then waits half a clock (100 kHz) */
static void i2c_pin(int pin, uint32_t level)
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

bool board_i2c_write(i2c_master_dev_handle_t device, uint8_t reg, const uint8_t *data, size_t len)
{
    /* The register, then the data, in one transfer and with no copy */
    i2c_master_transmit_multi_buffer_info_t parts[] = {{.write_buffer = &reg, .buffer_size = 1},
                                                       {.write_buffer = data, .buffer_size = len}};
    return i2c_master_multi_buffer_transmit(device, parts, 2, BOARD_I2C_TIMEOUT_MS) == ESP_OK;
}

bool board_i2c_read(i2c_master_dev_handle_t device, uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(device, &reg, 1, data, len, BOARD_I2C_TIMEOUT_MS) == ESP_OK;
}
#endif
