#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "usage_client.h"

// Build the screen (must be called inside Lvgl_lock).
void ui_app_init(void);

// Re-render with new data (callers MUST hold Lvgl_lock for the duration).
void ui_app_update(const usage_report_t *r);

// Mark current screen as stale (callers MUST hold Lvgl_lock).
void ui_app_mark_stale(void);

#ifdef __cplusplus
}
#endif
