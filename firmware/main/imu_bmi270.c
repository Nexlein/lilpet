/*
 * Motion sensor of the AtomS3R: a Bosch BMI270 on the internal I2C bus, accelerometer only, read when asked. Startup
 * follows the datasheet (BST-BMI270-DS000, section 4.4): soft reset, power saving off, Bosch's configuration file
 * uploaded in chunks, then the accelerometer on at its defaults (100 Hz, +-8 g).
 */
#include "imu.h"

#include "../third_party/bmi270/bmi270_config.h"
#include "board.h"
#include "board_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "imu_axes.h"

#include <stdio.h>
#include <string.h>

#define REG_CHIP_ID         0x00u
#define REG_STATUS          0x03u
#define REG_ACC_X           0x0Cu /* x, y, z: little-endian int16 */
#define REG_INTERNAL_STATUS 0x21u
#define REG_INIT_CTRL       0x59u
#define REG_INIT_ADDR       0x5Bu /* word offset in the configuration: low 4 bits, then the rest in 0x5C */
#define REG_INIT_DATA       0x5Eu
#define REG_PWR_CONF        0x7Cu
#define REG_PWR_CTRL        0x7Du
#define REG_CMD             0x7Eu

#define CHIP_ID    0x24u
#define SOFT_RESET 0xB6u
#define INIT_OK    0x01u
#define ACC_ON     0x04u
#define DATA_READY 0x80u   /* accelerometer bit of REG_STATUS */
#define LSB_PER_G  4096.0f /* at +-8 g, the default range */
#define CHUNK      64      /* bytes of the configuration per write */

/* Sensor axes to screen axes: how the sensor is mounted on the board */
static const imu_axis SCREEN_AXES[3] = BOARD_IMU_AXES;

static i2c_master_dev_handle_t dev;

static bool write_reg(uint8_t reg, const uint8_t *data, size_t len)
{
    uint8_t bytes[1 + CHUNK];
    if (len > CHUNK)
        return false;
    bytes[0] = reg;
    memcpy(bytes + 1, data, len);
    return i2c_master_transmit(dev, bytes, len + 1, BOARD_I2C_TIMEOUT_MS) == ESP_OK;
}

static bool write_reg8(uint8_t reg, uint8_t value)
{
    return write_reg(reg, &value, 1);
}

static bool read_reg(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(dev, &reg, 1, data, len, BOARD_I2C_TIMEOUT_MS) == ESP_OK;
}

static bool upload_config(void)
{
    for (size_t at = 0; at < BMI270_CONFIG_SIZE; at += CHUNK) {
        const uint8_t offset[2] = {(uint8_t)((at / 2u) & 0x0Fu), (uint8_t)(at / 32u)};
        if (!write_reg(REG_INIT_ADDR, offset, sizeof offset) || !write_reg(REG_INIT_DATA, BMI270_CONFIG + at, CHUNK))
            return false;
    }
    return true;
}

static bool start(void)
{
    uint8_t id = 0;
    if (!read_reg(REG_CHIP_ID, &id, 1) || id != CHIP_ID) {
        printf("motion sensor: not a BMI270 (chip ID 0x%02X)\n", id);
        return false;
    }
    write_reg8(REG_CMD, SOFT_RESET);
    vTaskDelay(pdMS_TO_TICKS(3));
    /* Writes need 450 us between them until power saving is off; a delay of n ticks can end up to one tick short */
    if (!write_reg8(REG_PWR_CONF, 0x00u))
        return false;
    vTaskDelay(pdMS_TO_TICKS(2));
    if (!write_reg8(REG_INIT_CTRL, 0x00u) || !upload_config() || !write_reg8(REG_INIT_CTRL, 0x01u))
        return false;
    uint8_t status = 0;
    for (int i = 0; i < 40 && (status & 0x0Fu) != INIT_OK; i++) { /* at most 20 ms, says the datasheet */
        vTaskDelay(pdMS_TO_TICKS(1));
        if (!read_reg(REG_INTERNAL_STATUS, &status, 1))
            status = 0;
    }
    if ((status & 0x0Fu) != INIT_OK) {
        printf("motion sensor: BMI270 refused its configuration (status 0x%02X)\n", status);
        return false;
    }
    if (!write_reg8(REG_PWR_CTRL, ACC_ON))
        return false;
    /* Once, on a board just plugged in, the accelerometer stayed off: wait for its first sample to be sure */
    uint8_t ready = 0;
    for (int i = 0; i < 20 && !(ready & DATA_READY); i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
        if (!read_reg(REG_STATUS, &ready, 1))
            ready = 0;
    }
    if (!(ready & DATA_READY))
        printf("motion sensor: BMI270 gives no sample\n");
    return ready & DATA_READY;
}

bool imu_init(void)
{
    const i2c_master_bus_handle_t bus = board_i2c();
    if (!bus || i2c_master_probe(bus, BOARD_IMU_ADDR, BOARD_I2C_TIMEOUT_MS) != ESP_OK)
        return false;
    dev = board_i2c_device(BOARD_IMU_ADDR);
    if (!dev)
        return false;
    if (start() || start()) /* a second try from a soft reset */
        return true;
    i2c_master_bus_rm_device(dev);
    dev = NULL;
    return false;
}

bool imu_gravity(float out[3])
{
    uint8_t raw[6];
    if (!dev || !read_reg(REG_ACC_X, raw, sizeof raw))
        return false;
    imu_axes_to_screen(raw, SCREEN_AXES, LSB_PER_G, out);
    return true;
}
