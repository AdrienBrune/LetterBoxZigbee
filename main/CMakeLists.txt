idf_component_register(
    SRCS "main.cpp" "zigbee.cpp" "reed_sensor.cpp" "memory/pers_mem.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES nvs_flash esp_driver_uart esp_driver_gpio esp_adc ieee802154
)
