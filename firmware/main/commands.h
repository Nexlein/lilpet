/* Commands from the PC app over the USB serial port, one per line: hello, look, look?, imu?, emotion, react and face
 * (lp_command_parse reads them), and test hang, the firmware's own, for the checks. What each takes and answers is in
 * docs/protocole.md; the timing reports share the port. */
#ifndef LILPET_COMMANDS_H
#define LILPET_COMMANDS_H

#include "lilpet/lilpet.h"

#include <stdbool.h>

/* What hello tells of the board, besides its versions and its name */
typedef struct {
    bool motion;       /* a motion sensor answers */
    const char *reset; /* why the chip last started: a name of esp_reset_reason_t, from main.c */
} commands_hello;

/* Starts the task that reads commands. `shown` is the look shown at startup, `kept` the one the flash holds (NULL when
 * the start did not read it, look_saving.h). */
void commands_start(const lp_look *shown, const lp_look *kept, const commands_hello *hello);

/* True once `> test hang` came: the drawing loop then blocks for good, as a stuck wait would, for its watchdog to reset
 * the chip (docs/protocole.md) */
bool commands_hang_asked(void);

/* Takes the next command for the eyes; false when none is waiting. The engine is not thread-safe: the drawing loop
 * carries them out, every frame. It lists every lp_command_type, so a new command does not build until it is handled
 * there, instead of answering "< ok" and being dropped. */
bool commands_next(lp_command *out);

#endif
