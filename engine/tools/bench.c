/*
 * Renderer benchmark on the host, to spot performance regressions: mise run engine:bench (Release build). Times
 * depend on the machine, so only compare runs made on the same one; the real budget is measured on the ESP32-S3
 * later. Prints Markdown tables (CI appends them to the job summary).
 *
 * - Each reference scene is drawn on a fresh renderer (whole screen, like the first frame), then drawn again (only
 *   the region of the eyes and particles, like a still frame on the device). Median of RUNS renders.
 * - Animation: one second per emotion, with idle saccades, blinks and particles, then the gaze follows a face moving
 *   in a circle. Each frame redraws only its dirty region, as on the device, and keeps its fastest time over PASSES
 *   identical passes, which filters out noise from the rest of the system.
 */
#include "../tests/js_vectors.h"
#include "lilpet/lilpet.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

enum { RUNS = 31, PASSES = 5, FPS = 60, FOLLOW_FRAMES = 3 * FPS, ANIM_FRAMES = LP_EMOTION_COUNT * FPS + FOLLOW_FRAMES };

static lp_eyes eyes;
static lp_renderer renderer;
static uint16_t frame[LP_SCREEN_SIZE * LP_SCREEN_SIZE];
static int64_t update_ns[ANIM_FRAMES], render_ns[ANIM_FRAMES], redrawn[ANIM_FRAMES];

static int64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * INT64_C(1000000000) + ts.tv_nsec;
}

static int compare_i64(const void *a, const void *b)
{
    const int64_t x = *(const int64_t *)a;
    const int64_t y = *(const int64_t *)b;
    return (x > y) - (x < y);
}

/* Sorts v in place */
static int64_t median(int64_t *v, int n)
{
    qsort(v, (size_t)n, sizeof v[0], compare_i64);
    return v[n / 2];
}

static int64_t rect_area(lp_rect r)
{
    return r.x1 > r.x0 && r.y1 > r.y0 ? (int64_t)(r.x1 - r.x0) * (r.y1 - r.y0) : 0;
}

/* Nanoseconds as microseconds with one decimal, without floating point */
static void print_us(int64_t ns)
{
    printf(" %lld.%lld |", (long long)(ns / 1000), (long long)(ns % 1000 / 100));
}

/* Share of the screen in percent, one decimal */
static void print_share(int64_t area)
{
    const int64_t permille = area * 1000 / (LP_SCREEN_SIZE * LP_SCREEN_SIZE);
    printf(" %lld.%lld %% |", (long long)(permille / 10), (long long)(permille % 10));
}

/* Mean, 95th percentile and max of v (sorts v) */
static void print_stats(const char *label, int64_t *v, void (*print)(int64_t))
{
    int64_t sum = 0;
    for (int i = 0; i < ANIM_FRAMES; i++)
        sum += v[i];
    qsort(v, ANIM_FRAMES, sizeof v[0], compare_i64);
    printf("| %s |", label);
    print(sum / ANIM_FRAMES);
    print(v[ANIM_FRAMES * 95 / 100]);
    print(v[ANIM_FRAMES - 1]);
    printf("\n");
}

static void bench_scenes(void)
{
    printf("| Reference scene | Whole screen (µs) | Eyes only (µs) | Eyes area |\n");
    printf("| --- | ---: | ---: | ---: |\n");
    const int count = (int)(sizeof LPV_SCENES / sizeof LPV_SCENES[0]);
    for (int i = 0; i < count; i++) {
        lp_eyes_init(&eyes, 1);
        lp_eyes_set_scene(&eyes, &LPV_SCENES[i].scene, &LPV_LOOKS[LPV_SCENES[i].look]);
        int64_t whole[RUNS];
        int64_t still[RUNS];
        lp_rect region = {0, 0, 0, 0};
        for (int r = 0; r < RUNS; r++) {
            lp_renderer_init(&renderer);
            const int64_t t0 = now_ns();
            lp_render(&renderer, &eyes, frame);
            const int64_t t1 = now_ns();
            region = lp_render(&renderer, &eyes, frame);
            still[r] = now_ns() - t1;
            whole[r] = t1 - t0;
        }
        printf("| %s |", LPV_SCENES[i].id);
        print_us(median(whole, RUNS));
        print_us(median(still, RUNS));
        print_share(rect_area(region));
        printf("\n");
    }
}

/* One second per emotion, then the gaze follows a face moving in a circle */
static void script(int f)
{
    if (f % FPS == 0 && f / FPS < LP_EMOTION_COUNT)
        lp_eyes_set_emotion(&eyes, (lp_emotion)(f / FPS), 0.0f);
    if (f >= LP_EMOTION_COUNT * FPS) {
        const float a = (float)f * 0.05f;
        lp_eyes_set_pointer(&eyes, true, 0.8f * cosf(a), 0.6f * sinf(a));
    }
}

static void bench_animation(void)
{
    for (int pass = 0; pass < PASSES; pass++) {
        lp_eyes_init(&eyes, 1);
        lp_eyes_set_look(&eyes, &LPV_LOOKS[0]);
        lp_renderer_init(&renderer);
        lp_render(&renderer, &eyes, frame); /* the first frame clears the whole screen: not counted */
        for (int f = 0; f < ANIM_FRAMES; f++) {
            script(f);
            const int64_t t0 = now_ns();
            lp_eyes_update(&eyes, 1.0f / (float)FPS);
            const int64_t t1 = now_ns();
            const lp_rect dirty = lp_render(&renderer, &eyes, frame);
            const int64_t t2 = now_ns();
            if (pass == 0 || t1 - t0 < update_ns[f])
                update_ns[f] = t1 - t0;
            if (pass == 0 || t2 - t1 < render_ns[f])
                render_ns[f] = t2 - t1;
            redrawn[f] = rect_area(dirty);
        }
    }
    printf("| Animation, %d frames at %d frames/s | Mean | 95th percentile | Max |\n", ANIM_FRAMES, FPS);
    printf("| --- | ---: | ---: | ---: |\n");
    print_stats("Update (µs)", update_ns, print_us);
    print_stats("Render (µs)", render_ns, print_us);
    print_stats("Redrawn area", redrawn, print_share);
}

int main(void)
{
    printf("## lilpet %s renderer benchmark (host, Release)\n\n", lp_version());
    printf("Compare runs made on the same machine only.\n\n");
    bench_scenes();
    printf("\n");
    bench_animation();
    return 0;
}
