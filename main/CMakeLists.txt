idf_component_register(
    SRCS "main.c" "src/display.c" "src/ble_beacon.c"
    INCLUDE_DIRS "." "include"
    PRIV_REQUIRES esp_driver_uart esp_driver_i2c bt nvs_flash
)