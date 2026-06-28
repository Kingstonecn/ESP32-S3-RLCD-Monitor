#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <driver/gpio.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#define KEY_GPIO GPIO_NUM_18   // side KEY button (active low, internal pullup)

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
    TaskHandle_t task_handle;  // usage_poll_task self, for wake-on-key
} poll_cfg_t;

// Shared between usage_poll_task and key_task: after 3 failed HTTP polls
// the poll task stops fetching (saves power). KEY press resets it.
static int g_poll_fail_count = 0;
// Pointer to cfg so key_task can wake usage_poll_task via task_handle.
// Set once in usage_poll_task startup.
static poll_cfg_t *g_poll_cfg = NULL;

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
 *
 *   4.12V+  100%
 *   4.02V    80%
 *   3.92V    60%
 *   3.82V    40%
 *   3.72V    25%
 *   3.62V    12%
 *   3.42V     0%  (cutoff)
 */
static int voltage_to_pct(float volt)
{
    if (volt >= 4.08f) return 100;
    if (volt >= 3.98f) return 80 + (int)((volt - 3.98f) / 0.10f * 20.0f);  // 80-100
    if (volt >= 3.88f) return 60 + (int)((volt - 3.88f) / 0.10f * 20.0f);  // 60-80
    if (volt >= 3.78f) return 40 + (int)((volt - 3.78f) / 0.10f * 20.0f);  // 40-60
    if (volt >= 3.68f) return 25 + (int)((volt - 3.68f) / 0.10f * 15.0f);  // 25-40
    if (volt >= 3.58f) return 12 + (int)((volt - 3.58f) / 0.10f * 13.0f);  // 12-25
    if (volt >= 3.38f) return (int)((volt - 3.38f) / 0.20f * 12.0f);       //  0-12
    return 0;
}

static int smooth_bat_pct = -1;  // exponential moving average

/** Return 0–100 battery percentage, or -1 if no battery detected.
 *  Samples multiple times and averages to smooth ADC noise; the caller is
 *  responsible for rate-limiting (battery voltage moves slowly, so we only
 *  call this every N polls — see usage_poll_task). */
static int battery_read_pct(void)
{
    if (!adc_handle) {
        ESP_LOGW(TAG, "battery: ADC not initialized");
        return -1;
    }
    // Average a few raw reads; the oneshot ADC has ±a few LSB jitter.
    long raw_sum = 0;
    int reads_ok = 0;
    for (int i = 0; i < 4; ++i) {
        int raw;
        if (adc_oneshot_read(adc_handle, BATTERY_ADC_CH, &raw) == ESP_OK) {
            raw_sum += raw;
            ++reads_ok;
        }
    }
    if (reads_ok == 0) {
        ESP_LOGW(TAG, "battery: ADC read failed");
        return -1;
    }
    int raw = (int)(raw_sum / reads_ok);
    int pin_mv;
    if (cali_handle && adc_cali_raw_to_voltage(cali_handle, raw, &pin_mv) == ESP_OK) {
        // calibrated millivolt at the ADC pin
    } else {
        // Fallback: 12-bit / 12dB atten → ~0–2900mV range
        pin_mv = (int)((float)raw / 4095.0f * 2900.0f);
    }
    int bat_mv = (int)((float)pin_mv * BATTERY_VOLT_DIV);
    float v = bat_mv / 1000.0f;
    ESP_LOGI(TAG, "battery: raw=%d pin=%dmV bat=%.2fV", raw, pin_mv, v);

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

// Key button (GPIO18): toggle DeepSeek / OpenCode view on press.
// Active-low with internal pullup. Polls at 50ms for debounce + edge detect.
static void key_task(void *arg)
{
    (void)arg;
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << KEY_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    int last = 1;  // pulled high = released
    for (;;) {
        int now = gpio_get_level(KEY_GPIO);
        if (last == 1 && now == 0) {  // falling edge = press
            static int m = 0;
            m ^= 1;
            // Also re-enable polling – reset the 3-failure guard
            g_poll_fail_count = 0;
            // Wake up usage_poll_task immediately so it fires a fetch now
            // instead of waiting up to 300s for the next delay expiry.
            if (g_poll_cfg && g_poll_cfg->task_handle) {
                xTaskNotifyGive(g_poll_cfg->task_handle);
            }
            if (Lvgl_lock(-1)) {
                ui_app_set_tracking_mode(m);
                Lvgl_unlock();
            }
        }
        last = now;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// Clock + indoor sensor + WiFi status.
// Time/WiFi update every 10s (clock must not lag visibly). SHTC3 temp/humidity
// drift slowly, so it's only read every 6 passes (≈60s) to keep the sensor and
// I2C bus idle between measurements.
static void clock_task(void *arg)
{
    (void) arg;
    float last_t = 0, last_h = 0;
    int shtc3_counter = 0;
    for (;;) {
        char hm[8];
        ntp_now_hm(hm, sizeof(hm));
        float t = last_t, h = last_h;
        bool ok = true;
        if (++shtc3_counter >= 6) {
            shtc3_counter = 0;
            float rt = 0, rh = 0;
            ok = (shtc3_read(&rt, &rh) == ESP_OK);
            if (ok) { last_t = rt; last_h = rh; t = rt; h = rh; }
        }
        bool wifi_up = false;
        esp_netif_t *n = esp_netif_get_handle_from_ifkey("STA_DEF");
        if (n) wifi_up = esp_netif_is_netif_up(n);
        if (Lvgl_lock(-1)) {
            ui_app_set_time(hm);
            ui_app_set_env(t, h, ok);
            ui_app_set_wifi(wifi_up);
            Lvgl_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

static void usage_poll_task(void *arg)
{
    poll_cfg_t *cfg = (poll_cfg_t *) arg;
    cfg->task_handle = xTaskGetCurrentTaskHandle();
    g_poll_cfg = cfg;  // expose so key_task can wake us
    // Battery voltage changes slowly; only sample every N polls to keep the
    // ADC (and its reference) idle. With RLCD_POLL_SEC=300 this reads ~10 min.
    // First poll always reads so the icon shows up immediately.
    int bat_counter = 0;
    for (;;) {
        // After 3 consecutive failures, stop polling to save power.
        // KEY press resets g_poll_fail_count AND notifies this task to
        // wake up immediately, so the next fetch fires without the 300s wait.
        if (g_poll_fail_count < 3) {
            usage_report_t rep;
            esp_err_t err = usage_client_fetch(cfg->url, cfg->token, &rep);
            if (Lvgl_lock(-1)) {
                if (err == ESP_OK) {
                    g_poll_fail_count = 0;
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
                    ++g_poll_fail_count;
                    ESP_LOGW(TAG, "fetch failed (%d/3): %s", g_poll_fail_count, esp_err_to_name(err));
                    if (g_poll_fail_count >= 3) {
                        ESP_LOGI(TAG, "3 consecutive failures, poll paused (KEY to resume)");
                    }
                    ui_app_mark_stale();
                }
                bool read_bat = (bat_counter == 0);
                if (++bat_counter >= 2) bat_counter = 0;
                ui_app_set_battery(read_bat ? battery_read_pct() : smooth_bat_pct, false);
                Lvgl_unlock();
            }
        }
        // Wait poll_sec seconds, but wake up early if KEY press notifies us.
        // This way the next fetch starts immediately instead of waiting for
        // the full delay to expire.
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(cfg->poll_sec * 1000));
    }
}

void UserApp_AppInit(const char *ssid, const char *password)
{
    ESP_LOGI(TAG, "connecting to '%s' ...", ssid);
    esp_err_t wl = wifi_app_connect_blocking(ssid, password);
    if (wl == ESP_ERR_TIMEOUT) {
        ESP_LOGW(TAG, "wifi link not up yet; continuing boot, poll task will retry");
    }
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
    xTaskCreatePinnedToCore(key_task, "key", 4 * 1024, NULL, 1, NULL, 1);
}
