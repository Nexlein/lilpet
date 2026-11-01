/* Looks as text, for the USB protocol with the PC app (notes/app.md): "AE0002 FF5252 FF5252 dot" */
#include "lilpet/lilpet.h"

#include <string.h>

static const char *const PUPIL_NAMES[] = {
    [LP_PUPIL_DOT] = "dot",
    [LP_PUPIL_CAT] = "cat",
    [LP_PUPIL_ACORN] = "acorn",
    [LP_PUPIL_CIRCLE] = "circle",
};
#define PUPIL_COUNT (sizeof PUPIL_NAMES / sizeof PUPIL_NAMES[0])

static bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static const char *skip_spaces(const char *s)
{
    while (is_space(*s))
        s++;
    return s;
}

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

/* Exactly six hex digits, then a space or the end; advances *s past them */
static bool parse_color(const char **s, uint32_t *out)
{
    const char *p = skip_spaces(*s);
    uint32_t value = 0;
    for (int i = 0; i < 6; i++) {
        const int d = hex_digit(p[i]);
        if (d < 0)
            return false;
        value = value << 4u | (uint32_t)d;
    }
    if (p[6] != '\0' && !is_space(p[6]))
        return false;
    *s = p + 6;
    *out = value;
    return true;
}

bool lp_look_parse(const char *text, lp_look *look)
{
    lp_look parsed;
    const char *s = text;
    if (!parse_color(&s, &parsed.sclera) || !parse_color(&s, &parsed.pupil_color[0]) ||
        !parse_color(&s, &parsed.pupil_color[1]))
        return false;
    s = skip_spaces(s);
    size_t len = 0;
    while (s[len] != '\0' && !is_space(s[len]))
        len++;
    if (*skip_spaces(s + len) != '\0')
        return false;
    for (size_t i = 0; i < PUPIL_COUNT; i++) {
        if (strlen(PUPIL_NAMES[i]) == len && strncmp(s, PUPIL_NAMES[i], len) == 0) {
            parsed.pupil = (lp_pupil)i;
            *look = parsed;
            return true;
        }
    }
    return false;
}

/* Six uppercase hex digits and a space (no printf: the engine stays free of stdio) */
static char *put_color(char *p, uint32_t color)
{
    static const char HEX[] = "0123456789ABCDEF";
    for (unsigned shift = 24u; shift > 0u;) {
        shift -= 4u;
        *p++ = HEX[(color >> shift) & 0xFu];
    }
    *p++ = ' ';
    return p;
}

int lp_look_format(const lp_look *look, char *out, size_t size)
{
    const size_t pupil = (size_t)look->pupil;
    if (pupil >= PUPIL_COUNT)
        return -1;
    char text[LP_LOOK_TEXT_SIZE];
    char *p = put_color(text, look->sclera);
    p = put_color(p, look->pupil_color[0]);
    p = put_color(p, look->pupil_color[1]);
    const size_t name = strlen(PUPIL_NAMES[pupil]);
    memcpy(p, PUPIL_NAMES[pupil], name);
    const size_t len = (size_t)(p - text) + name;
    if (size > 0) {
        const size_t n = len < size - 1 ? len : size - 1;
        memcpy(out, text, n);
        out[n] = '\0';
    }
    return (int)len;
}
