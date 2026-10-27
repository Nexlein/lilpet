/* CO5300 AMOLED over QSPI with ESP-IDF's esp_lcd panel IO (see display.h for the source of the values) */
#include "display.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_io.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define PIN_CS    GPIO_NUM_10
#define PIN_CLK   GPIO_NUM_11
#define PIN_D0    GPIO_NUM_12
#define PIN_D1    GPIO_NUM_13
#define PIN_D2    GPIO_NUM_14
#define PIN_D3    GPIO_NUM_15
#define PIN_RESET GPIO_NUM_8
#define PIN_TE    GPIO_NUM_9
#define PIN_POWER GPIO_NUM_18 /* keeps the board on when running on battery, once the power button is released */

#define QSPI_HZ       (40 * 1000 * 1000) /* Waveshare's value; the controller may take 80 MHz, to try on the board */
#define COLUMN_OFFSET 6                  /* first visible column of the panel */

/* QSPI framing: an opcode in the top byte, then the controller command in the middle byte */
#define OPCODE_WRITE_CMD    0x02u
#define OPCODE_WRITE_PIXELS 0x32u

#define CMD_NOP          0x00u
#define CMD_SLEEP_OUT    0x11u
#define CMD_DISPLAY_ON   0x29u
#define CMD_COLUMNS      0x2Au
#define CMD_ROWS         0x2Bu
#define CMD_WRITE_RAM    0x2Cu
#define CMD_TE_ON        0x35u
#define CMD_ORIENTATION  0x36u
#define CMD_PIXEL_FORMAT 0x3Au
#define CMD_BRIGHTNESS   0x51u

typedef struct {
    uint8_t cmd;
    uint8_t len;
    uint8_t params[1];
    uint16_t delay_ms;
} init_cmd;

/* From Waveshare's demo; the full-screen window it also sets is set by every display_draw here */
static const init_cmd INIT[] = {
    {0xFEu, 1, {0x00u}, 0},            /* command page 0 */
    {0xC4u, 1, {0x80u}, 0},            /* vendor command, value from the demo */
    {CMD_PIXEL_FORMAT, 1, {0x55u}, 0}, /* 16 bits per pixel, RGB565 */
    {CMD_TE_ON, 1, {0x00u}, 0},        /* TE signal at each vertical blank */
    {0x53u, 1, {0x20u}, 0},            /* brightness control on */
    {CMD_BRIGHTNESS, 1, {0xFFu}, 0},   /* brightness */
    {0x63u, 1, {0xFFu}, 0},            /* vendor command, value from the demo */
    {CMD_SLEEP_OUT, 0, {0}, 100},      /* leave sleep, then wait */
    {CMD_DISPLAY_ON, 0, {0}, 0},       /* display on */
    {CMD_ORIENTATION, 1, {0xC0u}, 0},  /* rows and columns reversed: 180 degrees, as the panel is mounted */
};

static esp_lcd_panel_io_handle_t io;
static SemaphoreHandle_t refresh;

static void send(uint32_t cmd, const uint8_t *params, size_t len)
{
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io, (int)(OPCODE_WRITE_CMD << 24 | cmd << 8), params, len));
}

static void IRAM_ATTR on_refresh(void *arg)
{
    (void)arg;
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(refresh, &woken);
    if (woken)
        portYIELD_FROM_ISR();
}

bool display_init(size_t max_band_bytes)
{
    const gpio_config_t outputs = {.pin_bit_mask = 1ULL << PIN_POWER | 1ULL << PIN_RESET, .mode = GPIO_MODE_OUTPUT};
    ESP_ERROR_CHECK(gpio_config(&outputs));
    gpio_set_level(PIN_POWER, 1);

    const spi_bus_config_t bus = {
        .sclk_io_num = PIN_CLK,
        .data0_io_num = PIN_D0,
        .data1_io_num = PIN_D1,
        .data2_io_num = PIN_D2,
        .data3_io_num = PIN_D3,
        .max_transfer_sz = (int)max_band_bytes,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
    const esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = PIN_CS,
        .dc_gpio_num = GPIO_NUM_NC,
        .spi_mode = 0,
        .pclk_hz = QSPI_HZ,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 32,
        .lcd_param_bits = 8,
        .flags.quad_mode = true,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io));

    /* TE: rising edge at the start of each refresh */
    refresh = xSemaphoreCreateBinary();
    const gpio_config_t te = {.pin_bit_mask = 1ULL << PIN_TE, .mode = GPIO_MODE_INPUT, .intr_type = GPIO_INTR_POSEDGE};
    ESP_ERROR_CHECK(gpio_config(&te));
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_TE, on_refresh, NULL));

    /* Hardware reset, timings of Espressif's driver for this controller family (esp_lcd_sh8601) */
    gpio_set_level(PIN_RESET, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_RESET, 1);
    vTaskDelay(pdMS_TO_TICKS(150));

    for (size_t i = 0; i < sizeof INIT / sizeof INIT[0]; i++) {
        send(INIT[i].cmd, INIT[i].len ? INIT[i].params : NULL, INIT[i].len);
        if (INIT[i].delay_ms)
            vTaskDelay(pdMS_TO_TICKS(INIT[i].delay_ms));
    }
    return display_wait_refresh(100);
}

bool display_wait_refresh(int timeout_ms)
{
    xSemaphoreTake(refresh, 0); /* forget an older edge: wait for the next refresh */
    return xSemaphoreTake(refresh, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void display_draw(int x0, int y0, int x1, int y1, uint16_t *pixels)
{
    const size_t count = (size_t)(x1 - x0) * (size_t)(y1 - y0);
    for (size_t i = 0; i < count; i++)
        pixels[i] = __builtin_bswap16(pixels[i]); /* the controller takes the high byte first */

    const int c0 = x0 + COLUMN_OFFSET;
    const int c1 = x1 - 1 + COLUMN_OFFSET;
    const uint8_t columns[4] = {(uint8_t)(c0 >> 8), (uint8_t)c0, (uint8_t)(c1 >> 8), (uint8_t)c1};
    const uint8_t rows[4] = {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)((y1 - 1) >> 8), (uint8_t)(y1 - 1)};
    send(CMD_COLUMNS, columns, sizeof columns); /* waits until the previous band is sent */
    send(CMD_ROWS, rows, sizeof rows);
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_color(io, (int)(OPCODE_WRITE_PIXELS << 24 | CMD_WRITE_RAM << 8), pixels,
                                              count * sizeof pixels[0]));
}

void display_flush(void)
{
    send(CMD_NOP, NULL, 0); /* a command waits for the queued bands to be sent */
}

void display_brightness(uint8_t level)
{
    send(CMD_BRIGHTNESS, &level, 1);
}
