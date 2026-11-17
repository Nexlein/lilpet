/* The battery: one .c per way of reading it, picked by board.cmake. battery_adc.c reads a divider on an ADC pin (the
 * AtomS3R on its Atomic Battery Base), battery_axp2101.c the AXP2101 power chip, which also charges it (the Waveshare
 * 1.75"), and battery_none.c is for boards without one. */
#ifndef LILPET_BATTERY_H
#define LILPET_BATTERY_H

/* Prepares the reading, and on a board that charges the battery, sets its charge current; call once at startup, before
 * anything that could take time */
void battery_init(void);
/* Millivolts at the battery, or -1: no way to read it on this board, none plugged, or the reading failed */
int battery_millivolts(void);

#endif
