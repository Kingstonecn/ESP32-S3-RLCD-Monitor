// PLACEHOLDER. See display.h — replace this with the LCD init sequence from
// waveshareteam/ESP32-S3-RLCD-4.2/02_Example/ESP-IDF/09_LVGL_V9_Test/main/
//
// We compile-stub it so the rest of the project links during dev.

#include "display.h"
#include "esp_log.h"

static const char *TAG = "display";
static uint16_t g_w = 480;   // adjust after running vendor demo
static uint16_t g_h = 272;

esp_err_t display_init(void)
{
    ESP_LOGE(TAG, "display_init: STUB — paste vendor init before flashing");
    // TODO: spi/i80 bus + esp_lcd_panel_io + esp_lcd_panel + lvgl_port_add_disp
    return ESP_OK;
}

uint16_t display_width(void)  { return g_w; }
uint16_t display_height(void) { return g_h; }
