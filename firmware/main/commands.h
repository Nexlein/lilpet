/* Commands from the PC app over the USB serial port (protocol in notes/app.md):
 *   "> hello"       ->  "< lilpet fw=<version> engine=<version> board=<board> camera=<0|1> motion=<0|1>"
 *   "> look <text>" ->  "< ok" once the look is saved, or "< err <reason>"
 *   "> look?"       ->  "< look <text>", the current look
 *   "> imu?"        ->  "< imu <x> <y> <z>", gravity in g in screen coordinates (imu.h), or "< err no motion sensor"
 * Other lines are ignored; the timing reports share the port. */
#ifndef LILPET_COMMANDS_H
#define LILPET_COMMANDS_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lilpet/lilpet.h"

#include <stdbool.h>

/* Starts the task that reads commands. Each new look goes into `looks`, a one-item queue the drawing loop reads;
 * `current` is the look shown at startup, `motion` tells hello that a motion sensor answers. */
void commands_start(QueueHandle_t looks, const lp_look *current, bool motion);

#endif
