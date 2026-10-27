/*
 * Firmware entry point: the eyes, live on the display. Each frame starts on the display refresh signal (TE), draws
 * its dirty region band by band into two small buffers in internal RAM, and sends each band by DMA while drawing the
 * next one: no whole frame in memory, so it also runs on boards without PSRAM. The animation is the one of the
 * benchmarks (engine/tools/bench_script.h), in a loop, alternating two looks. Timings go to the USB serial port every
 * 10 seconds (mise run firmware:monitor).
 *
 * Without a display (no TE signal, e.g. on a bare M5Stamp S3A), frames are paced at 60 per second and the bands go
 * out on unconnected pins: the timings stay meaningful.
 */
#include "../../engine/tools/bench_script.h"
#include "display.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lilpet/lilpet.h"
#include "sdkconfig.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BAND_ROWS     30 /* like the Waveshare display demos: 466 x 30 RGB565 pixels = 27960 bytes per buffer */
#define BAND_BYTES    (LP_SCREEN_SIZE * BAND_ROWS * sizeof(uint16_t))
#define REPORT_FRAMES (10 * LP_BENCH_FPS)

/* From eyes/looks.json: a dot pupil, and an acorn pupil (drawn as polygons, the slowest) */
static const lp_look LOOKS[] = {
    {0xFF1A1Au, LP_PUPIL_DOT, {0x2B4CFFu, 0x2B4CFFu}, false},
    {0x6EF2CFu, LP_PUPIL_ACORN, {0x167358u, 0x167358u}, false},
};

static lp_eyes eyes;
static lp_renderer renderer;

/* Sums over the frames since the last report, in microseconds */
static struct {
    int frames;
    int64_t render, render_max, frame, frame_max, area;
} stats;

static void report(int64_t elapsed_us, bool synced)
{
    const int n = stats.frames;
    printf("%" PRId64 " frames/s (%s) | draw mean %" PRId64 " max %" PRId64 " us | draw + send mean %" PRId64
           " max %" PRId64 " us | redrawn %" PRId64 " per mille of the screen\n",
           (int64_t)n * 1000000 / elapsed_us, synced ? "display refresh" : "no display, 60 paced", stats.render / n,
           stats.render_max, stats.frame / n, stats.frame_max, stats.area / n);
    memset(&stats, 0, sizeof stats);
}

void app_main(void)
{
    uint16_t *bands[2] = {heap_caps_malloc(BAND_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL),
                          heap_caps_malloc(BAND_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)};
    const bool synced = display_init(BAND_BYTES);
    printf("\nlilpet %s, ESP32-S3 at %d MHz, bands of %d rows, %s, %u bytes of internal RAM free\n", lp_version(),
           CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ, BAND_ROWS,
           synced ? "display found (TE signal)" : "no display found (no TE signal)",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    if (!bands[0] || !bands[1]) {
        printf("not enough DMA memory for the bands\n");
        return;
    }

    lp_eyes_init(&eyes, 1);
    lp_renderer_init(&renderer);
    TickType_t wake = xTaskGetTickCount();
    int64_t last = esp_timer_get_time();
    int64_t report_start = last;
    for (int f = 0;; f++) {
        if (synced)
            display_wait_refresh(50);
        else
            vTaskDelayUntil(&wake, pdMS_TO_TICKS(1000 / LP_BENCH_FPS));

        const int64_t start = esp_timer_get_time();
        const int step = f % LP_BENCH_FRAMES;
        if (step == 0)
            lp_eyes_set_look(&eyes, &LOOKS[(f / LP_BENCH_FRAMES) % 2]);
        lp_bench_script(&eyes, step);
        const float dt = (float)(start - last) * 1e-6f;
        lp_eyes_update(&eyes, dt < 0.05f ? dt : 0.05f);
        last = start;

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
        stats.area += (int64_t)(dirty.x1 - dirty.x0) * (dirty.y1 - dirty.y0) * 1000 / (LP_SCREEN_SIZE * LP_SCREEN_SIZE);
        if (stats.frames == REPORT_FRAMES) {
            const int64_t now = esp_timer_get_time();
            report(now - report_start, synced);
            report_start = now;
        }
    }
}
