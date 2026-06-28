#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include <esp_timer.h>
#include "lvgl_bsp.h"

static SemaphoreHandle_t lvgl_mux = NULL;
#define BYTES_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565))

// Post-flush callback — called once per lv_timer_handler() cycle after all
// flush tiles have been processed. Registered by main.cpp.
static void (*s_post_flush_cb)(void) = NULL;

static const char *TAG = "LvglPort";

static void Increase_lvgl_tick(void *arg)
{
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

bool Lvgl_lock(int timeout_ms)
{
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(lvgl_mux, timeout_ticks) == pdTRUE;
}

void Lvgl_unlock(void)
{
    assert(lvgl_mux && "bsp_display_start must be called first");
    xSemaphoreGive(lvgl_mux);
}

void Lvgl_SetPostFlushCb(void (*cb)(void))
{
    s_post_flush_cb = cb;
}

static void Lvgl_port_task(void *arg)
{
    uint32_t task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
    for (;;) {
        bool flushed = false;
        if (Lvgl_lock(-1)) {
            lv_timer_handler();
            Lvgl_unlock();
            flushed = true;
        }
        // One RLCD_Display per render cycle — NOT per flush tile.
        // LVGL may split a render into multiple tiles (PARTIAL mode), but
        // each tile writes pixels into DispBuffer via the flush callback.
        // We send the whole 1-bit buffer to the panel once here, so SPI
        // traffic = 15KB/cycle instead of 15KB × N tiles.
        if (flushed && s_post_flush_cb) s_post_flush_cb();
        if (task_delay_ms > LVGL_TASK_MAX_DELAY_MS) {
            task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < LVGL_TASK_MIN_DELAY_MS) {
            task_delay_ms = LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

void Lvgl_PortInit(int width, int height, DispFlushCb flush_cb)
{
    lvgl_mux = xSemaphoreCreateMutex();
    lv_init();
    lv_display_t *disp = lv_display_create(width, height);
    lv_display_set_flush_cb(disp, flush_cb);

    // Single partial buffer: 32KB = 400×40×2B.
    // PARTIAL mode: LVGL renders only dirty tiles (40 rows each) instead of
    // the full 300 rows every time.  For a small update like a clock digit
    // this renders just 1-2 tiles instead of the whole screen.
    size_t buf_size = width * LVGL_PARTIAL_ROWS * BYTES_PER_PIXEL;
    uint8_t *buf = (uint8_t *)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    assert(buf);
    lv_display_set_buffers(disp, buf, NULL, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

    ESP_LOGI(TAG, "LVGL init: %dx%d PARTIAL, buf=%zuB (%d rows, single)",
             width, height, buf_size, LVGL_PARTIAL_ROWS);

    ESP_LOGI(TAG, "Install LVGL tick timer (%dms)", LVGL_TICK_PERIOD_MS);
    esp_timer_create_args_t lvgl_tick_timer_args = {};
    lvgl_tick_timer_args.callback = &Increase_lvgl_tick;
    lvgl_tick_timer_args.name = "lvgl_tick";
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));

    xTaskCreatePinnedToCore(Lvgl_port_task, "LVGL", 8 * 1024, NULL, 5, NULL, 0);
}
