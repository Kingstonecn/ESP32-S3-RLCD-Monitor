#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_netif.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include "user_app.h"
#include "wifi_app.h"
#include "ntp.h"
#include "shtc3.h"
#include "usage_client.h"
#include "ui_app.h"
#include "lvgl_bsp.h"

// Battery ADC — Waveshare ESP32-S3-RLCD-4.2: GPIO4 (ADC1_CH3), 3:1 divider
#define BATTERY_ADC_CH      ADC_CHANNEL_3    // GPIO4 = ADC1_CH3
#define BATTERY_VOLT_DIV    3.0f

static const char *TAG = "user_app";

typedef struct {
    char url[160];
    char token[80];
    int  poll_sec;
} poll_cfg_t;

// ---- Battery ADC (ESP32-S3 GPIO4, 3:1 divider, 18650 Li-Ion) ----
static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t         cali_handle = NULL;

static bool battery_adc_init(void)
{
    adc_oneshot_unit_init_cfg_t ucfg = {
        .unit_id = ADC_UNIT_1,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    if (adc_oneshot_new_unit(&ucfg, &adc_handle) != ESP_OK) {
        ESP_LOGW(TAG, "battery ADC unit init failed");
        return false;
    }
    adc_oneshot_chan_cfg_t ccfg = {
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    if (adc_oneshot_config_channel(adc_handle, BATTERY_ADC_CH, &ccfg) != ESP_OK) {
        ESP_LOGW(TAG, "battery ADC channel config failed");
        return false;
    }
    // Calibration (curve-fitting, ESP32-S3)
    adc_cali_curve_fitting_config_t calcfg = {
        .unit_id  = ADC_UNIT_1,
        .chan     = BATTERY_ADC_CH,
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    esp_err_t err = adc_cali_create_scheme_curve_fitting(&calcfg, &cali_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ADC cali init failed (%s), using raw fallback", esp_err_to_name(err));
    }
    return true;
}

/**
 * Convert 18650 Li-Ion voltage to approximate percentage.
 * Piecewise-linear fit to typical Li-Ion discharge curve.
 */
static int voltage_to_pct(float volt)
{
    if (volt >= 4.2f) return 100;
    if (volt >= 4.0f) return 50 + (int)((volt - 4.0f) / 0.2f * 50.0f);
    if (volt >= 3.6f) return 15 + (int)((volt - 3.6f) / 0.4f * 35.0f);
    if (volt >= 3.0f) return (int)((volt - 3.0f) / 0.6f * 15.0f);
    return 0;
}

static int smooth_bat_pct = -1;  // exponential moving average

/** Return 0–100 battery percentage, or -1 if no battery detected. */
static int battery_read_pct(void)
{
    if (!adc_handle) {
        ESP_LOGW(TAG, "battery: ADC not initialized");
        return -1;
    }
    int raw;
    if (adc_oneshot_read(adc_handle, BATTERY_ADC_CH, &raw) != ESP_OK) {
        ESP_LOGW(TAG, "battery: ADC read failed");
        return -1;
    }
    int pin_mv;
    if (cali_handle && adc_cali_raw_to_voltage(cali_handle, raw, &pin_mv) == ESP_OK) {
        // calibrated millivolt at the ADC pin
    } else {
        // Fallback: 12-bit / 12dB atten → ~0–2900mV range
        pin_mv = (int)((float)raw / 4095.0f * 2900.0f);
    }
    int bat_mv = (int)((float)pin_mv * BATTERY_VOLT_DIV);
    float v = bat_mv / 1000.0f;
    ESP_LOGD(TAG, "battery: raw=%d pin=%dmV bat=%.2fV", raw, pin_mv, v);

    // < 2.5V → no battery connected (pulldown to GND via voltage divider)
    if (v < 2.5f) {
        ESP_LOGI(TAG, "battery: below 2.5V, hiding icon");
        return -1;
    }
    int pct = voltage_to_pct(v);
    // Simple exponential smoothing
    if (smooth_bat_pct < 0) smooth_bat_pct = pct;
    else smooth_bat_pct = (int)(0.4f * pct + 0.6f * smooth_bat_pct);
    ESP_LOGD(TAG, "battery: %.2fV → %d%% (smooth %d%%)", v, pct, smooth_bat_pct);
    return smooth_bat_pct;
}

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
            ui_app_set_battery(battery_read_pct(), false);
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
    battery_adc_init();
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
