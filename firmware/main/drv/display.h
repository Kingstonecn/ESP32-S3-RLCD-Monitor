#pragma once

#include "esp_err.h"
#include "lvgl.h"

// Initialise the RLCD panel and hand LVGL a registered display handle.
// PASTE THE WORKING INIT FROM:
//   waveshareteam/ESP32-S3-RLCD-4.2/02_Example/ESP-IDF/09_LVGL_V9_Test/main/
esp_err_t display_init(void);

// Width/height as actually reported by the vendor demo.
uint16_t display_width(void);
uint16_t display_height(void);
