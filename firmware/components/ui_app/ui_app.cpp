// Two-column dashboard: header (time | indoor | Shenzhen weather),
// left CLAUDE (5h/7d bars + today/month/total), right DEEPSEEK (balance).
// Built once in ui_app_init(); update functions only set text/values so the
// reflective panel only redraws on the 60s poll. See docs/mockup.png.

#include "ui_app.h"
#include "icons.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

// The reflective panel is 1-bit (pure black/white, no grayscale). Anything
// that isn't solid black gets thresholded to white or breaks up into faint
// dithered strokes — so every glyph and rule is pure black. Hierarchy comes
// from font size, not color.
#define INK   lv_color_black()
#define WHITE lv_color_white()
#define GRAY  lv_color_black()

// dynamic widgets
static lv_obj_t *lbl_time, *lbl_indoor, *img_wx, *lbl_wx_temp, *lbl_wx_city;
static lv_obj_t *bar_5h, *bar_7d, *lbl_5h_pct, *lbl_7d_pct, *lbl_reset;
static lv_obj_t *c_tok[3], *c_cost[3];     // today/month/total: tokens + cost cols
static lv_obj_t *lbl_ds_bal, *ds_val[3];   // granted/topped/today values (right-aligned)
static bool have_data;

static void fmt_tok(char *o, size_t n, int64_t t)
{
    if      (t >= 1000000000LL) snprintf(o, n, "%.1fB", t / 1e9);
    else if (t >= 10000000LL)   snprintf(o, n, "%.0fM", t / 1e6);
    else if (t >= 1000000LL)    snprintf(o, n, "%.1fM", t / 1e6);
    else if (t >= 1000LL)       snprintf(o, n, "%.0fk", t / 1e3);
    else                        snprintf(o, n, "%lld", (long long) t);
}
static void fmt_cost(char *o, size_t n, double c)
{
    if      (c < 100)   snprintf(o, n, "$%.2f", c);
    else if (c < 10000) snprintf(o, n, "$%.0f", c);
    else                snprintf(o, n, "$%.1fk", c / 1000.0);
}

static lv_obj_t *mklabel(lv_obj_t *p, int x, int y, const lv_font_t *f, lv_color_t col, const char *t)
{
    lv_obj_t *l = lv_label_create(p);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, col, 0);
    lv_obj_set_pos(l, x, y);
    lv_label_set_text(l, t);
    return l;
}
static void mkdiv(lv_obj_t *p, int x, int y, int w, int h)
{
    lv_obj_t *d = lv_obj_create(p);
    lv_obj_remove_style_all(d);
    lv_obj_set_pos(d, x, y);
    lv_obj_set_size(d, w, h);
    lv_obj_set_style_bg_color(d, INK, 0);
    lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
}
static lv_obj_t *mkbar(lv_obj_t *p, int x, int y, int w)
{
    lv_obj_t *b = lv_bar_create(p);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, 12);
    lv_bar_set_range(b, 0, 100);
    lv_obj_set_style_radius(b, 6, 0);
    lv_obj_set_style_bg_color(b, WHITE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(b, INK, LV_PART_MAIN);
    lv_obj_set_style_border_width(b, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(b, 6, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(b, INK, LV_PART_INDICATOR);
    lv_bar_set_value(b, 0, LV_ANIM_OFF);
    return b;
}
// right-aligned label: text flows to a fixed right edge
static lv_obj_t *mkright(lv_obj_t *p, int right_x, int y, int w, const lv_font_t *f, const char *t)
{
    lv_obj_t *l = lv_label_create(p);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, INK, 0);
    lv_obj_set_width(l, w);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(l, right_x - w, y);
    lv_label_set_text(l, t);
    return l;
}

static lv_obj_t *mkicon(lv_obj_t *p, int x, int y, const lv_image_dsc_t *src)
{
    lv_obj_t *im = lv_image_create(p);
    lv_image_set_src(im, src);
    lv_obj_set_pos(im, x, y);
    lv_obj_set_style_image_recolor(im, INK, 0);
    lv_obj_set_style_image_recolor_opa(im, LV_OPA_COVER, 0);
    return im;
}

void ui_app_init(void)
{
    lv_obj_t *s = lv_screen_active();
    lv_obj_set_style_bg_color(s, WHITE, 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);

    // ---- header ----
    lbl_time   = mklabel(s, 10, 4, &lv_font_montserrat_28, INK, "--:--");
    lbl_indoor = mklabel(s, 12, 44, &lv_font_montserrat_14, GRAY, "IN --.-\xC2\xB0""C  --%");
    img_wx     = mkicon(s, 250, 8, &icon_wx_cloud);
    lbl_wx_temp = mklabel(s, 286, 12, &lv_font_montserrat_20, INK, "--\xC2\xB0""C");
    lbl_wx_city = mklabel(s, 230, 44, &lv_font_montserrat_14, GRAY, "SHENZHEN");
    mkdiv(s, 10, 64, 380, 2);

    // ---- vertical split ----
    mkdiv(s, 200, 74, 2, 210);

    // ---- left: CLAUDE ----
    mkicon(s, 12, 78, &icon_claudecode);
    mklabel(s, 40, 76, &lv_font_montserrat_16, INK, "CLAUDE");
    mklabel(s, 12, 102, &lv_font_montserrat_14, GRAY, "5h");
    bar_5h = mkbar(s, 40, 103, 96);
    lbl_5h_pct = mklabel(s, 146, 100, &lv_font_montserrat_14, INK, "--%");
    mklabel(s, 12, 124, &lv_font_montserrat_14, GRAY, "7d");
    bar_7d = mkbar(s, 40, 125, 96);
    lbl_7d_pct = mklabel(s, 146, 122, &lv_font_montserrat_14, INK, "--%");
    lbl_reset  = mklabel(s, 12, 146, &lv_font_montserrat_14, GRAY, "reset --");
    mkdiv(s, 12, 168, 178, 1);
    const char *crows[3] = {"today", "month", "total"};
    for (int i = 0; i < 3; ++i) {
        int y = 176 + i * 24;
        mklabel(s, 12, y, &lv_font_montserrat_14, INK, crows[i]);
        c_tok[i]  = mkright(s, 124, y, 64, &lv_font_montserrat_14, "-");   // tokens col
        c_cost[i] = mkright(s, 192, y, 62, &lv_font_montserrat_14, "-");   // cost col
    }

    // ---- right: DEEPSEEK ----
    mkicon(s, 212, 78, &icon_deepseek);
    mklabel(s, 240, 76, &lv_font_montserrat_16, INK, "DEEPSEEK");
    mklabel(s, 212, 102, &lv_font_montserrat_14, GRAY, "balance CNY");
    lbl_ds_bal = mklabel(s, 212, 118, &lv_font_montserrat_28, INK, "--");
    mkdiv(s, 212, 168, 178, 1);
    const char *drows[3] = {"granted", "topped", "today"};
    for (int i = 0; i < 3; ++i) {
        int y = 176 + i * 24;
        mklabel(s, 212, y, &lv_font_montserrat_14, INK, drows[i]);
        ds_val[i] = mkright(s, 388, y, 120, &lv_font_montserrat_14, "-");
    }
    have_data = false;
}

static const lv_image_dsc_t *wx_icon(const char *key)
{
    if (!strcmp(key, "clear"))  return &icon_wx_clear;
    if (!strcmp(key, "partly")) return &icon_wx_partly;
    if (!strcmp(key, "rain"))   return &icon_wx_rain;
    if (!strcmp(key, "snow"))   return &icon_wx_snow;
    if (!strcmp(key, "fog"))    return &icon_wx_fog;
    return &icon_wx_cloud;
}

void ui_app_update(const usage_report_t *r)
{
    if (!r) return;
    have_data = true;
    char tk[16], ct[16];

    // claude bars
    int p5 = r->limits.util_5h_x100, p7 = r->limits.util_7d_x100;
    if (p5 >= 0) { lv_bar_set_value(bar_5h, p5, LV_ANIM_OFF); char b[16]; snprintf(b, 16, "%d%%", p5); lv_label_set_text(lbl_5h_pct, b); }
    if (p7 >= 0) { lv_bar_set_value(bar_7d, p7, LV_ANIM_OFF); char b[16]; snprintf(b, 16, "%d%%", p7); lv_label_set_text(lbl_7d_pct, b); }
    { char b[40]; int m = r->limits.reset_5h_min;
      if (m >= 0) snprintf(b, sizeof(b), "reset in %dh%02dm", m / 60, m % 60);
      else        snprintf(b, sizeof(b), "reset --");
      lv_label_set_text(lbl_reset, b); }

    const usage_bucket_t *cb[3] = { &r->today, &r->month, &r->lifetime };
    for (int i = 0; i < 3; ++i) {
        fmt_tok(tk, sizeof(tk), cb[i]->tokens_used);  lv_label_set_text(c_tok[i], tk);
        fmt_cost(ct, sizeof(ct), cb[i]->cost_usd);    lv_label_set_text(c_cost[i], ct);
    }

    // deepseek
    if (r->deepseek.valid) {
        char b[24];
        snprintf(b, sizeof(b), "%.2f", r->deepseek.balance);  lv_label_set_text(lbl_ds_bal, b);
        snprintf(b, sizeof(b), "%.2f", r->deepseek.granted);  lv_label_set_text(ds_val[0], b);
        snprintf(b, sizeof(b), "%.2f", r->deepseek.topped);   lv_label_set_text(ds_val[1], b);
        fmt_tok(tk, sizeof(tk), r->deepseek.today_tokens);
        strncat(tk, " tok", sizeof(tk) - strlen(tk) - 1);     lv_label_set_text(ds_val[2], tk);
    }

    // weather (from bridge)
    if (r->weather.valid) {
        lv_image_set_src(img_wx, wx_icon(r->weather.icon));
        char b[16]; snprintf(b, sizeof(b), "%.0f\xC2\xB0""C", r->weather.temp_c);
        lv_label_set_text(lbl_wx_temp, b);
        char c[32]; snprintf(c, sizeof(c), "SHENZHEN  %s", r->weather.condition);
        lv_label_set_text(lbl_wx_city, c);
    }
}

void ui_app_set_env(float temp_c, float humidity, bool ok)
{
    char b[32];
    if (ok) snprintf(b, sizeof(b), "IN %.1f\xC2\xB0""C  %.0f%%", temp_c, humidity);
    else    snprintf(b, sizeof(b), "IN --");
    lv_label_set_text(lbl_indoor, b);
}

void ui_app_set_time(const char *hm)
{
    if (lbl_time) lv_label_set_text(lbl_time, hm);
}

void ui_app_mark_stale(void)
{
    if (lbl_time && have_data) {
        // subtle marker; keep last good values
        lv_obj_set_style_text_color(lbl_time, GRAY, 0);
    }
}
