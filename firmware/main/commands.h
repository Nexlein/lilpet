/* Commands from the PC app over the USB serial port, one per line (protocol in notes/app.md):
 *   "> hello"       ->  "< lilpet fw=<version> engine=<version> board=<board> camera=<0|1> motion=<0|1>"
 *   "> look <text>" ->  "< ok" once the look is saved, or "< err <reason>"
 *   "> look?"       ->  "< look <text>", the current look
 *   "> imu?"        ->  "< imu <x> <y> <z>", gravity in g in screen coordinates (imu.h), or "< err no motion sensor"
 *   "> emotion <name> [seconds]"  an emotion for that long (above 0, up to 3600); with no seconds, the new mood
 *   "> react <event>"             the reaction to an event, by its name in the engine (tap, gesture_like...)
 *   "> gaze <x> <y>"              a face at x, y in -1..1, to send again while it is seen (lp_senses_face)
 *   "> gaze off"                  the face is gone
 * The last four answer "< ok", or "< err <reason>" for bad input. On a board that plays the benchmark animation they
 * are refused, and looks are kept but not shown. Other lines are ignored; the timing reports share the port. */
#ifndef LILPET_COMMANDS_H
#define LILPET_COMMANDS_H

#include "lilpet/lilpet.h"

#include <stdbool.h>

/* What a command asks of the eyes */
typedef struct {
    enum { COMMAND_LOOK, COMMAND_EMOTION, COMMAND_REACT, COMMAND_GAZE } type;
    union {
        lp_look look;
        struct {
            lp_emotion name;
            float seconds; /* 0: the new mood */
        } emotion;
        lp_event event;
        struct {
            bool seen; /* false: the face is gone */
            float x, y;
        } gaze;
    };
} command;

/* Starts the task that reads commands. `current` is the look shown at startup, `motion` tells hello that a motion
 * sensor answers. */
void commands_start(const lp_look *current, bool motion);

/* Takes the next command for the eyes; false when none is waiting. The engine is not thread-safe: the drawing loop
 * carries them out, every frame. */
bool commands_next(command *out);

#endif
