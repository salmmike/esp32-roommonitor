
idf_component_register(SRCS "room_monitor_main.c" "wifi_config.c"
                    INCLUDE_DIRS "include"
                    REQUIRES mbedtls esp_wifi
                    PRIV_REQUIRES nvs_flash wpa_supplicant
                    )
