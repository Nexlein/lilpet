/*
 * The AXP2101 power chip's registers and their values, free of ESP-IDF so that the host tests check them
 * (firmware/tests): it charges and measures the battery on the Waveshare 1.75" (battery_axp2101.c). Registers from
 * XPowersLib (AXP2101Constants.h, XPowersAXP2101.tpp), the library Waveshare's demos use; the charge current steps
 * match Zephyr's AXP2101 driver.
 */
#ifndef LILPET_AXP2101_H
#define LILPET_AXP2101_H

#include <stdint.h>

#define AXP2101_ADDR     0x34u /* on the board's I2C bus */
#define AXP2101_STATUS1  0x00u /* bit 3: a battery is connected */
#define AXP2101_ADC_CTRL 0x30u /* bit 0: measure the battery's voltage; bit 1: measure the TS (temperature) pin */
#define AXP2101_VBAT     0x34u /* battery voltage in mV: its 5 high bits here, its 8 low bits in the next register */
#define AXP2101_ICC_CHG  0x62u /* bits 4-0: the constant charge current, axp2101_charge_code */
#define AXP2101_TS_CTRL  0x50u /* the TS pin: whether the charger reads it as the battery's temperature sensor */
#define AXP2101_BAT_DET  0x68u /* bit 0: detect the battery, for the bit of AXP2101_STATUS1 */

#define AXP2101_BATTERY_CONNECTED 0x08u

/* The code for a charge current of at most `ma`: 0 (no charge) below 100 mA, then 4 for 100 mA and 1 more every 25 mA
 * up to 200 mA (8), then 1 more every 100 mA up to 1000 mA (16) */
static inline uint8_t axp2101_charge_code(int ma)
{
    if (ma < 100)
        return 0;
    if (ma <= 200)
        return (uint8_t)(4 + (ma - 100) / 25);
    if (ma > 1000)
        ma = 1000;
    return (uint8_t)(8 + (ma - 200) / 100);
}

/* The charge current register with that code, its other bits kept */
static inline uint8_t axp2101_charge_register(uint8_t was, int ma)
{
    return (uint8_t)((was & 0xE0u) | axp2101_charge_code(ma));
}

/* The TS pin's register with the pin as a fixed input, which the charger ignores. A board without the battery's
 * temperature sensor charges wrong otherwise (Waveshare's demo, examples/esp-idf/01_AXP2101): XPowersLib's
 * disableTSPinMeasure, which it calls at every start, writes this, then axp2101_adc_register */
static inline uint8_t axp2101_ts_register(uint8_t was)
{
    return (uint8_t)((was & 0xF0u) | 0x10u);
}

/* The ADC register with the battery's voltage measured and the TS pin's not */
static inline uint8_t axp2101_adc_register(uint8_t was)
{
    return (uint8_t)((was | 0x01u) & ~0x02u);
}

/* The battery detection register with the detection on: without it, STATUS1 never tells a battery is connected */
static inline uint8_t axp2101_detection_register(uint8_t was)
{
    return (uint8_t)(was | 0x01u);
}

/* The battery's voltage in mV, from the register at AXP2101_VBAT and the next one */
static inline int axp2101_millivolts(uint8_t high, uint8_t low)
{
    return (int)(high & 0x1Fu) << 8 | low;
}

#endif /* LILPET_AXP2101_H */
