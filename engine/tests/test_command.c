/* Command lines: what the USB port and the camera link read (notes/app.md) */
#include "lilpet/lilpet.h"
#include "test.h"

#include <string.h>

#define LOOK_LINE "> look AE0002 FF5252 FF5252 dot"

/* Feeds `size` bytes and an end of line: the line must end there, and nowhere before */
static lp_command_line line_of(const char *bytes, size_t size)
{
    lp_command_line line = {0};
    for (size_t i = 0; i < size; i++)
        CHECK(!lp_command_line_add(&line, bytes[i]));
    CHECK(lp_command_line_add(&line, '\n'));
    return line;
}

static const char *parse(const char *text, lp_command *out)
{
    const lp_command_line line = line_of(text, strlen(text));
    return lp_command_parse(&line, out);
}

static void check_accepted(const char *text, lp_command_type type, lp_command *out)
{
    const char *error = parse(text, out);
    if (error)
        LP_FAIL("\"%s\" refused: %s", text, error);
    CHECK(out->type == type);
}

static void check_refused(const char *text, const char *expected)
{
    lp_command c;
    const char *error = parse(text, &c);
    if (!error)
        LP_FAIL("\"%s\" accepted", text);
    else
        CHECK_STR_EQ(error, expected);
}

void test_command_line_ends_on_lf_or_cr(void)
{
    lp_command_line line = {0};
    lp_command c;
    CHECK(!lp_command_line_add(&line, '>'));
    CHECK(!lp_command_line_add(&line, ' '));
    CHECK(!lp_command_line_add(&line, 'w'));
    CHECK(lp_command_line_add(&line, '\r'));
    CHECK_STR_EQ(line.text, "> w");
    CHECK(lp_command_parse(&line, &c) != NULL);

    /* The \n of a \r\n is a line of its own, empty, which asks for nothing */
    line = (lp_command_line){0};
    CHECK(lp_command_line_add(&line, '\n'));
    CHECK_STR_EQ(line.text, "");
    CHECK(lp_command_parse(&line, &c) == NULL && c.type == LP_COMMAND_NONE);
}

void test_command_line_limit(void)
{
    char text[LP_COMMAND_LINE_MAX + 2];
    memset(text, ' ', sizeof text);
    memcpy(text, "> react tap", sizeof "> react tap" - 1);

    lp_command c;
    lp_command_line line = line_of(text, LP_COMMAND_LINE_MAX);
    CHECK(LP_COMMAND_LINE_MAX == 94);
    CHECK(!line.too_long);
    CHECK(lp_command_parse(&line, &c) == NULL && c.type == LP_COMMAND_REACT);

    line = line_of(text, LP_COMMAND_LINE_MAX + 1);
    CHECK(line.too_long);
    const char *error = lp_command_parse(&line, &c);
    CHECK(error != NULL);
    if (error)
        CHECK_STR_EQ(error, "line too long");
}

void test_command_line_drops_what_does_not_fit(void)
{
    char text[LP_COMMAND_LINE_MAX + 1 + sizeof LOOK_LINE];
    lp_command c;

    /* Right where the line stops fitting: the end must not pass for a command */
    memset(text, 'x', sizeof text);
    memcpy(text + LP_COMMAND_LINE_MAX + 1, LOOK_LINE, sizeof LOOK_LINE - 1);
    lp_command_line line = line_of(text, sizeof text - 1);
    CHECK(lp_command_parse(&line, &c) == NULL && c.type == LP_COMMAND_NONE);

    /* The same with a NUL in the part that fits, which ends the text and not the line */
    memcpy(text, "> wag", sizeof "> wag");
    line = line_of(text, sizeof text - 1);
    const char *error = lp_command_parse(&line, &c);
    CHECK(error != NULL);
    if (error)
        CHECK_STR_EQ(error, "line too long");
}

void test_command_reads_every_command(void)
{
    lp_command c;
    check_accepted("> hello", LP_COMMAND_HELLO, &c);
    check_accepted("> look?", LP_COMMAND_LOOK_QUERY, &c);
    check_accepted("> imu?", LP_COMMAND_IMU_QUERY, &c);

    check_accepted("> look B865A8 76D8EB 8F1716 acorn", LP_COMMAND_LOOK, &c);
    CHECK(c.look.sclera == 0xB865A8u && c.look.pupil == LP_PUPIL_ACORN);

    check_accepted("> emotion happy", LP_COMMAND_EMOTION, &c);
    CHECK(c.emotion.name == LP_EMOTION_HAPPY);
    CHECK_NEAR(c.emotion.seconds, 0.0f, 0.0f);
    check_accepted("> emotion   love   1.5  ", LP_COMMAND_EMOTION, &c);
    CHECK(c.emotion.name == LP_EMOTION_LOVE);
    CHECK_NEAR(c.emotion.seconds, 1.5f, 0.0f);
    check_accepted("> emotion sad 3600", LP_COMMAND_EMOTION, &c);
    CHECK_NEAR(c.emotion.seconds, 3600.0f, 0.0f);

    check_accepted("> react gesture_like", LP_COMMAND_REACT, &c);
    CHECK(c.event == LP_EVENT_GESTURE_LIKE);

    check_accepted("> gaze 0.5 -0.25", LP_COMMAND_GAZE, &c);
    CHECK(c.gaze.seen);
    CHECK_NEAR(c.gaze.x, 0.5f, 0.0f);
    CHECK_NEAR(c.gaze.y, -0.25f, 0.0f);
    check_accepted("> gaze 1 -1", LP_COMMAND_GAZE, &c);
    check_accepted("> gaze off", LP_COMMAND_GAZE, &c);
    CHECK(!c.gaze.seen);
}

void test_command_refuses_bad_input(void)
{
    static const struct {
        const char *text, *error;
    } CASES[] = {
        {"> ", "unknown command"},
        {"> wag", "unknown command"},
        {">  hello", "unknown command"},
        {"> hello  ", "unknown command"},
        {"> hello you", "unknown command"},
        {"> look? now", "unknown command"},
        {"> imu? now", "unknown command"},
        {"> HELLO", "unknown command"},
        {"> look", "malformed look, expected: RRGGBB RRGGBB RRGGBB dot|cat|acorn|circle"},
        {"> look 7DB5F4 C10787 slit", "malformed look, expected: RRGGBB RRGGBB RRGGBB dot|cat|acorn|circle"},
        {"> emotion", "malformed emotion, expected: <name> [seconds]"},
        {"> emotion happy 2 now", "malformed emotion, expected: <name> [seconds]"},
        {"> emotion furious", "unknown emotion"},
        {"> emotion happy 0", "malformed emotion, seconds go from above 0 to 3600"},
        {"> emotion happy -1", "malformed emotion, seconds go from above 0 to 3600"},
        {"> emotion happy 3601", "malformed emotion, seconds go from above 0 to 3600"},
        {"> emotion happy nan", "malformed emotion, seconds go from above 0 to 3600"},
        {"> emotion happy inf", "malformed emotion, seconds go from above 0 to 3600"},
        {"> emotion happy 2s", "malformed emotion, seconds go from above 0 to 3600"},
        {"> react", "malformed react, expected: <event>"},
        {"> react tap now", "malformed react, expected: <event>"},
        {"> react wag", "unknown event"},
        {"> gaze", "malformed gaze, expected: <x> <y> from -1 to 1, or off"},
        {"> gaze on", "malformed gaze, expected: <x> <y> from -1 to 1, or off"},
        {"> gaze 0.5", "malformed gaze, expected: <x> <y> from -1 to 1, or off"},
        {"> gaze 0 0 0", "malformed gaze, expected: <x> <y> from -1 to 1, or off"},
        {"> gaze 2 0", "malformed gaze, expected: <x> <y> from -1 to 1, or off"},
        {"> gaze 0 nan", "malformed gaze, expected: <x> <y> from -1 to 1, or off"},
    };
    for (size_t i = 0; i < sizeof CASES / sizeof CASES[0]; i++)
        check_refused(CASES[i].text, CASES[i].error);
}

void test_command_ignores_other_lines(void)
{
    const char *const LINES[] = {"", ">", "hello", ">hello", "< ok", "I (27) main_task: Calling app_main()"};
    for (size_t i = 0; i < sizeof LINES / sizeof LINES[0]; i++) {
        lp_command c;
        CHECK(parse(LINES[i], &c) == NULL);
        CHECK(c.type == LP_COMMAND_NONE);
    }
}
