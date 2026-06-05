idf_component_register(
    SRCS "ui_app.cpp" "icons.c" "icon_wifi.c" "icon_wifi_low.c" "icon_wifi_off.c"
         "icon_bat_full.c" "icon_bat_med.c" "icon_bat_low.c" "icon_bat_chg.c"
         "font_amt14.c" "font_bal28.c"
    INCLUDE_DIRS "./"
    REQUIRES lvgl__lvgl usage_client
    PRIV_REQUIRES app_bsp
)

# lv_font_conv output uses #include "lvgl.h" only under this define
target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE)
