#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// wifi connect (blocking)
void UserApp_AppInit(const char *ssid, const char *password);

// build LVGL screen (must hold Lvgl_lock when called)
void UserApp_UiInit(void);

// start HTTP polling task. token may be NULL/"" for no auth.
void UserApp_TaskInit(const char *bridge_url, const char *token, int poll_sec);

/**
 * 估算剩余续航时间（基于过去 ~6.7h 的放电速率）
 * @return -1: 数据不足, -2: >48h, -3: <1h, 0-48: 剩余小时数
 */
int battery_estimate_remaining(void);

#ifdef __cplusplus
}
#endif
