#pragma once

#include "lvgl.h"

#define LVGL_TICK_PERIOD_MS    50    // 20→50ms, 无动画不需要 50Hz 唤醒
#define LVGL_TASK_MAX_DELAY_MS 200   // 500→200ms, 空闲时睡更长
#define LVGL_TASK_MIN_DELAY_MS 10    // 50→10ms, 有事时能及时处理

#define LVGL_PARTIAL_ROWS      40    // PARTIAL 模式每块行数 (400×40×2=32KB)

typedef void (*DispFlushCb)(lv_display_t * disp, const lv_area_t * area, uint8_t * color_p);

void Lvgl_PortInit(int width, int height,DispFlushCb flush_cb);
bool Lvgl_lock(int timeout_ms);
void Lvgl_unlock(void);

/** Register a callback fired once per lv_timer_handler() cycle after all
 *  flush tiles have been processed.  Used to trigger a single RLCD_Display()
 *  per render pass instead of calling it from each tile's flush callback. */
void Lvgl_SetPostFlushCb(void (*cb)(void));