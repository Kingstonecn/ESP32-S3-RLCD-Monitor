idf_component_register(
    SRCS "usage_client.c"
    INCLUDE_DIRS "./"
    PRIV_REQUIRES esp_http_client json
)
