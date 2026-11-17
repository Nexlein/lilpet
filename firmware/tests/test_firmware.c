/* Pure parts of the firmware, run with the engine's host tests: which LCD controller answers, the motion sensor's axes,
 * when a look is written to flash, the rows of a band each core draws, the share of the screen redrawn, the column and
 * row ranges the display drivers send, when the eyes rest and how they look then, when the battery counts as low and
 * the eyes look tired, and the AXP2101 power chip's register values */
#include "../../engine/tests/test.h"
#include "../boards/atoms3r/board_config.h"
#include "../main/axp2101.h"
#include "../main/battery_rule.h"
#include "../main/dcs.h"
#include "../main/frame.h"
#include "../main/imu_axes.h"
#include "../main/lcd_id.h"
#include "../main/look_saving.h"
#include "../main/rest.h"

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
    look_saving_mark_sent(&s);
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

void test_a_start_writes_only_the_looks_that_come_after_it(void)
{
    const lp_look saved = {0x7DB5F4u, LP_PUPIL_CAT, {0xC10787u, 0x8F1716u}};
    const lp_look fallback = {0xAE0002u, LP_PUPIL_DOT, {0xFF5252u, 0xFF5252u}};
    saves = 0;
    save_ok = true;

    /* A start that shows a look the flash does not hold (after a crash: the default one) leaves the flash as it is */
    look_saving s = look_saving_start(&saved);
    CHECK(look_save_if_due(&s, &fallback, 0, fake_save) && saves == 0);
    look_saving_mark_sent(&s); /* the same look, sent: now chosen, and written */
    CHECK(look_save_if_due(&s, &fallback, 1, fake_save) && saves == 1 && look_same(&s.kept, &fallback));

    /* A start that did not read the flash: the first look that comes is written, whatever it is (all zeros: black) */
    const lp_look black = {0};
    s = look_saving_start(NULL);
    CHECK(look_save_if_due(&s, &black, 0, fake_save) && saves == 1);
    look_saving_mark_sent(&s);
    CHECK(look_save_if_due(&s, &black, 1, fake_save) && saves == 2);
    CHECK(look_save_if_due(&s, &black, 9 * LOOK_SAVE_GAP_US, fake_save) && saves == 2); /* known from then on */
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

void test_the_eyes_rest_after_a_while_with_nothing_happening(void)
{
    const int64_t t0 = 1000;
    rest r = rest_start(t0);
    CHECK(rest_state_at(&r, t0) == REST_AWAKE);
    CHECK(rest_state_at(&r, t0 + REST_IDLE_US - 1) == REST_AWAKE);
    CHECK(rest_state_at(&r, t0 + REST_IDLE_US) == REST_DROWSY); /* the sleepy look first */
    CHECK(rest_state_at(&r, t0 + REST_IDLE_US + REST_DROWSY_US - 1) == REST_DROWSY);
    CHECK(rest_state_at(&r, t0 + REST_IDLE_US + REST_DROWSY_US) == REST_ASLEEP);

    /* Anything that happens wakes it, asleep or not, and the wait starts over from there */
    const int64_t t1 = t0 + 2 * REST_IDLE_US;
    rest_activity(&r, t1);
    CHECK(rest_state_at(&r, t1) == REST_AWAKE);
    CHECK(rest_state_at(&r, t1 + REST_IDLE_US - 1) == REST_AWAKE);

    rest_sleep_now(&r, t1); /* the test command: asleep at once, without the sleepy look */
    CHECK(rest_state_at(&r, t1) == REST_ASLEEP);
}

void test_handling_the_board_counts_from_its_last_move(void)
{
    const float flat[3] = {0.0f, 0.0f, 1.0f};
    const float noise[3] = {0.05f, -0.05f, 0.95f};
    const float tilted[3] = {0.2f, 0.0f, 0.98f};
    const float up[3] = {0.0f, -0.2f, 1.0f};
    CHECK(!rest_moved(flat, flat) && !rest_moved(flat, noise));  /* the sensor's noise */
    CHECK(rest_moved(flat, tilted) && rest_moved(tilted, flat)); /* either way, on any axis */
    CHECK(rest_moved(flat, up));
    /* A slow tilt: each frame moves little, but it adds up from the reference */
    const float step[3] = {0.1f, 0.0f, 1.0f};
    CHECK(!rest_moved(flat, step) && rest_moved(flat, tilted));

    /* The reference is where it was the last time it moved; the first reading is where it lies, not a move */
    rest r = rest_start(0);
    CHECK(!rest_handled(&r, tilted));
    CHECK(!rest_handled(&r, step) && rest_handled(&r, flat)); /* 0.1 g, then 0.2 g away from the first reading */
    CHECK(!rest_handled(&r, flat) && !rest_handled(&r, noise));
    CHECK(rest_handled(&r, up));
    const float falling[3] = {0.0f, 0.0f, 0.0f}; /* no gravity at all is where it lies too, as a first reading */
    rest dropped = rest_start(0);
    CHECK(!rest_handled(&dropped, falling) && rest_handled(&dropped, flat));
}

void test_the_eyes_look_sleepy_before_they_sleep_and_get_their_mood_back(void)
{
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    lp_eyes_set_emotion(&eyes, LP_EMOTION_LOVE, 0.0f); /* the mood */
    rest_show(&eyes, REST_DROWSY);
    CHECK(eyes.emotion == LP_EMOTION_SLEEPY && eyes.mood == LP_EMOTION_LOVE);
    for (int i = 0; i < REST_DROWSY_US / 50000; i++) /* the whole sleepy time, in frames of 50 ms */
        lp_eyes_update(&eyes, 0.05f);
    CHECK(eyes.emotion == LP_EMOTION_SLEEPY); /* still, when the screen goes off */
    rest_show(&eyes, REST_ASLEEP);
    CHECK(eyes.emotion == LP_EMOTION_SLEEPY);
    rest_show(&eyes, REST_AWAKE);
    CHECK(eyes.emotion == LP_EMOTION_LOVE && eyes.mood == LP_EMOTION_LOVE);

    /* Woken by a reaction: the reaction keeps its emotion */
    rest_show(&eyes, REST_DROWSY);
    lp_eyes_react(&eyes, LP_EVENT_TAP);
    rest_show(&eyes, REST_AWAKE);
    CHECK(eyes.emotion == LP_EMOTION_HAPPY && eyes.mood == LP_EMOTION_LOVE);
}

void test_a_low_battery_stays_low_until_it_charges_back(void)
{
    CHECK(!battery_low(4100, false) && !battery_low(BATTERY_LOW_MV, false));
    CHECK(battery_low(BATTERY_LOW_MV - 1, false));
    /* Low, it stays low between the two thresholds, so the eyes do not flicker between two moods */
    CHECK(battery_low(BATTERY_LOW_MV + 50, true) && battery_low(BATTERY_OK_MV - 1, true));
    CHECK(!battery_low(BATTERY_OK_MV, true));
    CHECK(!battery_low(-1, false) && !battery_low(-1, true)); /* no battery is never low */
}

void test_a_low_battery_makes_the_eyes_tired_until_it_charges_back(void)
{
    lp_eyes eyes;
    lp_eyes_init(&eyes, 1);
    lp_eyes_set_emotion(&eyes, LP_EMOTION_HAPPY, 0.0f);
    lp_emotion before = LP_EMOTION_NEUTRAL;
    battery_show(&eyes, false, true, &before);
    CHECK(eyes.mood == LP_EMOTION_TIRED && before == LP_EMOTION_HAPPY);
    battery_show(&eyes, true, true, &before); /* still low: nothing changes */
    CHECK(eyes.mood == LP_EMOTION_TIRED && before == LP_EMOTION_HAPPY);
    battery_show(&eyes, true, false, &before);
    CHECK(eyes.mood == LP_EMOTION_HAPPY);
    battery_show(&eyes, false, false, &before);
    CHECK(eyes.mood == LP_EMOTION_HAPPY);
    lp_eyes_set_emotion(&eyes, LP_EMOTION_TIRED, 0.0f); /* the app's pick, with the battery fine, stays */
    battery_show(&eyes, false, false, &before);
    CHECK(eyes.mood == LP_EMOTION_TIRED);
    lp_eyes_set_emotion(&eyes, LP_EMOTION_HAPPY, 0.0f);

    /* A mood the app picks while the battery is low stays once it charges */
    battery_show(&eyes, false, true, &before);
    lp_eyes_set_emotion(&eyes, LP_EMOTION_LOVE, 0.0f);
    battery_show(&eyes, true, false, &before);
    CHECK(eyes.mood == LP_EMOTION_LOVE);
}

void test_the_axp2101_charges_at_most_the_current_asked(void)
{
    /* 100 to 200 mA by 25, then by 100 up to 1000 (Zephyr's table): 200 mA is code 8 */
    CHECK(axp2101_charge_code(100) == 4 && axp2101_charge_code(125) == 5 && axp2101_charge_code(200) == 8);
    CHECK(axp2101_charge_code(300) == 9 && axp2101_charge_code(1000) == 16);
    /* Between two steps, the lower one: never more than asked */
    CHECK(axp2101_charge_code(199) == 7 && axp2101_charge_code(250) == 8 && axp2101_charge_code(399) == 9);
    CHECK(axp2101_charge_code(99) == 0 && axp2101_charge_code(0) == 0); /* under the first step: no charge */
    CHECK(axp2101_charge_code(5000) == 16);
    /* The register's other bits stay */
    CHECK(axp2101_charge_register(0xE9u, 200) == 0xE8u && axp2101_charge_register(0x00u, 200) == 0x08u);
}

void test_the_axp2101_measures_the_battery_and_not_its_temperature_pin(void)
{
    CHECK(axp2101_adc_register(0x02u) == 0x01u && axp2101_adc_register(0xFFu) == 0xFDu);
    CHECK(axp2101_adc_register(0x00u) == 0x01u && axp2101_adc_register(0x1Cu) == 0x1Du);
    CHECK(axp2101_detection_register(0x00u) == 0x01u && axp2101_detection_register(0xF0u) == 0xF1u);
    /* 13 bits of mV, over two registers: 0x10 0x68 is 4200 mV; the high register's top bits are not part of it */
    CHECK(axp2101_millivolts(0x10u, 0x68u) == 4200 && axp2101_millivolts(0xF0u, 0x68u) == 4200);
    CHECK(axp2101_millivolts(0x0Du, 0xACu) == 3500);
}
