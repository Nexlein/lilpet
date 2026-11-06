/*
 * Firmware entry point: the eyes, live on the display. Each frame starts when the display is ready for it, draws its
 * dirty region band by band into two small buffers in internal RAM, and sends each band by DMA while drawing the next
 * one: no whole frame in memory, so it also runs on boards without PSRAM. The eyes idle on their own (saccades, blinks,
 * the head following the gaze), with the look chosen in the PC app (kept in flash, cherry until one is sent).
 * Without a display, they play the benchmark animation instead (engine/tools/bench_script.h), in a loop, so the
 * timings compare with the host's. Commands from the app and the timings, printed once per round of that animation,
 * share the USB serial port (mise run firmware:monitor).
 *
 * One image per board: the build links its parts (display.h, imu.h), and the engine draws at the size of its screen
 * (board_config.h). With a motion sensor, tilting the board turns the face to stay level and slides the eyes toward the
 * low side, and the eyes react (REACTIONS in the engine): angry held upside down, dizzy when shaken, happy when the
 * button (the AtomS3R's screen) is pressed. lp_senses turns what the hardware reports into all of that.
 */
#include "../../engine/tools/bench_script.h"
#include "board.h"
#include "board_config.h"
#include "commands.h"
#include "display.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "imu.h"
#include "lilpet/lilpet.h"
#include "look_store.h"
#include "sdkconfig.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BAND_ROWS     30 /* like the Waveshare display demos: 466 x 30 RGB565 pixels = 27960 bytes per buffer */
#define BAND_BYTES    (BOARD_DISPLAY_SIZE * BAND_ROWS * sizeof(uint16_t))
#define REPORT_FRAMES LP_BENCH_FRAMES /* a whole round of the benchmark: every report covers the same frames */

/* Until the PC app sends a look: cherry with a dot pupil, from eyes/looks.json */
static const lp_look DEFAULT_LOOK = {0xAE0002u, LP_PUPIL_DOT, {0xFF5252u, 0xFF5252u}};

static lp_eyes eyes;
static lp_renderer renderer;
static lp_senses senses;

/* Sums over the frames since the last report, in microseconds */
static struct {
    int frames;
    int64_t render, render_max, frame, frame_max, area;
} stats;

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
    default: /* the others are answered by the task that reads commands */
        break;
    }
}

void app_main(void)
{
    board_init();
    uint16_t *bands[2] = {heap_caps_malloc(BAND_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL),
                          heap_caps_malloc(BAND_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)};
    const char *screen = display_init(BAND_BYTES);
    const bool bench = BOARD_BENCH; /* no screen: replay the host benchmark's animation, for the timings */
    const bool motion = imu_init();
    printf(
        "\nlilpet %s, board %s, ESP32-S3 at %d MHz, display: %s, %dx%d, motion sensor: %s, animation: %s, bands of %d "
        "rows, %u bytes of internal RAM free\n",
        lp_version(), BOARD_NAME, CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ, screen, BOARD_DISPLAY_SIZE, BOARD_DISPLAY_SIZE,
        motion ? "yes" : "none", bench ? "benchmark" : "idle", BAND_ROWS,
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    if (!bands[0] || !bands[1]) {
        printf("not enough DMA memory for the bands\n");
        return;
    }

    look_store_init();
    lp_look look = DEFAULT_LOOK;
    look_store_load(&look);
    commands_start(&look, motion);

    lp_eyes_init(&eyes, 1);
    /* The benchmark replays the host's run (engine/tools/bench.c): its look, seed and fixed time step */
    lp_eyes_set_look(&eyes, bench ? &DEFAULT_LOOK : &look);
    lp_renderer_init(&renderer);
    lp_renderer_set_size(&renderer, BOARD_DISPLAY_SIZE); /* same picture, scaled to a smaller screen */
    lp_renderer_set_big_endian(&renderer, true);
    lp_senses_init(&senses);
    int64_t last = esp_timer_get_time();
    int64_t report_start = last;
    for (int f = 0;; f = (f + 1) % LP_BENCH_FRAMES) {
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
        float g[3];
        lp_senses_update(&senses, &eyes, imu_gravity(g) ? g : NULL, board_button_pressed(), dt);
        lp_eyes_update(&eyes, bench ? 1.0f / LP_BENCH_FPS : dt);

        const lp_rect dirty = lp_render_begin(&renderer, &eyes);
        int64_t render = 0;
        int b = 0;
        for (int y = dirty.y0; y < dirty.y1; y += BAND_ROWS, b ^= 1) {
            const int y1 = y + BAND_ROWS < dirty.y1 ? y + BAND_ROWS : dirty.y1;
            const int64_t t = esp_timer_get_time();
            lp_render_rows(&renderer, y, y1, bands[b]);
            render += esp_timer_get_time() - t;
            display_draw(dirty.x0, y, dirty.x1, y1, bands[b]);
        }
        display_flush();
        const int64_t frame = esp_timer_get_time() - start;

        stats.frames++;
        stats.render += render;
        stats.render_max = render > stats.render_max ? render : stats.render_max;
        stats.frame += frame;
        stats.frame_max = frame > stats.frame_max ? frame : stats.frame_max;
        stats.area +=
            (int64_t)(dirty.x1 - dirty.x0) * (dirty.y1 - dirty.y0) * 1000 / (BOARD_DISPLAY_SIZE * BOARD_DISPLAY_SIZE);
        if (stats.frames == REPORT_FRAMES) {
            const int64_t now = esp_timer_get_time();
            report(now - report_start);
            report_start = now;
        }
    }
}
