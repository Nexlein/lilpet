/*
 * Small LCD of the M5Stack AtomS3R: 128x128 over SPI, with a ST7735S or a GC9107 depending on the batch (M5Stack
 * changed it in 2026), told apart by their ID. Init sequences, offsets, orientation and color inversion come from
 * M5Stack's library (M5GFX, src/M5GFX.cpp, board_M5AtomS3R, and its panel classes), like the board's pins and backlight
 * (boards/atoms3r/board_config.h, board.c). No TE signal: display_paced.c paces the frames.
 */
#include "board.h"
#include "board_config.h"
#include "display.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_io.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>

#define SPI_HZ (40 * 1000 * 1000) /* M5GFX's write clock */
#define ID_HZ  (1 * 1000 * 1000)  /* slow and safe, for the one ID read */

#define CMD_NOP          0x00u
#define CMD_READ_ID      0x04u
#define CMD_INVERT_OFF   0x20u
#define CMD_INVERT_ON    0x21u
#define CMD_COLUMNS      0x2Au
#define CMD_ROWS         0x2Bu
#define CMD_WRITE_RAM    0x2Cu
#define CMD_ORIENTATION  0x36u
#define CMD_PIXEL_FORMAT 0x3Au

typedef struct {
    uint8_t cmd;
    uint8_t len;
    uint16_t delay_ms; /* after the command */
    uint8_t params[16];
} init_step;

/* Vendor init sequences of M5GFX (Panel_ST7735S in Panel_ST7735.hpp, Panel_GC9107 in Panel_GC9A01.hpp) */
static const init_step ST7735S_INIT[] = {
    {0x01u, 0, 150, {0}}, /* software reset */
    {0x11u, 0, 500, {0}}, /* sleep out */
    {0xB1u, 3, 0, {0x01u, 0x2Cu, 0x2Du}},
    {0xB2u, 3, 0, {0x01u, 0x2Cu, 0x2Du}},
    {0xB3u, 6, 0, {0x01u, 0x2Cu, 0x2Du, 0x01u, 0x2Cu, 0x2Du}},
    {0xB4u, 1, 0, {0x07u}},
    {0xC0u, 3, 0, {0xA2u, 0x02u, 0x84u}},
    {0xC1u, 1, 0, {0xC5u}},
    {0xC2u, 2, 0, {0x0Au, 0x00u}},
    {0xC3u, 2, 0, {0x8Au, 0x2Au}},
    {0xC4u, 2, 0, {0x8Au, 0xEEu}},
    {0xC5u, 1, 0, {0x0Eu}},
    {0xE0u,
     16,
     0,
     {0x02u, 0x1Cu, 0x07u, 0x12u, 0x37u, 0x32u, 0x29u, 0x2Du, 0x29u, 0x25u, 0x2Bu, 0x39u, 0x00u, 0x01u, 0x03u, 0x10u}},
    {0xE1u,
     16,
     0,
     {0x03u, 0x1Du, 0x07u, 0x06u, 0x2Eu, 0x2Cu, 0x29u, 0x2Du, 0x2Eu, 0x2Eu, 0x37u, 0x3Fu, 0x00u, 0x00u, 0x02u, 0x10u}},
    {0x13u, 0, 10, {0}},  /* normal mode */
    {0x29u, 0, 100, {0}}, /* display on */
};
static const init_step GC9107_INIT[] = {
    {0xFEu, 0, 5, {0}}, /* unlock the vendor registers */
    {0xEFu, 0, 5, {0}},
    {0xB0u, 1, 0, {0xC0u}},
    {0xB2u, 1, 0, {0x2Fu}},
    {0xB3u, 1, 0, {0x03u}},
    {0xB6u, 1, 0, {0x19u}},
    {0xB7u, 1, 0, {0x01u}},
    {0xACu, 1, 0, {0xCBu}},
    {0xABu, 1, 0, {0x0Eu}},
    {0xB4u, 1, 0, {0x04u}},
    {0xA8u, 1, 0, {0x19u}},
    {0xB8u, 1, 0, {0x08u}},
    {0xE8u, 1, 0, {0x24u}},
    {0xE9u, 1, 0, {0x48u}},
    {0xEAu, 1, 0, {0x22u}},
    {0xC6u, 1, 0, {0x30u}},
    {0xC7u, 1, 0, {0x18u}},
    {0xF0u, 14, 0, {0x01u, 0x2Bu, 0x23u, 0x3Cu, 0xB7u, 0x12u, 0x17u, 0x60u, 0x00u, 0x06u, 0x0Cu, 0x17u, 0x12u, 0x1Fu}},
    {0xF1u, 14, 0, {0x05u, 0x2Eu, 0x2Du, 0x44u, 0xD6u, 0x15u, 0x17u, 0xA0u, 0x02u, 0x0Du, 0x0Du, 0x1Au, 0x18u, 0x1Fu}},
    {0x11u, 0, 120, {0}}, /* sleep out */
    {0x29u, 0, 0, {0}},   /* display on */
};

typedef struct {
    const char *name;
    int column, row;     /* where the visible 128x128 starts in the controller's memory */
    uint8_t orientation; /* MADCTL, BGR bit included */
    bool invert;
    const init_step *init;
    size_t steps;
} panel;

/*
 * Offsets and MADCTL as M5GFX's Panel_LCD::setRotation computes them at rotation 0. ST7735S: 128x128 visible in a
 * 132x132 memory, offset_rotation 2 (MY | MX | ML | MH, so offset_x 2 and offset_y 1 count from the far side: column
 * 132 - 128 - 2, row 132 - 128 - 1), inverted colors.
 */
#define STEPS(list) (list), sizeof(list) / sizeof((list)[0])
static const panel ST7735S = {"AtomS3R LCD (ST7735S)", 2, 3, 0xDCu, true, STEPS(ST7735S_INIT)};
/* GC9107: 128x128 visible in a 128x160 memory, from row 32 */
static const panel GC9107 = {"AtomS3R LCD (GC9107)", 0, 32, 0x08u, false, STEPS(GC9107_INIT)};

static const panel *chip = &GC9107;
static esp_lcd_panel_io_handle_t io;

static void send(uint8_t cmd, const uint8_t *params, size_t len)
{
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io, cmd, params, len));
}

/*
 * Reads the controller ID (command 0x04) on the data wire, at a low clock. The answer may start after a dummy bit or
 * byte, so every alignment of the bits read is tried against the IDs M5GFX knows.
 */
static const panel *detect(char *raw, size_t raw_size)
{
    spi_device_handle_t dev;
    const spi_device_interface_config_t config = {
        .clock_speed_hz = ID_HZ,
        .spics_io_num = BOARD_LCD_CS,
        .queue_size = 1,
        .flags = SPI_DEVICE_3WIRE | SPI_DEVICE_HALFDUPLEX,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST, &config, &dev));
    ESP_ERROR_CHECK(spi_device_acquire_bus(dev, portMAX_DELAY));
    gpio_set_level(BOARD_LCD_DC, 0);
    spi_transaction_t command = {
        .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_CS_KEEP_ACTIVE, .length = 8, .tx_data = {CMD_READ_ID}};
    ESP_ERROR_CHECK(spi_device_polling_transmit(dev, &command));
    gpio_set_level(BOARD_LCD_DC, 1);
    uint8_t answer[6] = {0};
    spi_transaction_t data = {.rxlength = 8 * sizeof answer, .rx_buffer = answer};
    ESP_ERROR_CHECK(spi_device_polling_transmit(dev, &data));
    spi_device_release_bus(dev);
    ESP_ERROR_CHECK(spi_bus_remove_device(dev));

    snprintf(raw, raw_size, "%02X %02X %02X %02X %02X %02X", answer[0], answer[1], answer[2], answer[3], answer[4],
             answer[5]);
    uint64_t bits = 0;
    for (size_t i = 0; i < sizeof answer; i++)
        bits = bits << 8u | answer[i];
    for (unsigned shift = 0; shift <= 16u; shift++) {
        const uint32_t word = (uint32_t)(bits >> (16u - shift)); /* 4 bytes as they came, first byte highest */
        const uint32_t id = __builtin_bswap32(word);             /* first byte lowest, as M5GFX compares them */
        if ((id & 0xFFFFu) == 0x7683u || (id & 0xFFFFu) == 0x897Cu)
            return &ST7735S;
        if ((id & 0xFFFFFFu) == 0x079100u)
            return &GC9107;
    }
    return NULL;
}

const char *display_init(size_t max_band_bytes)
{
    const gpio_config_t outputs = {.pin_bit_mask = 1ULL << BOARD_LCD_RESET | 1ULL << BOARD_LCD_DC,
                                   .mode = GPIO_MODE_OUTPUT};
    ESP_ERROR_CHECK(gpio_config(&outputs));
    gpio_set_level(BOARD_LCD_RESET, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(BOARD_LCD_RESET, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    const spi_bus_config_t bus = {
        .sclk_io_num = BOARD_LCD_SCLK,
        .mosi_io_num = BOARD_LCD_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = (int)max_band_bytes,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &bus, SPI_DMA_CH_AUTO));
    char raw[24];
    const panel *found = detect(raw, sizeof raw);
    if (found)
        chip = found;
    else
        printf("display ID not recognized (%s): trying the GC9107 settings\n", raw);

    const esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = BOARD_LCD_CS,
        .dc_gpio_num = BOARD_LCD_DC,
        .spi_mode = 0,
        .pclk_hz = SPI_HZ,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI3_HOST, &io_config, &io));

    /* In M5GFX's order: vendor sequence (ends with display on), inversion, pixel format, orientation */
    for (size_t i = 0; i < chip->steps; i++) {
        const init_step *step = &chip->init[i];
        send(step->cmd, step->len ? step->params : NULL, step->len);
        if (step->delay_ms)
            vTaskDelay(pdMS_TO_TICKS(step->delay_ms));
    }
    send(chip->invert ? CMD_INVERT_ON : CMD_INVERT_OFF, NULL, 0);
    send(CMD_PIXEL_FORMAT, (const uint8_t[]){0x55u}, 1); /* 16 bits per pixel, RGB565 */
    send(CMD_ORIENTATION, &chip->orientation, 1);
    board_backlight_on();
    return chip->name;
}

void display_draw(int x0, int y0, int x1, int y1, const uint16_t *pixels)
{
    const size_t count = (size_t)(x1 - x0) * (size_t)(y1 - y0);
    const int c0 = x0 + chip->column;
    const int c1 = x1 - 1 + chip->column;
    const int r0 = y0 + chip->row;
    const int r1 = y1 - 1 + chip->row;
    const uint8_t columns[4] = {(uint8_t)(c0 >> 8), (uint8_t)c0, (uint8_t)(c1 >> 8), (uint8_t)c1};
    const uint8_t rows[4] = {(uint8_t)(r0 >> 8), (uint8_t)r0, (uint8_t)(r1 >> 8), (uint8_t)r1};
    send(CMD_COLUMNS, columns, sizeof columns); /* waits until the previous band is sent */
    send(CMD_ROWS, rows, sizeof rows);
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_color(io, CMD_WRITE_RAM, pixels, count * sizeof pixels[0]));
}

void display_flush(void)
{
    send(CMD_NOP, NULL, 0); /* a command waits for the queued bands to be sent */
}
