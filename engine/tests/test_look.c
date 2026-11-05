/* Looks as text: the format the PC app sends (app/src/device/protocol.ts) */
#include "lilpet/lilpet.h"
#include "test.h"

#include <string.h>

void test_look_text_round_trips(void)
{
    lp_look look;
    CHECK(lp_look_parse("B865A8 76D8EB 8F1716 acorn", &look));
    CHECK(look.sclera == 0xB865A8u && look.pupil_color[0] == 0x76D8EBu && look.pupil_color[1] == 0x8F1716u);
    CHECK(look.pupil == LP_PUPIL_ACORN);

    /* Lowercase, extra spaces and the line ending of a serial command are fine */
    CHECK(lp_look_parse("  ae0002 ff5252  ff5252 cat\r\n", &look));
    CHECK(look.sclera == 0xAE0002u && look.pupil == LP_PUPIL_CAT);

    const char *texts[] = {"AE0002 FF5252 FF5252 dot", "AE0002 FF5252 FF5252 cat", "AE0002 FF5252 FF5252 acorn",
                           "FFFFFF 000000 FFFFFF circle"};
    for (size_t i = 0; i < sizeof texts / sizeof texts[0]; i++) {
        char out[LP_LOOK_TEXT_SIZE];
        CHECK(lp_look_parse(texts[i], &look));
        CHECK(lp_look_format(&look, out, sizeof out) == (int)strlen(texts[i]));
        CHECK_STR_EQ(out, texts[i]);
    }

    look.pupil = (lp_pupil)7;
    char out[LP_LOOK_TEXT_SIZE];
    CHECK(lp_look_format(&look, out, sizeof out) == -1);
}

void test_every_pupil_type_has_a_name(void)
{
    /* A pupil added to the enum but forgotten in the table of names would crash, or be unreadable */
    for (int i = 0; i < LP_PUPIL_COUNT; i++) {
        const lp_look look = {0x123456u, (lp_pupil)i, {0x111111u, 0x222222u}};
        char out[LP_LOOK_TEXT_SIZE];
        CHECK(lp_look_format(&look, out, sizeof out) > 0);
        lp_look parsed;
        CHECK(lp_look_parse(out, &parsed));
        CHECK(parsed.pupil == (lp_pupil)i);
    }
}

void test_malformed_look_text_is_rejected(void)
{
    const lp_look before = {0x123456u, LP_PUPIL_DOT, {0x111111u, 0x222222u}};
    const char *bad[] = {
        "",
        "AE0002 FF5252 FF5252",          /* no pupil type */
        "AE0002 FF5252 dot",             /* one color missing */
        "AE000 FF5252 FF5252 dot",       /* five digits */
        "AE00021 FF5252 FF5252 dot",     /* seven digits */
        "AE0002 FF5252 FF52G2 dot",      /* not hex */
        "AE0002 FF5252 FF5252dot",       /* no space before the pupil type */
        "AE0002 FF5252 FF5252 slit",     /* old pupil name */
        "AE0002 FF5252 FF5252 dot more", /* extra word */
        "AE0002 FF5252 FF5252 do",
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        lp_look look = before;
        CHECK(!lp_look_parse(bad[i], &look));
        CHECK(memcmp(&look, &before, sizeof look) == 0);
    }
}
