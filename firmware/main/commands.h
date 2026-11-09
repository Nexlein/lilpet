/* Commands from the PC app over the USB serial port, one per line (docs/protocole.md, read by lp_command_parse):
 *   "> hello"       ->  "< lilpet fw=<version> engine=<version> board=<board> camera=<0|1> motion=<0|1>"
 *   "> look <text>" ->  "< ok" (written to flash at once, or in its turn: look_saving.h), or "< err <reason>"
 *   "> look?"       ->  "< look <text>", the current look
 *   "> imu?"        ->  "< imu <x> <y> <z>", gravity in g in screen coordinates (imu.h), or "< err no motion sensor"
 *   "> emotion <name> [seconds]"  an emotion for that long (above 0, up to 3600); with no seconds, the new mood
 *   "> react <event>"             the reaction to an event, by its name in the engine (tap, gesture_like...)
 *   "> face <x> <y>"              a face at x, y in -1..1, to send again while it is seen (lp_senses_face)
 *   "> face off"                  the face is gone
 * The last four answer "< ok", "< err <reason>" for bad input, or "< err busy" when the drawing loop's queue is full.
 * On a board that plays the benchmark animation they are refused, and looks are kept but not shown. A line too long
 * gets "< err line too long". Other lines are ignored; the timing reports share the port. */
#ifndef LILPET_COMMANDS_H
#define LILPET_COMMANDS_H

#include "lilpet/lilpet.h"

#include <stdbool.h>

/* Starts the task that reads commands. `current` is the look shown at startup, `motion` tells hello that a motion
 * sensor answers. */
void commands_start(const lp_look *current, bool motion);

/* Takes the next command for the eyes; false when none is waiting. The engine is not thread-safe: the drawing loop
 * carries them out, every frame. It lists every lp_command_type, so a new command does not build until it is handled
 * there, instead of answering "< ok" and being dropped. */
bool commands_next(lp_command *out);

#endif
