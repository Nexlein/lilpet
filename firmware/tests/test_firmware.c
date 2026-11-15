/* Pure parts of the firmware, run with the engine's host tests: which LCD controller answers, the motion sensor's axes,
 * when a look is written to flash, the rows of a band each core draws, the share of the screen redrawn, and the column
 * and row ranges the display drivers send */
#include "../../engine/tests/test.h"
#include "../boards/atoms3r/board_config.h"
#include "../main/dcs.h"
#include "../main/frame.h"
#include "../main/imu_axes.h"
#include "../main/lcd_id.h"
#include "../main/look_saving.h"

void test_lcd_id_tells_the_two_controllers_apart(void)
{
    /* GC9107: 00 91 07, first byte lowest; ST7735S: 83 76 or 7C 89. Right away, after a dummy byte, after a dummy bit
     */
    const uint8_t gc9107[LCD_ID_ANSWER] = {0x00u, 0x91u, 0x07u, 0x00u, 0x00u, 0x00u};
    const uint8_t gc9107_late[LCD_ID_ANSWER] = {0xFFu, 0x00u, 0x91u, 0x07u, 0x00u, 0x00u};
    const uint8_t gc9107_bit[LCD_ID_ANSWER] = {0x00u, 0x48u, 0x83u, 0x80u, 0x00u, 0x00u}; /* 00 91 07 one bit later */
    const uint8_t st7735s[LCD_ID_ANSWER] = {0x83u, 0x76u, 0x00u, 0x00u, 0x00u, 0x00u};
    const uint8_t st7735s_other[LCD_ID_ANSWER] = {0x00u, 0x7Cu, 0x89u, 0x00u, 0x00u, 0x00u};
    const uint8_t nothing[LCD_ID_ANSWER] = {0};
    const uint8_t floating[LCD_ID_ANSWER] = {0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu};
    CHECK(lcd_id_match(gc9107) == LCD_ID_GC9107);
    CHECK(lcd_id_match(gc9107_late) == LCD_ID_GC9107);
    CHECK(lcd_id_match(gc9107_bit) == LCD_ID_GC9107);
    CHECK(lcd_id_match(st7735s) == LCD_ID_ST7735S);
    CHECK(lcd_id_match(st7735s_other) == LCD_ID_ST7735S);
    CHECK(lcd_id_match(nothing) == LCD_ID_UNKNOWN);
    CHECK(lcd_id_match(floating) == LCD_ID_UNKNOWN);
}

void test_imu_axes_give_gravity_on_the_screen(void)
{
    /* The AtomS3R's mounting: held up, USB port down, its sensor's x reads -1 g, the reaction to gravity; on its left
     * side, its y reads +1 g. Gravity on the screen points down (y), then left (-x). */
    const imu_axis atoms3r[3] = BOARD_IMU_AXES;
    const uint8_t upright[6] = {0x00u, 0xF0u, 0x00u, 0x00u, 0x00u, 0x00u};     /* x = -4096 */
    const uint8_t on_its_left[6] = {0x00u, 0x00u, 0x00u, 0x10u, 0x00u, 0x00u}; /* y = 4096 */
    const uint8_t extremes[6] = {0x00u, 0x80u, 0xFFu, 0x7Fu, 0xFFu, 0xFFu};    /* x -32768, y 32767, z -1 */
    float g[3];
    imu_axes_to_screen(upright, atoms3r, 4096.0f, g);
    CHECK(g[0] == 0.0f && g[1] == 1.0f && g[2] == 0.0f);
    imu_axes_to_screen(on_its_left, atoms3r, 4096.0f, g);
    CHECK(g[0] == -1.0f && g[1] == 0.0f && g[2] == 0.0f);
    imu_axes_to_screen(extremes, atoms3r, 4096.0f, g);
    CHECK_NEAR(g[0], -32767.0f / 4096.0f, 1e-6f);
    CHECK(g[1] == 8.0f && g[2] == 1.0f / 4096.0f);
}

static int saves;    /* the writes look_save_if_due asked for */
static bool save_ok; /* what the fake flash answers */

static bool fake_save(const lp_look *look)
{
    (void)look;
    saves++;
    return save_ok;
}

void test_a_look_is_written_at_once_then_at_most_every_gap(void)
{
    const int64_t gap = LOOK_SAVE_GAP_US;
    const lp_look kept = {0xAE0002u, LP_PUPIL_DOT, {0xFF5252u, 0xFF5252u}};
    CHECK(look_same(&kept, &kept));
    /* Each field makes another look: one that differs in any of them is written */
    lp_look sclera = kept;
    lp_look pupil = kept;
    lp_look left = kept;
    lp_look other = kept;
    sclera.sclera = 0x7DB5F4u;
    pupil.pupil = LP_PUPIL_CAT;
    left.pupil_color[0] = 0x0964E7u;
    other.pupil_color[1] = 0x0964E7u;
    CHECK(!look_same(&kept, &sclera) && !look_same(&kept, &pupil) && !look_same(&kept, &left));
    CHECK(!look_same(&kept, &other));

    look_saving s = look_saving_start(&kept);
    saves = 0;
    save_ok = true;
    CHECK(look_save_if_due(&s, &kept, 9 * gap, fake_save) && saves == 0); /* the flash holds it: nothing to write */
    CHECK(look_save_if_due(&s, &other, 0, fake_save) && saves == 1);      /* the first new look, at once */
    CHECK(look_same(&s.kept, &other));
    CHECK(look_save_if_due(&s, &sclera, gap - 1, fake_save) && saves == 1); /* too soon after the last write: later */
    CHECK(look_save_if_due(&s, &sclera, gap, fake_save) && saves == 2);
    save_ok = false;
    CHECK(!look_save_if_due(&s, &pupil, 2 * gap, fake_save) && saves == 3);
    CHECK(look_same(&s.kept, &sclera)); /* the flash still holds the last one written */
    CHECK(look_save_if_due(&s, &pupil, 3 * gap - 1, fake_save) && saves == 3); /* tried again a gap after the try */
    save_ok = true;
    CHECK(look_save_if_due(&s, &pupil, 3 * gap, fake_save) && saves == 4);
    CHECK(look_save_if_due(&s, &pupil, 9 * gap, fake_save) && saves == 4); /* kept: not written again */
}

/* Each core draws its rows of a band and leaves the other's alone (the cores draw at once): together, the frame drawn
 * whole */
void test_frame_cores_share_the_rows_of_each_band(void)
{
    enum { SIZE = 128, ROWS = 7 }; /* the AtomS3R's screen; bands that do not divide the region, the last is partial */
    static lp_eyes eyes;
    static lp_renderer whole;
    static lp_renderer banded;
    static lp_row_scratch second;
    static uint16_t reference[SIZE * SIZE];
    static uint16_t frame[SIZE * SIZE];
    static uint16_t band[2][SIZE * ROWS]; /* what each core writes */
    lp_eyes_init(&eyes, 1);
    lp_eyes_set_emotion(&eyes, LP_EMOTION_LOVE, 0.0f); /* hearts too */
    lp_renderer_init(&whole);
    lp_renderer_init(&banded);
    lp_renderer_set_size(&whole, SIZE);
    lp_renderer_set_size(&banded, SIZE);
    for (int f = 0; f < 60; f++) {
        lp_eyes_update(&eyes, 1.0f / 60.0f);
        lp_render(&whole, &eyes, reference);
        const lp_rect d = lp_render_begin(&banded, &eyes);
        const int width = d.x1 - d.x0;
        for (int y0 = d.y0; y0 < d.y1; y0 += ROWS) {
            const int y1 = y0 + ROWS < d.y1 ? y0 + ROWS : d.y1;
            for (int i = 0; i < SIZE * ROWS; i++)
                band[0][i] = band[1][i] = 0xABCDu; /* a row a core does not draw keeps it */
            const frame_band b[2] = {{y0, y1, width, band[0]}, {y0, y1, width, band[1]}};
            frame_draw_core_rows(&banded, 0, NULL, &b[0]);
            frame_draw_core_rows(&banded, 1, &second, &b[1]);
            for (int y = y0; y < y1; y++) {
                const int core = (y - y0) % 2;
                const uint16_t *other = &band[1 - core][(y - y0) * width];
                for (int x = 0; x < width; x++)
                    if (other[x] != 0xABCDu)
                        LP_FAIL("frame %d: both cores drew row %d", f, y);
                memcpy(&frame[y * SIZE + d.x0], &band[core][(y - y0) * width], (size_t)width * sizeof band[0][0]);
            }
        }
        if (memcmp(frame, reference, sizeof frame) != 0)
            LP_FAIL("frame %d: the two cores' rows differ from the frame drawn whole", f);
    }
}

void test_frame_measures_the_redrawn_share(void)
{
    CHECK(frame_area_permille((lp_rect){466, 466, 0, 0}, 466) == 0); /* empty: not a whole screen */
    CHECK(frame_area_permille((lp_rect){0, 466, 466, 0}, 466) == 0); /* empty in y only: nothing, not negative */
    CHECK(frame_area_permille((lp_rect){466, 0, 0, 466}, 466) == 0); /* empty in x only */
    CHECK(frame_area_permille((lp_rect){0, 0, 466, 466}, 466) == 1000);
    CHECK(frame_area_permille((lp_rect){0, 0, 233, 466}, 466) == 500);
    CHECK(frame_area_permille((lp_rect){0, 0, 64, 128}, 128) == 500);
}

void test_dcs_range_is_big_endian_first_and_last(void)
{
    uint8_t bytes[4];
    dcs_range(6, 471, bytes); /* the round screen's whole width, past its 6-column offset */
    CHECK(bytes[0] == 0x00u && bytes[1] == 0x06u && bytes[2] == 0x01u && bytes[3] == 0xD7u);
    dcs_range(2, 129, bytes); /* the AtomS3R's 128 columns, from 2 */
    CHECK(bytes[0] == 0x00u && bytes[1] == 0x02u && bytes[2] == 0x00u && bytes[3] == 0x81u);
}
