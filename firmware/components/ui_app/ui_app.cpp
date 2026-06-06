// Full-screen DeepSeek dashboard for RLCD 4.2 (400x300, 1-bit reflective).
// Header: time+weather | WiFi+battery, then FC/IN/RH.
// Body: centered DeepSeek balance + two-column TODAY|MONTH usage.
#include "ui_app.h"
#include "icons.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(font_amt14);   // Arial-Bold 14 (ascii + ° + ¥)
LV_FONT_DECLARE(font_bal28);   // DejaVuSans-Bold 28 (digits . ¥)

#define INK   lv_color_black()
#define WHITE lv_color_white()

// Header
static lv_obj_t *lbl_time_weather;   // "14:30 阴天" (28px)
static lv_obj_t *lbl_env;            // "FC 21°C / IN 26.3°C / RH 65%" (14px)
static lv_obj_t *img_wifi;
static lv_obj_t *img_battery;
static lv_obj_t *lbl_bat_pct;        // "85%"

// DeepSeek centered brand
static lv_obj_t *lbl_ds_bal;         // ¥ amount (bal28)

// Usage columns
static lv_obj_t *ds_hdr[2];   // "TODAY", "MONTH"
static lv_obj_t *ds_val[6];   // [today-tok, today-cost, today-cch, month-tok, month-cost, month-cch]

static bool have_data;
static float _fc_temp = -99.0f;  // last forecast temp from bridge
static float _fc_min = -99.0f, _fc_max = -99.0f;

static void fmt_tok(char *o, size_t n, int64_t t)
{
    if      (t >= 1000000000LL) snprintf(o, n, "%.1fB", t / 1e9);
    else if (t >= 10000000LL)   snprintf(o, n, "%.0fM", t / 1e6);
    else if (t >= 1000000LL)    snprintf(o, n, "%.1fM", t / 1e6);
    else if (t >= 1000LL)       snprintf(o, n, "%.0fk", t / 1e3);
    else                        snprintf(o, n, "%lld", (long long) t);
}

static lv_obj_t *mklabel(lv_obj_t *p, int x, int y, const lv_font_t *f, const char *t)
{
    lv_obj_t *l = lv_label_create(p);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, INK, 0);
    lv_obj_set_pos(l, x, y);
    lv_label_set_text(l, t);
    return l;
}
static lv_obj_t *mkalign(lv_obj_t *p, int left_x, int y, int w, lv_text_align_t a,
                         const lv_font_t *f, const char *t)
{
    lv_obj_t *l = lv_label_create(p);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, INK, 0);
    lv_obj_set_width(l, w);
    lv_obj_set_style_text_align(l, a, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(l, left_x, y);
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
static lv_obj_t *mkicon(lv_obj_t *p, int x, int y, const lv_image_dsc_t *src)
{
    lv_obj_t *im = lv_image_create(p);
    lv_image_set_src(im, src);
    lv_obj_set_pos(im, x, y);
    lv_obj_set_style_image_recolor(im, INK, 0);
    lv_obj_set_style_image_recolor_opa(im, LV_OPA_COVER, 0);
    return im;
}
static lv_obj_t *mkrawicon(lv_obj_t *p, int x, int y, const lv_image_dsc_t *src)
{
    lv_obj_t *im = lv_image_create(p);
    lv_image_set_src(im, src);
    lv_obj_set_pos(im, x, y);
    return im;
}

void ui_app_init(void)
{
    lv_obj_t *s = lv_screen_active();
    lv_obj_set_style_bg_color(s, WHITE, 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);

    // ---- header line 1: time+weather | WiFi battery ----
    lbl_time_weather = mklabel(s, 10, 4, &lv_font_montserrat_28, "--:--");
    img_wifi   = mkrawicon(s, 372, 4, &icon_wifi);
    img_battery = mkrawicon(s, 344, 4, &icon_bat_full);
    lbl_bat_pct = mkalign(s, 310, 8, 30, LV_TEXT_ALIGN_RIGHT, &lv_font_montserrat_14, "-");

    // ---- header line 2: FC / IN / RH ----
    lbl_env = mklabel(s, 10, 44, &lv_font_montserrat_14,
                      "FC --\xC2\xB0""C / IN --.-\xC2\xB0""C / RH --%");

    mkdiv(s, 10, 66, 380, 2);

    // ---- centered brand: icon + title + balance + ¥ ----
    mkicon(s, 120, 84, &icon_deepseek);
    mklabel(s, 160, 88, &lv_font_montserrat_20, "DEEPSEEK");
    mkalign(s, 100, 118, 200, LV_TEXT_ALIGN_CENTER, &lv_font_montserrat_20, "balance");
    lbl_ds_bal = mkalign(s, 100, 152, 200, LV_TEXT_ALIGN_CENTER, &font_bal28, "\xC2\xA5""0.00");

    mkdiv(s, 10, 190, 380, 1);

    // ---- usage: two-column (TODAY | MONTH) ----
    ds_hdr[0] = mkalign(s, 20, 200, 170, LV_TEXT_ALIGN_CENTER, &lv_font_montserrat_14, "TODAY");
    ds_hdr[1] = mkalign(s, 210, 200, 170, LV_TEXT_ALIGN_CENTER, &lv_font_montserrat_14, "MONTH");
    for (int i = 0; i < 2; ++i) {
        int bx = 20 + i * 190;
        ds_val[i*3+0] = mkalign(s, bx, 226, 170, LV_TEXT_ALIGN_CENTER, &font_amt14, "-");  // tokens
        ds_val[i*3+1] = mkalign(s, bx, 250, 170, LV_TEXT_ALIGN_CENTER, &font_amt14, "-");  // cost
        ds_val[i*3+2] = mkalign(s, bx, 274, 170, LV_TEXT_ALIGN_CENTER, &font_amt14, "-");  // cache
    }

    have_data = false;
}

void ui_app_update(const usage_report_t *r)
{
    if (!r) return;
    char tk[24], b[40];

    if (r->deepseek.valid) {
        snprintf(b, sizeof(b), "\xC2\xA5""%.2f", r->deepseek.balance);
        lv_label_set_text(lbl_ds_bal, b);

        // Only update usage data when tokens > 0 (real ccusage data)
        bool has_usage = (r->deepseek.today_tokens > 0 || r->deepseek.month_tokens > 0);
        if (has_usage) {
            // Update column header: "TODAY" or "YESTERDAY"
            lv_label_set_text(ds_hdr[0], r->deepseek.today_label);

            // today
            fmt_tok(tk, sizeof(tk), r->deepseek.today_tokens);
            snprintf(b, sizeof(b), "%s tokens", tk);
            lv_label_set_text(ds_val[0], b);
            snprintf(b, sizeof(b), "\xC2\xA5""%.2f", r->deepseek.today_cost_cny);
            lv_label_set_text(ds_val[1], b);
            if (r->deepseek.today_cache_pct >= 0)
                { snprintf(b, sizeof(b), "cache %.1f%%", r->deepseek.today_cache_pct); lv_label_set_text(ds_val[2], b); }

            // month
            fmt_tok(tk, sizeof(tk), r->deepseek.month_tokens);
            snprintf(b, sizeof(b), "%s tokens", tk);
            lv_label_set_text(ds_val[3], b);
            snprintf(b, sizeof(b), "\xC2\xA5""%.2f", r->deepseek.month_cost_cny);
            lv_label_set_text(ds_val[4], b);
            if (r->deepseek.month_cache_pct >= 0)
                { snprintf(b, sizeof(b), "cache %.1f%%", r->deepseek.month_cache_pct); lv_label_set_text(ds_val[5], b); }

            have_data = true;
        }
    }

    if (r->weather.valid) {
        _fc_temp = r->weather.temp_c;
        _fc_min = r->weather.temp_min;
        _fc_max = r->weather.temp_max;
        char b[64];
        if (_fc_min > -50.0f && _fc_max > -50.0f)
            snprintf(b, sizeof(b), "FC %.0f-%.0f\xC2\xB0""C / IN -- / RH --%%", _fc_min, _fc_max);
        else
            snprintf(b, sizeof(b), "FC %.0f\xC2\xB0""C / IN -- / RH --%%", _fc_temp);
        lv_label_set_text(lbl_env, b);
    }
}

void ui_app_set_env(float temp_c, float humidity, bool ok)
{
    char b[64];
    if (_fc_min > -50.0f && _fc_max > -50.0f && ok)
        snprintf(b, sizeof(b), "FC %.0f-%.0f\xC2\xB0""C / IN %.1f\xC2\xB0""C / RH %.0f%%",
                 _fc_min, _fc_max, temp_c, humidity);
    else if (_fc_min > -50.0f && _fc_max > -50.0f)
        snprintf(b, sizeof(b), "FC %.0f-%.0f\xC2\xB0""C / IN -- / RH --%%", _fc_min, _fc_max);
    else if (_fc_temp > -50.0f && ok)
        snprintf(b, sizeof(b), "FC %.0f\xC2\xB0""C / IN %.1f\xC2\xB0""C / RH %.0f%%",
                 _fc_temp, temp_c, humidity);
    else if (_fc_temp > -50.0f)
        snprintf(b, sizeof(b), "FC %.0f\xC2\xB0""C / IN -- / RH --%%", _fc_temp);
    else if (ok)
        snprintf(b, sizeof(b), "FC --\xC2\xB0""C / IN %.1f\xC2\xB0""C / RH %.0f%%", temp_c, humidity);
    else
        snprintf(b, sizeof(b), "FC --\xC2\xB0""C / IN -- / RH --%%");
    lv_label_set_text(lbl_env, b);
}

void ui_app_set_time(const char *hm)
{
    if (lbl_time_weather) {
        const char *curr = lv_label_get_text(lbl_time_weather);
        const char *desc = strchr(curr, ' ');
        if (desc) {
            char b[40];
            snprintf(b, sizeof(b), "%s%s", hm, desc);
            lv_label_set_text(lbl_time_weather, b);
        } else {
            lv_label_set_text(lbl_time_weather, hm);
        }
    }
}

void ui_app_set_weather_desc(const char *desc)
{
    if (lbl_time_weather && desc) {
        const char *curr = lv_label_get_text(lbl_time_weather);
        const char *existing = strchr(curr, ' ');
        char time_part[16];
        if (existing) {
            size_t len = existing - curr;
            if (len >= sizeof(time_part)) len = sizeof(time_part) - 1;
            strncpy(time_part, curr, len);
            time_part[len] = '\0';
        } else {
            strncpy(time_part, curr, sizeof(time_part) - 1);
            time_part[sizeof(time_part) - 1] = '\0';
        }
        char b[40];
        snprintf(b, sizeof(b), "%s %s", time_part, desc);
        lv_label_set_text(lbl_time_weather, b);
    }
}

void ui_app_set_wifi(bool connected)
{
    (void)connected;
    // Icon is always visible on 1-bit panel
}

void ui_app_set_battery(int level, bool charging)
{
    if (!img_battery) return;
    if (level < 0) {
        lv_obj_add_flag(img_battery, LV_OBJ_FLAG_HIDDEN);
        if (lbl_bat_pct) lv_label_set_text(lbl_bat_pct, "");
        return;
    }
    lv_obj_remove_flag(img_battery, LV_OBJ_FLAG_HIDDEN);
    const void *src = &icon_bat_full;
    if (charging)        src = &icon_bat_chg;
    else if (level < 20) src = &icon_bat_low;
    else if (level < 60) src = &icon_bat_med;
    lv_image_set_src(img_battery, src);

    if (lbl_bat_pct) {
        char b[12];
        snprintf(b, sizeof(b), "%d%%", level);
        lv_label_set_text(lbl_bat_pct, b);
    }
}

void ui_app_mark_stale(void)
{
    // keep last good values on 1-bit panel
}
