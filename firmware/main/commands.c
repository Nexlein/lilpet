#include "commands.h"

#include "board_config.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_app_desc.h"
#include "freertos/task.h"
#include "imu.h"
#include "look_store.h"

#include <stdio.h>
#include <string.h>

#define LINE_SIZE 96

typedef struct {
    QueueHandle_t looks;
    lp_look current;
    bool motion;
} commands_state;

static void reply_look(const lp_look *look)
{
    char text[LP_LOOK_TEXT_SIZE];
    lp_look_format(look, text, sizeof text);
    printf("< look %s\n", text);
}

static void handle(commands_state *state, const char *line)
{
    /* With no one reading, the log stops mid-line when the USB buffer fills: a reply must start on a line of its own */
    if (strncmp(line, "> ", 2) == 0)
        putchar('\n');
    if (strcmp(line, "> hello") == 0) {
        printf("< lilpet fw=%s engine=%s board=%s camera=%d motion=%d\n", esp_app_get_description()->version,
               lp_version(), BOARD_NAME, BOARD_CAMERA, state->motion);
    } else if (strcmp(line, "> look?") == 0) {
        reply_look(&state->current);
    } else if (strncmp(line, "> look ", 7) == 0) {
        lp_look look;
        if (!lp_look_parse(line + 7, &look)) {
            printf("< err malformed look, expected: RRGGBB RRGGBB RRGGBB dot|cat|acorn|circle\n");
        } else {
            /* Shown even when it cannot be kept: the app still sees it */
            state->current = look;
            xQueueOverwrite(state->looks, &look);
            printf(look_store_save(&look) ? "< ok\n" : "< err shown, but cannot save the look in flash\n");
        }
    } else if (strcmp(line, "> imu?") == 0) {
        float g[3];
        if (imu_gravity(g))
            printf("< imu %.3f %.3f %.3f\n", (double)g[0], (double)g[1], (double)g[2]);
        else
            printf("< err no motion sensor\n");
    } else if (strncmp(line, "> ", 2) == 0) {
        printf("< err unknown command\n");
    }
    fflush(stdout);
}

static void commands_task(void *arg)
{
    commands_state *state = arg;
    char line[LINE_SIZE];
    for (;;) {
        if (!fgets(line, sizeof line, stdin)) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        line[strcspn(line, "\r\n")] = '\0';
        handle(state, line);
    }
}

void commands_start(QueueHandle_t looks, const lp_look *current, bool motion)
{
    /* Blocking reads on the USB serial port: needs its driver, not just the default console output */
    usb_serial_jtag_driver_config_t config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    usb_serial_jtag_driver_install(&config);
    usb_serial_jtag_vfs_use_driver();
    setvbuf(stdin, NULL, _IONBF, 0);

    static commands_state state;
    state.looks = looks;
    state.current = *current;
    state.motion = motion;
    xTaskCreate(commands_task, "commands", 4096, &state, 5, NULL);
}
