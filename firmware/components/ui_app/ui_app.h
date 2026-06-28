#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "usage_client.h"

void ui_app_init(void);                       // build screen (hold Lvgl_lock)
void ui_app_update(const usage_report_t *r);  // data from bridge (hold lock)
void ui_app_set_env(float temp_c, float humidity, bool ok);  // SHTC3 (hold lock)
void ui_app_set_time(const char *hm);         // "14:30" (hold lock)
void ui_app_set_weather_desc(const char *desc); // "阴天" (hold lock)
void ui_app_set_wifi(bool connected);         // WiFi status (hold lock)
void ui_app_set_battery(int level, bool charging); // 0-100 (hold lock)
void ui_app_set_tracking_mode(int mode); // 0=DeepSeek, 1=OpenCode (hold lock)
void ui_app_mark_stale(void);                 // bridge unreachable (hold lock)

#ifdef __cplusplus
}
#endif
