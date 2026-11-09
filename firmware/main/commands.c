#include "commands.h"

#include "board_config.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_app_desc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "imu.h"
#include "look_saving.h"
#include "look_store.h"

#include "esp_timer.h"

#include <stdio.h>

#define QUEUE_SLOTS  32   /* the drawing loop empties them every frame: a burst of 32 lines between two frames fits */
#define RX_BUFFER    1024 /* bytes of commands waiting on the USB port */
#define READ_WAIT_MS 100  /* how long a read waits for a byte before a look waiting for its turn gets it */

/* Set before the task starts, then written by the task alone */
static struct {
    lp_look current;
    lp_look kept;     /* the last look written to flash */
    int64_t tried_at; /* the last write or try (esp_timer_get_time) */
    bool motion;
} state;

/*
 * To the drawing loop. A command gets a slot at once or `< err busy`, never a wait: while this task waits, nothing
 * reads the USB port, and the driver drops the packets that no longer fit RX_BUFFER, which splices the lines behind.
 */
static QueueHandle_t queue;

static bool push(const lp_command *c)
{
    return xQueueSend(queue, c, 0) == pdTRUE;
}

/* Writes the shown look to flash once its turn comes (look_saving.h); false when the flash refuses it */
static bool save_if_due(void)
{
    const int64_t now = esp_timer_get_time();
    if (!look_save_due(&state.current, &state.kept, now, state.tried_at))
        return true;
    state.tried_at = now;
    if (!look_store_save(&state.current))
        return false;
    state.kept = state.current;
    return true;
}

static void handle_eyes_command(const lp_command *c)
{
    if (BOARD_BENCH)
        printf("< err benchmark animation running\n");
    else if (!push(c))
        printf("< err busy\n");
    else
        printf("< ok\n");
}

static void handle_look(const lp_command *c)
{
    /* The benchmark animation keeps its own look */
    if (!BOARD_BENCH && !push(c)) {
        printf("< err busy\n");
        return;
    }
    /* Shown even when it cannot be kept: the app still sees it. Written now if its turn has come, else by read_line */
    state.current = c->look;
    if (!save_if_due())
        printf("< err shown, but cannot save the look in flash\n");
    else
        printf("< ok\n");
}

static void reply_look(const lp_look *look)
{
    char text[LP_LOOK_TEXT_SIZE];
    lp_look_format(look, text);
    printf("< look %s\n", text);
}

static void reply_imu(void)
{
    float g[3];
    if (imu_gravity(g))
        printf("< imu %.3f %.3f %.3f\n", (double)g[0], (double)g[1], (double)g[2]);
    else
        printf("< err no motion sensor\n");
}

static void handle(const lp_command_line *line)
{
    lp_command c;
    const char *error = lp_command_parse(line, &c);
    if (!error && c.type == LP_COMMAND_NONE)
        return;
    /* With no one reading, the log stops mid-line when the USB buffer fills: a reply must start on a line of its own */
    putchar('\n');

    if (error) {
        printf("< err %s\n", error);
    } else if (c.type == LP_COMMAND_HELLO) {
        printf("< lilpet fw=%s engine=%s board=%s camera=%d motion=%d\n", esp_app_get_description()->version,
               lp_version(), BOARD_NAME, BOARD_CAMERA, state.motion);
    } else if (c.type == LP_COMMAND_LOOK_QUERY) {
        reply_look(&state.current);
    } else if (c.type == LP_COMMAND_LOOK) {
        handle_look(&c);
    } else if (c.type == LP_COMMAND_IMU_QUERY) {
        reply_imu();
    } else {
        handle_eyes_command(&c);
    }
    fflush(stdout);
}

/* Waits for a line and reads it, whatever it holds. The driver's read, with a timeout: after each byte, and while the
 * port is quiet, a look left for later gets written once its turn comes */
static void read_line(lp_command_line *line)
{
    *line = (lp_command_line){0};
    for (;;) {
        char byte;
        const bool got = usb_serial_jtag_read_bytes(&byte, 1, pdMS_TO_TICKS(READ_WAIT_MS)) == 1;
        if (!save_if_due()) /* no one waits for a reply: a failure goes to the log, and is tried again a gap later */
            printf("\ncannot save the look in flash\n"); /* on a line of its own, like a reply (handle) */
        if (got && lp_command_line_add(line, byte))
            return;
    }
}

static void commands_task(void *arg)
{
    (void)arg;
    lp_command_line line;
    for (;;) {
        read_line(&line);
        handle(&line);
    }
}

void commands_start(const lp_look *current, bool motion)
{
    /* Reads with a timeout (read_line) and the replies through printf: both need the port's driver, not just the
     * default console output. The driver drops what does not fit in its buffer: this one holds a burst of about 80
     * commands, where the default 256 bytes held 20. */
    usb_serial_jtag_driver_config_t config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    config.rx_buffer_size = RX_BUFFER;
    usb_serial_jtag_driver_install(&config);
    usb_serial_jtag_vfs_use_driver();

    queue = xQueueCreate(QUEUE_SLOTS, sizeof(lp_command));
    state.current = *current;
    state.kept = *current;              /* what the flash holds, or the default look that stands for nothing kept */
    state.tried_at = -LOOK_SAVE_GAP_US; /* the first new look is written at once */
    state.motion = motion;
    xTaskCreate(commands_task, "commands", 4096, NULL, 5, NULL);
}

bool commands_next(lp_command *out)
{
    return xQueueReceive(queue, out, 0) == pdTRUE;
}
