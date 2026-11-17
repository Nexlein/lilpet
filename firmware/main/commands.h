/* Commands from the PC app over the USB serial port, one per line: those lp_command_parse reads, and test hang and test
 * sleep, the firmware's own, for the checks. Which they are, what each takes and answers, is in docs/protocole.md (a
 * test makes sure it lists them all); the timing reports share the port. */
#ifndef LILPET_COMMANDS_H
#define LILPET_COMMANDS_H

#include "lilpet/lilpet.h"
#include "rest.h"

#include <stdbool.h>

/* What hello tells of the board, besides its versions and its name */
typedef struct {
    bool motion;         /* a motion sensor answers */
    const char *reset;   /* why the chip last started: a name of esp_reset_reason_t, from main.c */
    const char *storage; /* what look_store_init found: ok, erased or none */
} commands_hello;

/* Starts the task that reads commands. `shown` is the look shown at startup, `kept` the one the flash holds (NULL when
 * the start read none: nothing saved, unreadable, or not read after a crash, look_saving.h). */
void commands_start(const lp_look *shown, const lp_look *kept, const commands_hello *hello);

/* True once `> test hang` came: the drawing loop then blocks for good, as a stuck wait would, for its watchdog to reset
 * the chip (docs/protocole.md) */
bool commands_hang_asked(void);

/* True once `> test sleep` came, then false until the next one: the drawing loop sleeps at once */
bool commands_sleep_asked(void);

/* True when a command line came since the last call, waiting up to `wait_ms` for one: a command wakes the eyes. The
 * test commands do not count, or `> test sleep` would wake what it put to sleep. */
bool commands_heard(int wait_ms);

/* What hello tells of the eyes' rest and of the battery (millivolts, or -1 for none), kept by the drawing loop */
void commands_set_status(rest_state state_now, int battery_mv);

/* Takes the next command for the eyes; false when none is waiting. The engine is not thread-safe: the drawing loop
 * carries them out, every frame. It lists every lp_command_type, so a new command does not build until it is handled
 * there, instead of answering "< ok" and being dropped. */
bool commands_next(lp_command *out);

#endif
