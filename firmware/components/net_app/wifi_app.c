// Adapted from vendor 02_WIFI_STA demo. Same pattern + an event group so
// app_main can block until the first IP_EVENT_STA_GOT_IP fires.

#include "wifi_app.h"

#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_pm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char         *TAG                 = "wifi";
static EventGroupHandle_t  s_evt;
static const EventBits_t   BIT_CONNECTED       = BIT0;

static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_evt, BIT_CONNECTED);
        ESP_LOGW(TAG, "disconnected, retry");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *) data;
        ESP_LOGI(TAG, "got IP " IPSTR, IP2STR(&ev->ip_info.ip));
        xEventGroupSetBits(s_evt, BIT_CONNECTED);
    }
}

esp_err_t wifi_app_connect_blocking(const char *ssid, const char *password)
{
    s_evt = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_config_t wc = {0};
    strncpy((char *) wc.sta.ssid,     ssid,     sizeof(wc.sta.ssid)     - 1);
    strncpy((char *) wc.sta.password, password, sizeof(wc.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());

    xEventGroupWaitBits(s_evt, BIT_CONNECTED, pdFALSE, pdTRUE, portMAX_DELAY);

    // Enable Modem Sleep: radio sleeps between DTIM beacons, saves ~50-70mA
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM));
    ESP_LOGI(TAG, "Modem Sleep enabled (WIFI_PS_MIN_MODEM)");

    // Auto CPU freq scaling + light sleep when idle
    esp_pm_config_t pm = {
        .max_freq_mhz = 240,
        .min_freq_mhz = 80,
        .light_sleep_enable = false,
    };
    esp_err_t pm_err = esp_pm_configure(&pm);
    if (pm_err == ESP_OK) {
        ESP_LOGI(TAG, "PM: freq 80-240MHz, light sleep on");
    } else {
        ESP_LOGW(TAG, "PM failed (%s), light sleep unavailable", esp_err_to_name(pm_err));
    }
    return ESP_OK;
}
