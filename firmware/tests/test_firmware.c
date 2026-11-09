/* Pure parts of the firmware, run with the engine's host tests: which LCD controller answers, the motion sensor's axes,
 * when a look is written to flash, the band split between the cores and the share of the screen redrawn */
#include "../../engine/tests/test.h"
#include "../boards/atoms3r/board_config.h"
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
    CHECK(look_save_plan(&kept, &kept, 0, -gap) == LOOK_KEPT);           /* the flash holds it: nothing to write */
    CHECK(look_save_plan(&other, &kept, 0, -gap) == LOOK_SAVE_NOW);      /* the first new look, at once */
    CHECK(look_save_plan(&other, &kept, gap - 1, 0) == LOOK_SAVE_LATER); /* too soon after the last write */
    CHECK(look_save_plan(&other, &kept, gap, 0) == LOOK_SAVE_NOW);
    /* The one left for later is written once its turn comes, the last one sent */
    CHECK(!look_save_due(true, gap - 1, 0));
    CHECK(look_save_due(true, gap, 0));
    CHECK(!look_save_due(false, 9 * gap, 0));
}

void test_frame_splits_bands_and_measures_the_redrawn_share(void)
{
    /* Each core draws half of a band, the second core the row left over when they are odd */
    for (int rows = 1; rows <= 30; rows++) {
        const int first = frame_band_half(100, 100 + rows) - 100;
        const int second = rows - first;
        CHECK(first >= 0 && second - first >= 0 && second - first <= 1);
    }
    CHECK(frame_area_permille((lp_rect){466, 466, 0, 0}, 466) == 0); /* empty: not a whole screen */
    CHECK(frame_area_permille((lp_rect){0, 466, 466, 0}, 466) == 0); /* empty in y only: nothing, not negative */
    CHECK(frame_area_permille((lp_rect){466, 0, 0, 466}, 466) == 0); /* empty in x only */
    CHECK(frame_area_permille((lp_rect){0, 0, 466, 466}, 466) == 1000);
    CHECK(frame_area_permille((lp_rect){0, 0, 233, 466}, 466) == 500);
    CHECK(frame_area_permille((lp_rect){0, 0, 64, 128}, 128) == 500);
}
