#include "commands.h"

#include "board_config.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_app_desc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "imu.h"
#include "look_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_SIZE     96
#define MAX_WORDS     2       /* after the name of a command */
#define MAX_SECONDS   3600.0f /* longest temporary emotion */
#define QUEUE_SLOTS   8       /* the drawing loop empties them every frame */
#define QUEUE_WAIT_MS 100     /* several frames: only a stuck drawing loop makes a command wait that long */
#define RX_BUFFER     1024    /* bytes of commands waiting on the USB port */

typedef struct {
    lp_look current;
    bool motion;
} commands_state;

/* To the drawing loop: a command that reached RX_BUFFER is never dropped, it waits for a slot or gets an error */
static QueueHandle_t queue;

static bool push(const command *c)
{
    return xQueueSend(queue, c, pdMS_TO_TICKS(QUEUE_WAIT_MS)) == pdTRUE;
}

/* A number from min to max; false on anything else, which covers nan and inf */
static bool parse_number(const char *word, float min, float max, float *out)
{
    char *end;
    const float value = strtof(word, &end);
    if (end == word || *end != '\0' || !(value >= min && value <= max))
        return false;
    *out = value;
    return true;
}

/* Cuts `text` into words, in place, and returns how many there are; only the first MAX_WORDS are kept */
static int split(char *text, char *words[MAX_WORDS])
{
    int count = 0;
    char *rest;
    for (char *word = strtok_r(text, " ", &rest); word; word = strtok_r(NULL, " ", &rest)) {
        if (count < MAX_WORDS)
            words[count] = word;
        count++;
    }
    return count;
}

/* The parse_ functions fill `out`, and return NULL or what is wrong with the command */
static const char *parse_emotion(char *const words[], int count, command *out)
{
    if (count < 1 || count > 2)
        return "malformed emotion, expected: <name> [seconds]";
    *out = (command){.type = COMMAND_EMOTION, .emotion = {.name = lp_emotion_from_name(words[0])}};
    if (out->emotion.name == LP_EMOTION_COUNT)
        return "unknown emotion";
    if (count == 2 &&
        (!parse_number(words[1], 0.0f, MAX_SECONDS, &out->emotion.seconds) || out->emotion.seconds == 0.0f))
        return "malformed emotion, seconds go from above 0 to 3600";
    return NULL;
}

static const char *parse_react(char *const words[], int count, command *out)
{
    if (count != 1)
        return "malformed react, expected: <event>";
    *out = (command){.type = COMMAND_REACT, .event = lp_event_from_name(words[0])};
    return out->event == LP_EVENT_COUNT ? "unknown event" : NULL;
}

static const char *parse_gaze(char *const words[], int count, command *out)
{
    *out = (command){.type = COMMAND_GAZE};
    if (count == 1 && strcmp(words[0], "off") == 0)
        return NULL;
    out->gaze.seen = true;
    if (count == 2 && parse_number(words[0], -1.0f, 1.0f, &out->gaze.x) &&
        parse_number(words[1], -1.0f, 1.0f, &out->gaze.y))
        return NULL;
    return "malformed gaze, expected: <x> <y> from -1 to 1, or off";
}

static const char *parse_eyes_command(const char *name, char *args, command *out)
{
    char *words[MAX_WORDS];
    const int count = split(args, words);
    if (strcmp(name, "emotion") == 0)
        return parse_emotion(words, count, out);
    if (strcmp(name, "react") == 0)
        return parse_react(words, count, out);
    return parse_gaze(words, count, out);
}

static void handle_eyes_command(const char *name, char *args)
{
    command c;
    const char *error = parse_eyes_command(name, args, &c);
    if (!error && BOARD_BENCH)
        error = "benchmark animation running";
    if (!error && !push(&c))
        error = "busy";
    if (error)
        printf("< err %s\n", error);
    else
        printf("< ok\n");
}

static void handle_look(commands_state *state, const char *text)
{
    command c = {.type = COMMAND_LOOK};
    if (!lp_look_parse(text, &c.look)) {
        printf("< err malformed look, expected: RRGGBB RRGGBB RRGGBB dot|cat|acorn|circle\n");
        return;
    }
    /* The benchmark animation keeps its own look */
    if (!BOARD_BENCH && !push(&c)) {
        printf("< err busy\n");
        return;
    }
    /* Shown even when it cannot be kept: the app still sees it */
    state->current = c.look;
    printf(look_store_save(&c.look) ? "< ok\n" : "< err shown, but cannot save the look in flash\n");
}

static void reply_look(const lp_look *look)
{
    char text[LP_LOOK_TEXT_SIZE];
    lp_look_format(look, text);
    printf("< look %s\n", text);
}

/* `whole`: the line fit in LINE_SIZE; otherwise it holds its start only */
static void handle(commands_state *state, char *line, bool whole)
{
    if (strncmp(line, "> ", 2) != 0)
        return;
    /* With no one reading, the log stops mid-line when the USB buffer fills: a reply must start on a line of its own */
    putchar('\n');
    char *name = line + 2;
    char *args = name + strcspn(name, " ");
    if (*args != '\0')
        *args++ = '\0';

    if (!whole) {
        printf("< err line too long\n");
    } else if (strcmp(name, "hello") == 0 && *args == '\0') {
        printf("< lilpet fw=%s engine=%s board=%s camera=%d motion=%d\n", esp_app_get_description()->version,
               lp_version(), BOARD_NAME, BOARD_CAMERA, state->motion);
    } else if (strcmp(name, "look?") == 0 && *args == '\0') {
        reply_look(&state->current);
    } else if (strcmp(name, "look") == 0) {
        handle_look(state, args);
    } else if (strcmp(name, "imu?") == 0 && *args == '\0') {
        float g[3];
        if (imu_gravity(g))
            printf("< imu %.3f %.3f %.3f\n", (double)g[0], (double)g[1], (double)g[2]);
        else
            printf("< err no motion sensor\n");
    } else if (strcmp(name, "emotion") == 0 || strcmp(name, "react") == 0 || strcmp(name, "gaze") == 0) {
        handle_eyes_command(name, args);
    } else {
        printf("< err unknown command\n");
    }
    fflush(stdout);
}

/* Reads a line, without its end. False when it does not fit: `line` keeps its start, and the rest is thrown away up to
 * the end of the line, so that it cannot pass for a command. */
static bool read_line(char *line, size_t size)
{
    char rest[32];
    char *chunk = line;
    for (;;) {
        if (!fgets(chunk, (int)size, stdin)) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        const size_t length = strcspn(chunk, "\r\n");
        const bool ended = chunk[length] != '\0' || length + 1 < size;
        chunk[length] = '\0';
        if (ended)
            return chunk == line;
        chunk = rest;
        size = sizeof rest;
    }
}

static void commands_task(void *arg)
{
    commands_state *state = arg;
    char line[LINE_SIZE];
    for (;;) {
        const bool whole = read_line(line, sizeof line);
        handle(state, line, whole);
    }
}

void commands_start(const lp_look *current, bool motion)
{
    /* Blocking reads on the USB serial port: needs its driver, not just the default console output */
    usb_serial_jtag_driver_config_t config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    /* The driver drops what does not fit in its buffer: this one holds a burst of about 80 commands while the queue
     * drains, where the default 256 bytes held 20. */
    config.rx_buffer_size = RX_BUFFER;
    usb_serial_jtag_driver_install(&config);
    usb_serial_jtag_vfs_use_driver();
    setvbuf(stdin, NULL, _IONBF, 0);

    queue = xQueueCreate(QUEUE_SLOTS, sizeof(command));
    static commands_state state;
    state.current = *current;
    state.motion = motion;
    xTaskCreate(commands_task, "commands", 4096, &state, 5, NULL);
}

bool commands_next(command *out)
{
    return xQueueReceive(queue, out, 0) == pdTRUE;
}
