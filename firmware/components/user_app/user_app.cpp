#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_netif.h>

#include "user_app.h"
#include "wifi_app.h"
#include "ntp.h"
#include "shtc3.h"
#include "usage_client.h"
#include "ui_app.h"
#include "lvgl_bsp.h"

static const char *TAG = "user_app";

typedef struct {
    char url[160];
    char token[80];
    int  poll_sec;
} poll_cfg_t;

// Clock + indoor sensor + WiFi status: cheap, update every 10s.
static void clock_task(void *arg)
{
    (void) arg;
    float last_t = 0, last_h = 0;
    for (;;) {
        char hm[8];
        ntp_now_hm(hm, sizeof(hm));
        float t = 0, h = 0;
        bool ok = (shtc3_read(&t, &h) == ESP_OK);
        if (ok) { last_t = t; last_h = h; }
        else    { t = last_t; h = last_h; }
        bool wifi_up = false;
        esp_netif_t *n = esp_netif_get_handle_from_ifkey("STA_DEF");
        if (n) wifi_up = esp_netif_is_netif_up(n);
        if (Lvgl_lock(-1)) {
            ui_app_set_time(hm);
            ui_app_set_env(t, h, ok);
            ui_app_set_wifi(wifi_up);
            // Show battery icon and percentage for debugging
            ui_app_set_battery(-1, false);
            Lvgl_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

static void usage_poll_task(void *arg)
{
    poll_cfg_t *cfg = (poll_cfg_t *) arg;
    for (;;) {
        usage_report_t rep;
        esp_err_t err = usage_client_fetch(cfg->url, cfg->token, &rep);
        if (Lvgl_lock(-1)) {
            if (err == ESP_OK) {
                ui_app_update(&rep);
                // Update weather description in header line 1
                if (rep.weather.valid && rep.weather.condition[0]) {
                    ui_app_set_weather_desc(rep.weather.condition);
                }
                // Update env line with forecast temp from bridge
                if (rep.weather.valid) {
                    float t = 0, h = 0;
                    shtc3_read(&t, &h);  // best-effort
                    // ui_app_set_env will rebuild the string
                }
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
    ntp_start();
    if (shtc3_init() != ESP_OK) ESP_LOGW(TAG, "shtc3 init failed");
}

void UserApp_UiInit(void)
{
    ui_app_init();
}

void UserApp_TaskInit(const char *bridge_url, const char *token, int poll_sec)
{
    poll_cfg_t *cfg = (poll_cfg_t *) calloc(1, sizeof(*cfg));
    strncpy(cfg->url, bridge_url, sizeof(cfg->url) - 1);
    if (token) strncpy(cfg->token, token, sizeof(cfg->token) - 1);
    cfg->poll_sec = poll_sec > 0 ? poll_sec : 60;
    xTaskCreatePinnedToCore(usage_poll_task, "usage_poll", 6 * 1024, cfg, 4, NULL, 1);
    xTaskCreatePinnedToCore(clock_task, "clock", 4 * 1024, NULL, 3, NULL, 1);
}
