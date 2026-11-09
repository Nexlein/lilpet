/*
 * Round AMOLED of the Waveshare ESP32-S3-Touch-AMOLED-1.32: 466x466, CO5300 controller, QSPI, with ESP-IDF's esp_lcd
 * panel IO. Pins, init sequence, 180-degree rotation and 6-column offset come from Waveshare's ESP-IDF demo
 * (07_LVGL_V9_Test, components/lvgl_port_bsp); to check on the board: orientation, offset, colors and tearing.
 * Also the driver of boards without a display: no TE pin in their board_config.h, so display_paced.c paces the frames,
 * and the bands go out on unconnected pins. The pins and the offset come from the board
 * (boards/<board>/board_config.h).
 */
#include "board_config.h"
#include "dcs.h"
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

#define QSPI_HZ       (40 * 1000 * 1000) /* Waveshare's value; the controller may take 80 MHz, to try on the board */
#define TE_TIMEOUT_MS 50                 /* a frame still starts if the signal is missed */

/* QSPI framing: an opcode in the top byte, then the controller command in the middle byte */
#define OPCODE_WRITE_CMD    0x02u
#define OPCODE_WRITE_PIXELS 0x32u

typedef struct {
    uint8_t cmd;
    uint8_t len;
    uint8_t params[1];
    uint16_t delay_ms;
} init_cmd;

/* From Waveshare's demo, but for display on, sent after the first frame (display_flush); the full-screen window it
 * also sets is set by every display_draw here */
static const init_cmd INIT[] = {
    {0xFEu, 1, {0x00u}, 0},            /* command page 0 */
    {0xC4u, 1, {0x80u}, 0},            /* vendor command, value from the demo */
    {DCS_PIXEL_FORMAT, 1, {0x55u}, 0}, /* 16 bits per pixel, RGB565 */
    {DCS_TE_ON, 1, {0x00u}, 0},        /* TE signal at each vertical blank */
    {DCS_CONTROL, 1, {0x20u}, 0},      /* brightness control on */
    {DCS_BRIGHTNESS, 1, {0xFFu}, 0},   /* brightness */
    {0x63u, 1, {0xFFu}, 0},            /* vendor command, value from the demo */
    {DCS_SLEEP_OUT, 0, {0}, 100},      /* leave sleep, then wait */
    {DCS_ORIENTATION, 1, {0xC0u}, 0},  /* rows and columns reversed: 180 degrees, as the panel is mounted */
};

static esp_lcd_panel_io_handle_t io;
/* The panel's memory holds noise after a reset: it turns on once the first frame is in (display_flush) */
static bool panel_on;

static void send(uint32_t cmd, const uint8_t *params, size_t len)
{
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io, (int)(OPCODE_WRITE_CMD << 24 | cmd << 8), params, len));
}

#ifdef BOARD_LCD_TE
static SemaphoreHandle_t refresh;

static void IRAM_ATTR on_refresh(void *arg)
{
    (void)arg;
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(refresh, &woken);
    if (woken)
        portYIELD_FROM_ISR();
}

static bool wait_refresh(int timeout_ms)
{
    xSemaphoreTake(refresh, 0); /* forget an older edge: wait for the next refresh */
    return xSemaphoreTake(refresh, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void display_wait_frame(void)
{
    wait_refresh(TE_TIMEOUT_MS);
}
#endif

const char *display_init(size_t max_band_bytes)
{
    const gpio_config_t outputs = {.pin_bit_mask = 1ULL << BOARD_LCD_RESET, .mode = GPIO_MODE_OUTPUT};
    ESP_ERROR_CHECK(gpio_config(&outputs));

    const spi_bus_config_t bus = {
        .sclk_io_num = BOARD_LCD_CLK,
        .data0_io_num = BOARD_LCD_D0,
        .data1_io_num = BOARD_LCD_D1,
        .data2_io_num = BOARD_LCD_D2,
        .data3_io_num = BOARD_LCD_D3,
        .max_transfer_sz = (int)max_band_bytes,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
    const esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = BOARD_LCD_CS,
        .dc_gpio_num = GPIO_NUM_NC,
        .spi_mode = 0,
        .pclk_hz = QSPI_HZ,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 32,
        .lcd_param_bits = 8,
        .flags.quad_mode = true,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io));

#ifdef BOARD_LCD_TE
    /* TE: rising edge at the start of each refresh */
    refresh = xSemaphoreCreateBinary();
    const gpio_config_t te = {
        .pin_bit_mask = 1ULL << BOARD_LCD_TE, .mode = GPIO_MODE_INPUT, .intr_type = GPIO_INTR_POSEDGE};
    ESP_ERROR_CHECK(gpio_config(&te));
    const esp_err_t isr = gpio_install_isr_service(0); /* another module (touch, IMU) may have installed it */
    if (isr != ESP_ERR_INVALID_STATE)
        ESP_ERROR_CHECK(isr);
    ESP_ERROR_CHECK(gpio_isr_handler_add(BOARD_LCD_TE, on_refresh, NULL));
#endif

    /* Hardware reset, timings of Espressif's driver for this controller family (esp_lcd_sh8601) */
    gpio_set_level(BOARD_LCD_RESET, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(BOARD_LCD_RESET, 1);
    vTaskDelay(pdMS_TO_TICKS(150));

    for (size_t i = 0; i < sizeof INIT / sizeof INIT[0]; i++) {
        send(INIT[i].cmd, INIT[i].len ? INIT[i].params : NULL, INIT[i].len);
        if (INIT[i].delay_ms)
            vTaskDelay(pdMS_TO_TICKS(INIT[i].delay_ms));
    }
#ifdef BOARD_LCD_TE
    return wait_refresh(100) ? "CO5300 AMOLED (TE signal)" : "CO5300 AMOLED (no TE signal: is the panel there?)";
#else
    return "CO5300 pins (no TE pin, frames paced)";
#endif
}

void display_draw(int x0, int y0, int x1, int y1, const uint16_t *pixels)
{
    const size_t count = (size_t)(x1 - x0) * (size_t)(y1 - y0);
    uint8_t columns[4];
    uint8_t rows[4];
    dcs_range(x0 + BOARD_LCD_COLUMN_OFFSET, x1 - 1 + BOARD_LCD_COLUMN_OFFSET, columns);
    dcs_range(y0, y1 - 1, rows);
    send(DCS_COLUMNS, columns, sizeof columns); /* waits until the previous band is sent */
    send(DCS_ROWS, rows, sizeof rows);
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_color(io, (int)(OPCODE_WRITE_PIXELS << 24 | DCS_WRITE_RAM << 8), pixels,
                                              count * sizeof pixels[0]));
}

void display_flush(void)
{
    send(DCS_NOP, NULL, 0); /* a command waits for the queued bands to be sent */
    if (!panel_on) {
        send(DCS_DISPLAY_ON, NULL, 0);
        panel_on = true;
    }
}
