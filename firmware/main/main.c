/*
 * Firmware entry point. Until the display driver exists, it benchmarks the renderer on the chip the way the device
 * will draw: band by band into a small buffer in internal RAM, with no whole frame in memory, so it also runs on
 * boards without PSRAM. It plays the animation of engine/tools/bench_script.h (the same as the host benchmark) and
 * prints the timings on the USB serial port every few seconds (mise run firmware:monitor).
 */
#include "../../engine/tools/bench_script.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lilpet/lilpet.h"
#include "sdkconfig.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define BAND_ROWS 30 /* like the Waveshare display demos: 466 x 30 RGB565 pixels = 27960 bytes */
#define PASSES    3  /* each frame keeps its fastest time over the passes */

/* From eyes/looks.json: a dot pupil, and an acorn pupil (drawn as polygons, the slowest) */
static const lp_look LOOKS[] = {
    {0xFF1A1Au, LP_PUPIL_DOT, {0x2B4CFFu, 0x2B4CFFu}, false},
    {0x6EF2CFu, LP_PUPIL_ACORN, {0x167358u, 0x167358u}, false},
};
static const char *const LOOK_NAMES[] = {"cherry (dot pupil)", "mint (acorn pupil)"};

static lp_eyes eyes;
static lp_renderer renderer;
static uint16_t band[LP_SCREEN_SIZE * BAND_ROWS];
static int64_t update_us[LP_BENCH_FRAMES], render_us[LP_BENCH_FRAMES], redrawn[LP_BENCH_FRAMES];

/* Draws the dirty region band by band; on the device, each band goes to the display while the next one is drawn */
static lp_rect render_frame(void)
{
    const lp_rect dirty = lp_render_begin(&renderer, &eyes);
    for (int y = dirty.y0; y < dirty.y1; y += BAND_ROWS)
        lp_render_rows(&renderer, y, y + BAND_ROWS, band);
    return dirty;
}

static int compare_i64(const void *a, const void *b)
{
    const int64_t x = *(const int64_t *)a;
    const int64_t y = *(const int64_t *)b;
    return (x > y) - (x < y);
}

/* Mean, 95th percentile and max of v (sorts v) */
static void print_stats(const char *label, int64_t *v, const char *unit)
{
    int64_t sum = 0;
    for (int i = 0; i < LP_BENCH_FRAMES; i++)
        sum += v[i];
    qsort(v, LP_BENCH_FRAMES, sizeof v[0], compare_i64);
    printf("  %-13s mean %6" PRId64 "  p95 %6" PRId64 "  max %6" PRId64 " %s\n", label, sum / LP_BENCH_FRAMES,
           v[LP_BENCH_FRAMES * 95 / 100], v[LP_BENCH_FRAMES - 1], unit);
}

static void bench_look(int look)
{
    int64_t first = 0;
    for (int pass = 0; pass < PASSES; pass++) {
        lp_eyes_init(&eyes, 1);
        lp_eyes_set_look(&eyes, &LOOKS[look]);
        lp_renderer_init(&renderer);
        const int64_t t = esp_timer_get_time();
        render_frame(); /* the first frame redraws the whole screen */
        const int64_t whole = esp_timer_get_time() - t;
        if (pass == 0 || whole < first)
            first = whole;
        for (int f = 0; f < LP_BENCH_FRAMES; f++) {
            lp_bench_script(&eyes, f);
            const int64_t t0 = esp_timer_get_time();
            lp_eyes_update(&eyes, 1.0f / (float)LP_BENCH_FPS);
            const int64_t t1 = esp_timer_get_time();
            const lp_rect dirty = render_frame();
            const int64_t t2 = esp_timer_get_time();
            if (pass == 0 || t1 - t0 < update_us[f])
                update_us[f] = t1 - t0;
            if (pass == 0 || t2 - t1 < render_us[f])
                render_us[f] = t2 - t1;
            /* Redrawn share of the screen, in per mille */
            redrawn[f] =
                (int64_t)(dirty.x1 - dirty.x0) * (dirty.y1 - dirty.y0) * 1000 / (LP_SCREEN_SIZE * LP_SCREEN_SIZE);
            if (f % LP_BENCH_FPS == 0)
                vTaskDelay(1); /* let the idle task run, or the task watchdog complains */
        }
    }
    printf("Look %s\n  whole screen  %6" PRId64 " us\n", LOOK_NAMES[look], first);
    print_stats("update", update_us, "us");
    print_stats("render", render_us, "us");
    print_stats("redrawn area", redrawn, "per mille of the screen");
}

void app_main(void)
{
    for (;;) {
        printf("\nlilpet %s renderer benchmark, ESP32-S3 at %d MHz, bands of %d rows (%u bytes), %u bytes of internal "
               "RAM free\n",
               lp_version(), CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ, BAND_ROWS, (unsigned)sizeof band,
               (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
        printf("Animation of %d frames at %d frames/s; budget per frame %d us\n", LP_BENCH_FRAMES, LP_BENCH_FPS,
               1000000 / LP_BENCH_FPS);
        for (int look = 0; look < (int)(sizeof LOOKS / sizeof LOOKS[0]); look++)
            bench_look(look);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
