
idf_component_register(SRCS "room_monitor_main.c" "wifi_config.c" "sensor_read.c" "bme680.c"
                    INCLUDE_DIRS "include"
                    REQUIRES mbedtls esp_wifi
                    PRIV_REQUIRES nvs_flash wpa_supplicant driver mqtt
                    )
