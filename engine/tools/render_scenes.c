/*
 * Renders every reference scene with the engine and writes one PPM image per scene, to compare by eye with
 * prototype/reference.html. Usage: lilpet_render_scenes <output-dir>
 */
#include "../tests/js_vectors.h"
#include "lilpet/lilpet.h"

#include <stdio.h>

static lp_eyes eyes;
static lp_renderer renderer;
static uint16_t frame[LP_SCREEN_SIZE * LP_SCREEN_SIZE];

static int write_ppm(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return 1;
    fprintf(f, "P6\n%d %d\n255\n", LP_SCREEN_SIZE, LP_SCREEN_SIZE);
    for (int i = 0; i < LP_SCREEN_SIZE * LP_SCREEN_SIZE; i++) {
        const uint32_t p = frame[i];
        const unsigned char rgb[3] = {
            (unsigned char)(((p >> 11u) & 0x1Fu) * 255u / 31u),
            (unsigned char)(((p >> 5u) & 0x3Fu) * 255u / 63u),
            (unsigned char)((p & 0x1Fu) * 255u / 31u),
        };
        fwrite(rgb, 1, 3, f);
    }
    return fclose(f) != 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <output-dir>\n", argv[0]);
        return 2;
    }
    const int count = (int)(sizeof LPV_SCENES / sizeof LPV_SCENES[0]);
    for (int i = 0; i < count; i++) {
        lp_eyes_init(&eyes, 1);
        lp_eyes_set_scene(&eyes, &LPV_SCENES[i].scene, &LPV_LOOKS[LPV_SCENES[i].look]);
        lp_renderer_init(&renderer);
        lp_render(&renderer, &eyes, frame);
        char path[512];
        snprintf(path, sizeof path, "%s/%02d-%s.ppm", argv[1], i, LPV_SCENES[i].id);
        if (write_ppm(path)) {
            fprintf(stderr, "cannot write %s\n", path);
            return 1;
        }
    }
    printf("%d scenes written to %s\n", count, argv[1]);
    return 0;
}
