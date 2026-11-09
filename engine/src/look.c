/* Looks: the default look, eye shape and pupil types of eyes/looks.json (looks_table.h), and looks as text, for the USB
 * protocol with the PC app (docs/protocole.md): "AE0002 FF5252 FF5252 dot" */
#include "internal.h"
#include "lilpet/lilpet.h"
#include "looks_table.h"

#include <string.h>

lp_look lp_default_look(void)
{
    return DEFAULT_LOOK;
}

lp_shape lp_default_shape(void)
{
    return DEFAULT_SHAPE;
}

const lp_pupils *lp_pupils_get(void)
{
    return &PUPILS;
}

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
    for (int i = 0; i < LP_PUPIL_COUNT; i++) {
        const char *name = PUPILS.types[i].name;
        if (strlen(name) == len && strncmp(s, name, len) == 0) {
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

int lp_look_format(const lp_look *look, char *out)
{
    const size_t pupil = (size_t)look->pupil;
    if (pupil >= LP_PUPIL_COUNT)
        return -1;
    char *p = put_color(out, look->sclera);
    p = put_color(p, look->pupil_color[0]);
    p = put_color(p, look->pupil_color[1]);
    const char *name = PUPILS.types[pupil].name;
    const size_t len = strlen(name);
    memcpy(p, name, len + 1);
    return (int)((size_t)(p - out) + len);
}
