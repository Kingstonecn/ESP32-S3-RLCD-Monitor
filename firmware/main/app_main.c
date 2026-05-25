#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "drv/display.h"
#include "net/wifi.h"
#include "net/usage_client.h"
#include "ui/ui_main.h"

static const char *TAG = "rlcd";

#include "secrets.h"

static void usage_poll_task(void *arg)
{
    while (1) {
        usage_report_t rep;
        esp_err_t err = usage_client_fetch(RLCD_BRIDGE_URL, RLCD_BRIDGE_TOKEN, &rep);
        if (err == ESP_OK) {
            ui_main_update(&rep);
        } else {
            ESP_LOGW(TAG, "usage fetch failed: %s", esp_err_to_name(err));
            ui_main_mark_stale();
        }
        vTaskDelay(pdMS_TO_TICKS(RLCD_POLL_SEC * 1000));
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_LOGI(TAG, "boot");

    display_init();
    ui_main_init();

    wifi_connect_blocking(RLCD_WIFI_SSID, RLCD_WIFI_PASSWORD);

    xTaskCreate(usage_poll_task, "usage_poll", 8192, NULL, 5, NULL);
}
