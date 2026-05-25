// Monospace font + minimal palette for the reflective-LCD "terminal" look.
// Switch to a wider-glyph LVGL bin font (e.g. JetBrains Mono 18) once the
// vendor display demo confirms resolution.

#include "lvgl.h"

static lv_style_t s_mono;
static bool s_inited;

const lv_style_t *ui_mono_style(void)
{
    if (!s_inited) {
        lv_style_init(&s_mono);
        // Use built-in mono font until a custom bin font is added.
        lv_style_set_text_font(&s_mono, &lv_font_unscii_16);
        lv_style_set_text_color(&s_mono, lv_color_black());
        lv_style_set_bg_color(&s_mono, lv_color_white());
        lv_style_set_pad_all(&s_mono, 4);
        s_inited = true;
    }
    return &s_mono;
}
