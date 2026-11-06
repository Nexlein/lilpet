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

#define QUEUE_SLOTS   8    /* the drawing loop empties them every frame */
#define QUEUE_WAIT_MS 100  /* several frames: only a stuck drawing loop makes a command wait that long */
#define RX_BUFFER     1024 /* bytes of commands waiting on the USB port */

/* Shared by every link. `current` is written by the task of a link: a second one that takes looks needs a lock around
 * it. The queue, the flash and the motion sensor are safe from any task. */
typedef struct {
    lp_look current;
    bool motion;
} commands_state;

/* Where a task reads commands and sends the replies: a new link (the camera, over UART) is one more task */
typedef struct {
    FILE *in, *out;
    commands_state *state;
} commands_link;

/* To the drawing loop: a command that reached RX_BUFFER is never dropped, it waits for a slot or gets an error */
static QueueHandle_t queue;

static bool push(const lp_command *c)
{
    return xQueueSend(queue, c, pdMS_TO_TICKS(QUEUE_WAIT_MS)) == pdTRUE;
}

static void handle_eyes_command(FILE *out, const lp_command *c)
{
    if (BOARD_BENCH)
        fprintf(out, "< err benchmark animation running\n");
    else if (!push(c))
        fprintf(out, "< err busy\n");
    else
        fprintf(out, "< ok\n");
}

static void handle_look(const commands_link *link, const lp_command *c)
{
    /* The benchmark animation keeps its own look */
    if (!BOARD_BENCH && !push(c)) {
        fprintf(link->out, "< err busy\n");
        return;
    }
    /* Shown even when it cannot be kept: the app still sees it */
    link->state->current = c->look;
    fprintf(link->out, look_store_save(&c->look) ? "< ok\n" : "< err shown, but cannot save the look in flash\n");
}

static void reply_look(FILE *out, const lp_look *look)
{
    char text[LP_LOOK_TEXT_SIZE];
    lp_look_format(look, text);
    fprintf(out, "< look %s\n", text);
}

static void reply_imu(FILE *out)
{
    float g[3];
    if (imu_gravity(g))
        fprintf(out, "< imu %.3f %.3f %.3f\n", (double)g[0], (double)g[1], (double)g[2]);
    else
        fprintf(out, "< err no motion sensor\n");
}

static void handle(const commands_link *link, const lp_command_line *line)
{
    FILE *out = link->out;
    lp_command c;
    const char *error = lp_command_parse(line, &c);
    if (!error && c.type == LP_COMMAND_NONE)
        return;
    /* With no one reading, the log stops mid-line when the USB buffer fills: a reply must start on a line of its own */
    fputc('\n', out);

    if (error) {
        fprintf(out, "< err %s\n", error);
    } else if (c.type == LP_COMMAND_HELLO) {
        fprintf(out, "< lilpet fw=%s engine=%s board=%s camera=%d motion=%d\n", esp_app_get_description()->version,
                lp_version(), BOARD_NAME, BOARD_CAMERA, link->state->motion);
    } else if (c.type == LP_COMMAND_LOOK_QUERY) {
        reply_look(out, &link->state->current);
    } else if (c.type == LP_COMMAND_LOOK) {
        handle_look(link, &c);
    } else if (c.type == LP_COMMAND_IMU_QUERY) {
        reply_imu(out);
    } else {
        handle_eyes_command(out, &c);
    }
    fflush(out);
}

/* Waits for a line and reads it, whatever it holds */
static void read_line(FILE *in, lp_command_line *line)
{
    *line = (lp_command_line){0};
    for (;;) {
        const int byte = fgetc(in);
        if (byte == EOF) {
            clearerr(in);
            vTaskDelay(pdMS_TO_TICKS(20));
        } else if (lp_command_line_add(line, (char)byte)) {
            return;
        }
    }
}

static void commands_task(void *arg)
{
    const commands_link *link = arg;
    lp_command_line line;
    for (;;) {
        read_line(link->in, &line);
        handle(link, &line);
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

    queue = xQueueCreate(QUEUE_SLOTS, sizeof(lp_command));
    static commands_state state;
    state.current = *current;
    state.motion = motion;
    static commands_link usb = {.state = &state};
    usb.in = stdin;
    usb.out = stdout;
    xTaskCreate(commands_task, "commands", 4096, &usb, 5, NULL);
}

bool commands_next(lp_command *out)
{
    return xQueueReceive(queue, out, 0) == pdTRUE;
}
