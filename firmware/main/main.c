/*
 * Firmware entry point: the eyes, live on the display. Each frame draws its dirty region band by band into two small
 * buffers in internal RAM, every other row of a band on each core, and sends a band by DMA while the next one is drawn:
 * no whole frame in memory, so it runs without PSRAM. Without a display, the eyes play the benchmark animation instead
 * (engine/tools/bench_script.h). With nothing happening, the eyes get sleepy, then sleep, the screen off, until a
 * command, the button or a move wakes them; a low battery makes them look tired. What the firmware does, board by
 * board: docs/firmware.md; why both cores: docs/adr/0009-rendu-deux-coeurs.md; power: docs/adr/0012-energie.md.
 */
#include "../../engine/tools/bench_script.h"
#include "battery.h"
#include "battery_rule.h"
#include "board.h"
#include "board_config.h"
#include "commands.h"
#include "display.h"
#include "driver/usb_serial_jtag.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "frame.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "imu.h"
#include "lilpet/lilpet.h"
#include "look_store.h"
#include "rest.h"
#include "sdkconfig.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BAND_ROWS  30 /* like the Waveshare display demos: 466 x 30 RGB565 pixels = 27960 bytes per buffer */
#define BAND_BYTES (BOARD_DISPLAY_SIZE * BAND_ROWS * sizeof(uint16_t))

/* The engine draws at most LP_SCREEN_SIZE pixels a side, rounded down to even (lp_renderer_set_size): a larger or odd
 * size would leave part of the screen undrawn, silently */
_Static_assert(BOARD_DISPLAY_SIZE <= LP_SCREEN_SIZE && BOARD_DISPLAY_SIZE % 2 == 0,
               "the board's display must be even and no larger than the engine's screen");

#if CONFIG_FREERTOS_UNICORE
#error "the eyes are drawn on both cores: CONFIG_FREERTOS_UNICORE must stay off"
#endif

static lp_eyes eyes;
static lp_renderer renderer;
static lp_senses senses;

/*
 * The second core draws the odd rows of each band, from the frame `renderer` prepared and with its own scratch, while
 * this one draws the even rows. `start` hands it the band, `done` gives it back; the eyes are only read meanwhile. All
 * static, so that nothing can fail to start.
 */
#define SECOND_CORE_STACK 4096 /* bytes */
static struct {
    frame_band band;
    lp_row_scratch scratch;
    SemaphoreHandle_t start, done;
    StaticSemaphore_t start_buffer, done_buffer;
    StaticTask_t task;
    StackType_t stack[SECOND_CORE_STACK];
} second_core;

static void second_core_task(void *arg)
{
    (void)arg;
    for (;;) {
        xSemaphoreTake(second_core.start, portMAX_DELAY);
        frame_draw_core_rows(&renderer, 1, &second_core.scratch, &second_core.band);
        xSemaphoreGive(second_core.done);
    }
}

#define BATTERY_EVERY_US 10000000 /* how often the battery is read, in microseconds */
#define ASLEEP_POLL_MS   20       /* asleep without light sleep (a PC on USB): how often it looks for what wakes it */
#define LIGHT_SLEEP_US   1000000  /* asleep on its own: how long a light sleep lasts at most, between two looks */

/* The eyes' rest and the battery (rest.h, battery_rule.h) */
static struct {
    rest rest;
    rest_state state;     /* as the eyes show it */
    int brightness;       /* the screen's, as last set: 0 to 255 (rest_brightness) */
    int64_t battery_read; /* when, in microseconds; 0: not yet */
    int battery_mv;       /* -1: none */
    bool battery_low;
    lp_emotion mood_before_tired;
} power = {.battery_mv = -1, .brightness = 255};

/* Sums over the frames since the last report, in microseconds */
static struct {
    int frames;
    int64_t render, render_max, frame, frame_max, area;
} stats;

/* What the renderer needs for this board: the size of its display, and the byte order of the controllers */
static void start_renderer(void)
{
    lp_renderer_init(&renderer);
    lp_renderer_set_size(&renderer, BOARD_DISPLAY_SIZE); /* same picture, scaled to a smaller screen */
    lp_renderer_set_big_endian(&renderer, true);
}

/*
 * Draws the frame of `eyes` band by band into the two DMA buffers, every other row on each core, while the band
 * before is sent, then waits for the last one. Returns the region drawn; *render: the time both cores spent drawing.
 */
static lp_rect draw_frame(uint16_t *bands[2], int64_t *render)
{
    const lp_rect dirty = lp_render_begin(&renderer, &eyes);
    const int width = dirty.x1 - dirty.x0;
    *render = 0;
    int b = 0;
    for (int y = dirty.y0; y < dirty.y1; y += BAND_ROWS, b ^= 1) {
        const int y1 = y + BAND_ROWS < dirty.y1 ? y + BAND_ROWS : dirty.y1;
        const int64_t t = esp_timer_get_time();
        second_core.band = (frame_band){y, y1, width, bands[b]};
        xSemaphoreGive(second_core.start);
        frame_draw_core_rows(&renderer, 0, NULL, &second_core.band);
        xSemaphoreTake(second_core.done, portMAX_DELAY);
        *render += esp_timer_get_time() - t;
        display_draw(dirty.x0, y, dirty.x1, y1, bands[b]);
    }
    display_flush();
    return dirty;
}

/* A round of the benchmark starts as each pass of tools/bench.c: the same eyes, and their first frame drawn whole,
 * not counted. Its frames are then the host's, every one. */
static void start_bench_round(uint16_t *bands[2])
{
    lp_bench_start(&eyes);
    start_renderer();
    int64_t render;
    draw_frame(bands, &render);
}

static void report(int64_t elapsed_us)
{
    const int n = stats.frames;
    const int64_t tenths = (int64_t)n * 10000000 / elapsed_us; /* frames per second, in tenths */
    printf("%" PRId64 ".%" PRId64 " frames/s | draw mean %" PRId64 " max %" PRId64 " us | draw + send mean %" PRId64
           " max %" PRId64 " us | redrawn %" PRId64 " per mille of the screen\n",
           tenths / 10, tenths % 10, stats.render / n, stats.render_max, stats.frame / n, stats.frame_max,
           stats.area / n);
    memset(&stats, 0, sizeof stats);
}

/* Why the chip last started, for hello and the boot line: the name of the esp_reset_reason_t, in lower case */
static const char *reset_name(esp_reset_reason_t reason)
{
    switch (reason) {
    case ESP_RST_POWERON:
        return "poweron";
    case ESP_RST_EXT:
        return "ext";
    case ESP_RST_SW:
        return "sw";
    case ESP_RST_PANIC:
        return "panic";
    case ESP_RST_INT_WDT:
        return "int_wdt";
    case ESP_RST_TASK_WDT:
        return "task_wdt";
    case ESP_RST_WDT:
        return "wdt";
    case ESP_RST_DEEPSLEEP:
        return "deepsleep";
    case ESP_RST_BROWNOUT:
        return "brownout";
    case ESP_RST_SDIO:
        return "sdio";
    case ESP_RST_USB:
        return "usb";
    case ESP_RST_JTAG:
        return "jtag";
    case ESP_RST_EFUSE:
        return "efuse";
    case ESP_RST_PWR_GLITCH:
        return "pwr_glitch";
    case ESP_RST_CPU_LOCKUP:
        return "cpu_lockup";
    case ESP_RST_UNKNOWN:
        break;
    }
    return "unknown";
}

/* A start after a crash leaves the saved data out once: if they caused it, the next start is not another crash */
static bool after_crash(esp_reset_reason_t reason)
{
    return reason == ESP_RST_PANIC || reason == ESP_RST_INT_WDT || reason == ESP_RST_TASK_WDT ||
           reason == ESP_RST_WDT || reason == ESP_RST_CPU_LOCKUP;
}

/* The eyes, and what hello tells, as they enter `state` */
static void enter_rest(rest_state state)
{
    if (state == power.state)
        return;
    rest_show(&eyes, power.state, state);
    power.state = state;
    commands_set_status(state, power.battery_mv);
}

/* Reads the battery every BATTERY_EVERY_US: the eyes look tired while it is low (battery_show) */
static void read_battery(int64_t now)
{
    if (power.battery_read != 0 && now - power.battery_read < BATTERY_EVERY_US)
        return;
    power.battery_read = now;
    power.battery_mv = battery_millivolts();
    const bool low = battery_low(power.battery_mv, power.battery_low);
    battery_show(&eyes, power.battery_low, low, &power.mood_before_tired);
    if (low && !power.battery_low)
        printf("battery low: %d mV, the eyes look tired\n", power.battery_mv);
    else if (!low && power.battery_low && power.battery_mv >= 0)
        printf("battery back: %d mV\n", power.battery_mv);
    else if (!low && power.battery_low)
        printf("battery no longer read\n");
    power.battery_low = low;
    commands_set_status(power.state, power.battery_mv);
}

/* What the board senses: activity is what keeps the eyes awake or wakes them, a command taken, a press or a move. A new
 * input (a touch screen, a power key) joins it here, for the sleep loop and the frames alike. */
typedef struct {
    bool active;
    bool button;  /* held down now, for the eyes' tap */
    bool pressed; /* the button just went down: what wakes the eyes on a press (rest_woke) */
    bool gravity; /* g holds gravity */
    float g[3];
} inputs;

/* Reads them all; wait_ms: how long to wait for a command */
static inputs read_inputs(int wait_ms)
{
    inputs in = {.gravity = false};
    const bool heard = commands_heard(wait_ms);
    in.button = board_button_pressed();
    in.pressed = rest_button_down(&power.rest, in.button);
    in.gravity = imu_gravity(in.g);
    in.active = heard || in.pressed || (in.gravity && rest_handled(&power.rest, in.g));
    return in;
}

/*
 * Asleep: the screen off and the drawing loop out of its watchdog, until a command, the button or a move wakes the
 * eyes. Light sleep only with no USB host (board_light_sleep), and on a board that rests by itself (`rests`): the
 * others wake on a command only, which needs the USB pad on. Returns how long it slept, in microseconds.
 */
static int64_t sleep_until_woken(bool rests)
{
    const int64_t start = esp_timer_get_time();
    enter_rest(REST_ASLEEP);
    display_sleep();
    power.brightness = 0; /* the frames fade it in once awake (rest_brightness) */
    printf("asleep: screen off\n");
    ESP_ERROR_CHECK(esp_task_wdt_delete(NULL));
    inputs in;
    for (;;) {
        const bool light = rests && !usb_serial_jtag_is_connected();
        if (light)
            board_light_sleep(LIGHT_SLEEP_US, power.rest.button_held);
        /* A tick at least: on a board whose loop never waits otherwise, the idle task would starve its own watchdog */
        in = read_inputs(light ? 1 : ASLEEP_POLL_MS);
        read_battery(esp_timer_get_time());
        if (in.active || commands_hang_asked())
            break;
    }
    (void)commands_sleep_asked(); /* a `> test sleep` that came while asleep asks for nothing more */
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    if (commands_hang_asked()) /* `> test hang`: the drawing loop hangs in its watchdog, the screen still off */
        return esp_timer_get_time() - start;
    display_wake();
    start_renderer(); /* the next frame is drawn whole, whatever the panel kept while it slept */
    /* Once the screen is back: its fade in starts now, not before the wait for its wake-up */
    rest_woke(&power.rest, esp_timer_get_time(), in.pressed);
    enter_rest(REST_AWAKE); /* before the log line: a hello that follows it finds the eyes awake */
    printf("awake: screen on\n");
    return esp_timer_get_time() - start;
}

static void apply(const lp_command *c)
{
    switch (c->type) {
    case LP_COMMAND_LOOK:
        lp_eyes_set_look(&eyes, &c->look);
        break;
    case LP_COMMAND_EMOTION:
        lp_eyes_set_emotion(&eyes, c->emotion.name, c->emotion.seconds);
        break;
    case LP_COMMAND_REACT:
        lp_eyes_react(&eyes, c->event);
        break;
    case LP_COMMAND_FACE:
        lp_senses_face(&senses, &eyes, c->face.seen, c->face.x, c->face.y);
        break;
    case LP_COMMAND_NONE: /* the others are answered by the task that reads commands */
    case LP_COMMAND_HELLO:
    case LP_COMMAND_LOOK_QUERY:
    case LP_COMMAND_IMU_QUERY:
        break;
    }
}

void app_main(void)
{
    /* First, before anything that could stop: the PC app and check-device tell a restart by this line, whatever the
     * log level */
    printf("\nlilpet starts\n");
    board_init();
    battery_init();
    uint16_t *bands[2] = {heap_caps_malloc(BAND_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL),
                          heap_caps_malloc(BAND_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)};
    /* Before the display: starting the sensor takes 0.25 s, with the backlight on it would show noise until the first
     * frame */
    const bool motion = imu_init();
    const char *screen = display_init(BAND_BYTES);
    const bool bench = BOARD_BENCH; /* no screen: replay the host benchmark's animation, for the timings */
    const esp_reset_reason_t reason = esp_reset_reason();
    commands_hello hello = {.motion = motion, .reset = reset_name(reason)};
    printf(
        "\nlilpet %s, board %s, ESP32-S3 at %d MHz, display: %s, %dx%d, motion sensor: %s, animation: %s, bands of %d "
        "rows on both cores, %u bytes of internal RAM free, reset: %s\n",
        lp_version(), BOARD_NAME, CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ, screen, BOARD_DISPLAY_SIZE, BOARD_DISPLAY_SIZE,
        motion ? "yes" : "none", bench ? "benchmark" : "idle", BAND_ROWS,
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), hello.reset);
    if (!bands[0] || !bands[1]) {
        printf("not enough DMA memory for the bands\n");
        return;
    }

    hello.storage = look_store_init();
    lp_look look = lp_default_look(); /* until the PC app sends one */
    const bool crashed = after_crash(reason);
    bool kept = false;
    if (crashed)
        printf("started after a crash: the default look this once, the flash keeps the saved one\n");
    else
        kept = look_store_load(&look);
    /* Nothing kept (none saved yet, unreadable, or not read after a crash): the first look sent is written, the default
     * one too, so that a later firmware's default does not replace a look picked on purpose */
    commands_start(&look, kept ? &look : NULL, &hello);

    lp_eyes_init(&eyes, 1); /* the benchmark starts its own eyes: start_bench_round */
    lp_eyes_set_look(&eyes, &look);
    start_renderer();
    second_core.start = xSemaphoreCreateBinaryStatic(&second_core.start_buffer);
    second_core.done = xSemaphoreCreateBinaryStatic(&second_core.done_buffer);
    xTaskCreateStaticPinnedToCore(second_core_task, "second core", SECOND_CORE_STACK, NULL, uxTaskPriorityGet(NULL),
                                  second_core.stack, &second_core.task, 1);
    lp_senses_init(&senses);
    power.rest = rest_start(esp_timer_get_time());
    /* Only a board that can wake by itself, on its button or on motion, rests by itself; the benchmark animation never
     * does: its timings would stop. `> test sleep` puts any board to sleep. */
    const bool rests = !bench && (motion || board_has_button());
    /* A frame that does not end within CONFIG_ESP_TASK_WDT_TIMEOUT_S resets the chip, whatever it waits for: a band
     * from the second core, the display's bus. The benchmark's first frame too. */
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    if (bench)
        start_bench_round(bands);
    int64_t last = esp_timer_get_time();
    int64_t report_start = last;
    for (int f = 0;; f = (f + 1) % LP_BENCH_FRAMES) {
        esp_task_wdt_reset();
        if (rest_state_at(&power.rest, esp_timer_get_time()) == REST_ASLEEP) {
            const int64_t slept = sleep_until_woken(rests); /* out of the frame times and of the report */
            last += slept;
            report_start += slept;
        }
        while (commands_hang_asked()) /* after the sleep, which leaves early for it: the hang comes before a frame */
            vTaskDelay(portMAX_DELAY);
        display_wait_frame();

        const int64_t start = esp_timer_get_time();
        lp_command c;
        while (commands_next(&c))
            apply(&c);
        const float elapsed = (float)(start - last) * 1e-6f;
        const float dt = elapsed < 0.05f ? elapsed : 0.05f;
        last = start;
        if (bench)
            lp_bench_script(&eyes, f);
        /* Before what came after it: a command that follows `> test sleep` wakes the eyes */
        const bool sleep_asked = commands_sleep_asked();
        const inputs in = read_inputs(0); /* every frame: a command taken now wakes nothing later */
        rest_frame(&power.rest, start, rests, sleep_asked, in.active);
        enter_rest(rest_state_at(&power.rest, start)); /* asleep: at the top of the next frame */
        read_battery(start);
        const int brightness = rest_brightness(&power.rest, start);
        if (brightness != power.brightness) { /* only while it fades, out before sleep or in after it */
            display_set_brightness(brightness);
            power.brightness = brightness;
        }
        lp_senses_update(&senses, &eyes, in.gravity ? in.g : NULL, rest_press(&power.rest, in.button), dt);
        lp_eyes_update(&eyes, bench ? 1.0f / LP_BENCH_FPS : dt);

        int64_t render;
        const lp_rect dirty = draw_frame(bands, &render);
        const int64_t frame = esp_timer_get_time() - start;

        stats.frames++;
        stats.render += render;
        stats.render_max = render > stats.render_max ? render : stats.render_max;
        stats.frame += frame;
        stats.frame_max = frame > stats.frame_max ? frame : stats.frame_max;
        stats.area += frame_area_permille(dirty, BOARD_DISPLAY_SIZE);
        if (f == LP_BENCH_FRAMES - 1) { /* a whole round of the benchmark: every report covers the same frames */
            const int64_t now = esp_timer_get_time();
            report(now - report_start);
            if (bench)
                start_bench_round(bands); /* between two rounds, out of any frame's time */
            /* Outside the benchmark, the report's own time counts in the next one */
            report_start = bench ? esp_timer_get_time() : now;
        }
    }
}
