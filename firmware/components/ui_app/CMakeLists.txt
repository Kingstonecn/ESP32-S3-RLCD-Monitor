idf_component_register(
    SRCS "ui_app.cpp" "icons.c"
    INCLUDE_DIRS "./"
    REQUIRES lvgl__lvgl usage_client
    PRIV_REQUIRES app_bsp
)
