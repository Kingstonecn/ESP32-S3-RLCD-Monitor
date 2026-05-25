#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

#include "user_app.h"
#include "wifi_app.h"
#include "usage_client.h"
#include "ui_app.h"
#include "lvgl_bsp.h"

static const char *TAG = "user_app";

typedef struct {
    char  url[160];
    char  token[80];
    int   poll_sec;
} poll_cfg_t;

static void usage_poll_task(void *arg)
{
    poll_cfg_t *cfg = (poll_cfg_t *) arg;
    for (;;) {
        usage_report_t rep;
        esp_err_t err = usage_client_fetch(cfg->url, cfg->token, &rep);
        if (Lvgl_lock(-1)) {
            if (err == ESP_OK) {
                ui_app_update(&rep);
            } else {
                ESP_LOGW(TAG, "fetch failed: %s", esp_err_to_name(err));
                ui_app_mark_stale();
            }
            Lvgl_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(cfg->poll_sec * 1000));
    }
}

void UserApp_AppInit(const char *ssid, const char *password)
{
    ESP_LOGI(TAG, "connecting to '%s' ...", ssid);
    wifi_app_connect_blocking(ssid, password);
}

void UserApp_UiInit(void)
{
    ui_app_init();
}

void UserApp_TaskInit(const char *bridge_url, const char *token, int poll_sec)
{
    poll_cfg_t *cfg = (poll_cfg_t *) calloc(1, sizeof(*cfg));
    strncpy(cfg->url,   bridge_url, sizeof(cfg->url)   - 1);
    if (token) strncpy(cfg->token, token, sizeof(cfg->token) - 1);
    cfg->poll_sec = poll_sec > 0 ? poll_sec : 60;
    xTaskCreatePinnedToCore(usage_poll_task, "usage_poll", 6 * 1024, cfg, 4, NULL, 1);
}
