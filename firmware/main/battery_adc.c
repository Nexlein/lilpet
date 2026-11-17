/* The battery read through a divider on an ADC pin (BOARD_BATTERY_ADC, BOARD_BATTERY_DIVIDER in board_config.h): the
 * AtomS3R on M5Stack's Atomic Battery Base */
#include "battery.h"

#include "board_config.h"

#define BATTERY_ABSENT_MV 2500 /* below it, no cell: a protected LiPo cuts out before */
#define READINGS          16   /* averaged, for the ADC's noise */

#ifndef BOARD_BATTERY_ADC
#error "battery_adc.c reads BOARD_BATTERY_ADC: a board without one links battery_none.c (board.cmake)"
#endif

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"

#include <stdbool.h>

static struct {
    adc_oneshot_unit_handle_t unit;
    adc_channel_t channel;
    adc_cali_handle_t calibration; /* raw reading to millivolts, from the chip's eFuse calibration */
    bool ready;
} adc;

void battery_init(void)
{
    adc_unit_t unit;
    if (adc_oneshot_io_to_channel(BOARD_BATTERY_ADC, &unit, &adc.channel) != ESP_OK)
        return;
    const adc_oneshot_unit_init_cfg_t unit_config = {.unit_id = unit};
    /* 12 dB: up to about 3.1 V at the pin, above half a full cell (4.2 V) behind the divider */
    const adc_oneshot_chan_cfg_t channel_config = {.atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT};
    const adc_cali_curve_fitting_config_t calibration_config = {
        .unit_id = unit, .chan = adc.channel, .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT};
    adc.ready = adc_oneshot_new_unit(&unit_config, &adc.unit) == ESP_OK &&
                adc_oneshot_config_channel(adc.unit, adc.channel, &channel_config) == ESP_OK &&
                adc_cali_create_scheme_curve_fitting(&calibration_config, &adc.calibration) == ESP_OK;
}

int battery_millivolts(void)
{
    if (!adc.ready)
        return -1;
    int sum = 0;
    for (int i = 0; i < READINGS; i++) {
        int raw;
        if (adc_oneshot_read(adc.unit, adc.channel, &raw) != ESP_OK)
            return -1;
        sum += raw;
    }
    int millivolts;
    if (adc_cali_raw_to_voltage(adc.calibration, sum / READINGS, &millivolts) != ESP_OK)
        return -1;
    millivolts *= BOARD_BATTERY_DIVIDER;
    return millivolts < BATTERY_ABSENT_MV ? -1 : millivolts; /* nothing plugged: the divider pulls the pin down */
}
