/* Commands from the PC app over the USB serial port, one per line: hello, look, look?, imu?, emotion, react and face
 * (lp_command_parse reads them). What each takes and answers is in docs/protocole.md; the timing reports share the
 * port. */
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
