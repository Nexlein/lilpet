/* The battery's voltage, read through the board's divider on an ADC pin (BOARD_BATTERY_ADC in board_config.h): the
 * AtomS3R on M5Stack's Atomic Battery Base. A board without one answers -1. */
#ifndef LILPET_BATTERY_H
#define LILPET_BATTERY_H

/* Prepares the ADC; call once at startup */
void battery_init(void);
/* Millivolts at the battery, or -1: no battery pin on this board, none plugged, or the ADC failed */
int battery_millivolts(void);

#endif
