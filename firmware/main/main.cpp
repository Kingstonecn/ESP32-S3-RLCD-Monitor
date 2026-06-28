// rlcd-dashboard top-level entry. Mirrors the vendor 09_LVGL_V9_Test
// main.cpp structure, but UserApp_* now owns our terminal-style UI and
// the HTTP polling task instead of the gui-guider screens.

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include "nvs_flash.h"

#include "display_bsp.h"
#include "lvgl_bsp.h"
#include "user_app.h"
#include "user_config.h"
#include "secrets.h"

DisplayPort RlcdPort(RLCD_MOSI_PIN, RLCD_SCK_PIN, RLCD_DC_PIN, RLCD_CS_PIN, RLCD_RST_PIN, LCD_WIDTH, LCD_HEIGHT);

// Post-flush callback: send the 1-bit framebuffer to the panel once per
// render cycle, after all dirty-tile flushes have written their pixels.
static void Rlcd_PostFlushCb(void)
{
    RlcdPort.RLCD_Display();
}

static void Lvgl_FlushCallback(lv_display_t *drv, const lv_area_t *area, uint8_t *color_map)
{
    // Convert the dirty rectangle from RGB565 → 1-bit into DispBuffer.
    // with PARTIAL mode this is a small fraction of the full screen.
    uint16_t *buffer = (uint16_t *)color_map;
    for (int y = area->y1; y <= area->y2; y++) {
        for (int x = area->x1; x <= area->x2; x++) {
            uint8_t color = (*buffer < 0x7fff) ? ColorBlack : ColorWhite;
            RlcdPort.RLCD_SetPixel(x, y, color);
            buffer++;
        }
    }
    // Do NOT call RLCD_Display here — the post-flush callback handles it
    // once per render cycle, avoiding redundant SPI sends when LVGL splits
    // the screen into multiple tiles.
    lv_disp_flush_ready(drv);
}

extern "C" void app_main(void)
{
    nvs_flash_init();

    // Bring up the display + UI first so placeholders show during WiFi connect.
    RlcdPort.RLCD_Init();
    Lvgl_PortInit(LCD_WIDTH, LCD_HEIGHT, Lvgl_FlushCallback);
    Lvgl_SetPostFlushCb(Rlcd_PostFlushCb);
    if (Lvgl_lock(-1)) {
        UserApp_UiInit();
        Lvgl_unlock();
    }

    UserApp_AppInit(RLCD_WIFI_SSID, RLCD_WIFI_PASSWORD);  // wifi (blocking) + ntp + shtc3
    UserApp_TaskInit(RLCD_BRIDGE_URL, RLCD_BRIDGE_TOKEN, RLCD_POLL_SEC);
}
