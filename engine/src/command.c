/* Command lines of the text protocol (notes/app.md): the same on the USB port and on the camera link */
#include "lilpet/lilpet.h"

#include <stdlib.h>
#include <string.h>

#define MAX_WORDS   2       /* after the name of a command */
#define MAX_SECONDS 3600.0f /* longest temporary emotion */

bool lp_command_line_add(lp_command_line *line, char byte)
{
    if (byte == '\n' || byte == '\r') {
        line->text[line->length] = '\0';
        return true;
    }
    if (line->length < LP_COMMAND_LINE_MAX)
        line->text[line->length++] = byte;
    else
        line->too_long = true;
    return false;
}

/* A number from min to max; false on anything else, which covers nan and inf */
static bool parse_number(const char *word, float min, float max, float *out)
{
    char *end;
    const float value = strtof(word, &end);
    if (end == word || *end != '\0' || !(value >= min && value <= max))
        return false;
    *out = value;
    return true;
}

/* Cuts `text` into words, in place, and returns how many there are; only the first MAX_WORDS are kept */
static int split(char *text, char *words[MAX_WORDS])
{
    int count = 0;
    for (char *word = text + strspn(text, " "); *word != '\0'; word += strspn(word, " ")) {
        if (count < MAX_WORDS)
            words[count] = word;
        count++;
        word += strcspn(word, " ");
        if (*word != '\0')
            *word++ = '\0';
    }
    return count;
}

/* The parse_ functions fill `out`, and return NULL or what is wrong with the command */
static const char *parse_emotion(char *const words[], int count, lp_command *out)
{
    if (count < 1 || count > 2)
        return "malformed emotion, expected: <name> [seconds]";
    *out = (lp_command){.type = LP_COMMAND_EMOTION, .emotion = {.name = lp_emotion_from_name(words[0])}};
    if (out->emotion.name == LP_EMOTION_COUNT)
        return "unknown emotion";
    if (count == 2 &&
        (!parse_number(words[1], 0.0f, MAX_SECONDS, &out->emotion.seconds) || out->emotion.seconds == 0.0f))
        return "malformed emotion, seconds go from above 0 to 3600";
    return NULL;
}

static const char *parse_react(char *const words[], int count, lp_command *out)
{
    if (count != 1)
        return "malformed react, expected: <event>";
    *out = (lp_command){.type = LP_COMMAND_REACT, .event = lp_event_from_name(words[0])};
    return out->event == LP_EVENT_COUNT ? "unknown event" : NULL;
}

static const char *parse_gaze(char *const words[], int count, lp_command *out)
{
    *out = (lp_command){.type = LP_COMMAND_GAZE};
    if (count == 1 && strcmp(words[0], "off") == 0)
        return NULL;
    out->gaze.seen = true;
    if (count == 2 && parse_number(words[0], -1.0f, 1.0f, &out->gaze.x) &&
        parse_number(words[1], -1.0f, 1.0f, &out->gaze.y))
        return NULL;
    return "malformed gaze, expected: <x> <y> from -1 to 1, or off";
}

const char *lp_command_parse(const lp_command_line *line, lp_command *out)
{
    *out = (lp_command){.type = LP_COMMAND_NONE};
    if (strncmp(line->text, "> ", 2) != 0)
        return NULL;
    if (line->too_long)
        return "line too long";

    char text[LP_COMMAND_LINE_MAX + 1];
    memcpy(text, line->text, line->length + 1);
    char *name = text + 2;
    char *args = name + strcspn(name, " ");
    if (*args != '\0')
        *args++ = '\0';

    if (strcmp(name, "hello") == 0 && *args == '\0') {
        out->type = LP_COMMAND_HELLO;
    } else if (strcmp(name, "look?") == 0 && *args == '\0') {
        out->type = LP_COMMAND_LOOK_QUERY;
    } else if (strcmp(name, "imu?") == 0 && *args == '\0') {
        out->type = LP_COMMAND_IMU_QUERY;
    } else if (strcmp(name, "look") == 0) {
        out->type = LP_COMMAND_LOOK;
        if (!lp_look_parse(args, &out->look))
            return "malformed look, expected: RRGGBB RRGGBB RRGGBB dot|cat|acorn|circle";
    } else {
        char *words[MAX_WORDS];
        const int count = split(args, words);
        if (strcmp(name, "emotion") == 0)
            return parse_emotion(words, count, out);
        if (strcmp(name, "react") == 0)
            return parse_react(words, count, out);
        if (strcmp(name, "gaze") == 0)
            return parse_gaze(words, count, out);
        return "unknown command";
    }
    return NULL;
}
