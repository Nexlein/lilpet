/*
 * The battery on a board whose AXP2101 power chip charges and measures it: the Waveshare 1.75". At every start, the
 * charge current is set to BOARD_CHARGE_MA (board_config.h): the chip forgets it once it has neither USB nor battery,
 * and the board's factory firmware charges at 400 mA (docs/materiel.md). The boot log tells whether it is set: until it
 * says so, the battery stays unplugged. Not tried on the board yet.
 */
#include "axp2101.h"
#include "battery.h"
#include "board.h"
#include "board_config.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static i2c_master_dev_handle_t pmu; /* NULL: the chip did not answer */

static bool write_reg(uint8_t reg, uint8_t value)
{
    const uint8_t bytes[2] = {reg, value};
    return i2c_master_transmit(pmu, bytes, sizeof bytes, BOARD_I2C_TIMEOUT_MS) == ESP_OK;
}

static bool read_reg(uint8_t reg, uint8_t *out)
{
    return i2c_master_transmit_receive(pmu, &reg, 1, out, 1, BOARD_I2C_TIMEOUT_MS) == ESP_OK;
}

/* Sets the charge current, then the battery's detection and measurement, and reads the charge current back */
static bool set_up(void)
{
    uint8_t charge, detection, adc;
    return read_reg(AXP2101_ICC_CHG, &charge) &&
           write_reg(AXP2101_ICC_CHG, axp2101_charge_register(charge, BOARD_CHARGE_MA)) &&
           read_reg(AXP2101_BAT_DET, &detection) && write_reg(AXP2101_BAT_DET, axp2101_detection_register(detection)) &&
           read_reg(AXP2101_ADC_CTRL, &adc) && write_reg(AXP2101_ADC_CTRL, axp2101_adc_register(adc)) &&
           read_reg(AXP2101_ICC_CHG, &charge) && (charge & 0x1Fu) == axp2101_charge_code(BOARD_CHARGE_MA);
}

void battery_init(void)
{
    pmu = board_i2c_device(AXP2101_ADDR);
    if (pmu && set_up()) {
        printf("battery: AXP2101 charges at %d mA at most\n", BOARD_CHARGE_MA);
        return;
    }
    printf("battery: the AXP2101 does not answer, its charge current is not set: keep the battery unplugged\n");
    pmu = NULL;
}

int battery_millivolts(void)
{
    uint8_t status, high, low, again;
    /* One register at a time, as XPowersLib reads them. A measurement that lands between the two would mix two
     * voltages, up to 256 mV apart: the high byte is read again, and the low one too when it changed. */
    if (!pmu || !read_reg(AXP2101_STATUS1, &status) || !(status & AXP2101_BATTERY_CONNECTED) ||
        !read_reg(AXP2101_VBAT, &high) || !read_reg(AXP2101_VBAT + 1u, &low) || !read_reg(AXP2101_VBAT, &again) ||
        (again != high && !read_reg(AXP2101_VBAT + 1u, &low)))
        return -1;
    return axp2101_millivolts(again, low);
}
