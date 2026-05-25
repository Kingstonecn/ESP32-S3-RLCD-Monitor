// Terminal-style monospace dashboard. The whole screen is one wrapped
// label so refresh is dead-simple (1 lv_label_set_text per minute — no
// animation, no partial updates; plays nicely with the reflective LCD).

#include "ui_app.h"

#include "lvgl.h"
#include <stdio.h>
#include <string.h>

static lv_obj_t *s_label;
static char      s_buf[1024];
static bool      s_have_data;

static void fmt_tokens(char *out, size_t n, int64_t t)
{
    if      (t >= 1000000000LL) snprintf(out, n, "%5.1fB", t / 1e9);
    else if (t >= 1000000)      snprintf(out, n, "%5.1fM", t / 1e6);
    else if (t >= 1000)         snprintf(out, n, "%5.0fk", t / 1e3);
    else                        snprintf(out, n, "%5lld", (long long) t);
}

static void fmt_bar(char *out, size_t n, int pct_x100)
{
    int p = pct_x100 < 0 ? 0 : pct_x100 / 1000;  // 0..10 cells
    if (p > 10) p = 10;
    char *q = out;
    *q++ = '[';
    for (int i = 0; i < 10; ++i) *q++ = (i < p) ? '#' : '.';
    *q++ = ']';
    *q   = 0;
    (void) n;
}

void ui_app_init(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);

    s_label = lv_label_create(scr);
    lv_obj_set_style_text_font(s_label, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(s_label, lv_color_black(), 0);
    lv_obj_set_style_bg_color(s_label, lv_color_white(), 0);
    lv_obj_set_style_pad_all(s_label, 4, 0);
    lv_label_set_long_mode(s_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_label, lv_pct(100));
    lv_label_set_text(s_label, "claude-code @ rlcd ~ $ status\nwaiting for bridge...");
}

void ui_app_update(const usage_report_t *r)
{
    if (!s_label || !r) return;
    s_have_data = true;

    char tok_5h[16], tok_today[16], tok_month[16], tok_life[16];
    char bar5h[16],  barWk[16];

    fmt_tokens(tok_5h,    sizeof(tok_5h),    r->active_block.tokens_used);
    fmt_tokens(tok_today, sizeof(tok_today), r->today.tokens_used);
    fmt_tokens(tok_month, sizeof(tok_month), r->month.tokens_used);
    fmt_tokens(tok_life,  sizeof(tok_life),  r->lifetime.tokens_used);
    fmt_bar(bar5h, sizeof(bar5h), r->active_block.percent_used_x100);
    fmt_bar(barWk, sizeof(barWk), r->weekly.percent_used_x100);

    char pct5buf[8] = "  --", pctWbuf[8] = "  --";
    if (r->active_block.percent_used_x100 >= 0)
        snprintf(pct5buf, sizeof(pct5buf), "%3d%%", r->active_block.percent_used_x100 / 100);
    if (r->weekly.percent_used_x100 >= 0)
        snprintf(pctWbuf, sizeof(pctWbuf), "%3d%%", r->weekly.percent_used_x100 / 100);

    snprintf(s_buf, sizeof(s_buf),
        "claude-code @ rlcd ~ $ status%s\n"
        "------------------------------------------\n"
        "5h window  %s %s\n"
        "  %s tok   $%6.2f\n"
        "  reset in %ldm\n"
        "\n"
        "weekly     %s %s\n"
        "\n"
        "today    %s tok   $%7.2f\n"
        "month    %s tok   $%7.2f\n"
        "total    %s tok   $%7.2f\n",
        r->stale ? "  (stale)" : "",
        bar5h, pct5buf,
        tok_5h, r->active_block.cost_usd, (long) r->active_block.minutes_remaining,
        barWk, pctWbuf,
        tok_today, r->today.cost_usd,
        tok_month, r->month.cost_usd,
        tok_life,  r->lifetime.cost_usd);

    lv_label_set_text(s_label, s_buf);
}

void ui_app_mark_stale(void)
{
    if (!s_label) return;
    if (!s_have_data) {
        lv_label_set_text(s_label, "claude-code @ rlcd ~ $ status  (stale)\nbridge unreachable");
        return;
    }
    const char *cur = lv_label_get_text(s_label);
    if (cur && strstr(cur, "(stale)") == NULL) {
        char tmp[1100];
        snprintf(tmp, sizeof(tmp), "(stale)\n%s", cur);
        lv_label_set_text(s_label, tmp);
    }
}
