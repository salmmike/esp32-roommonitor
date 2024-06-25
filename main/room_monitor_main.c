/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_smartconfig.h"
#include "esp_system.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "include/sensor_read.h"
#include "include/wifi_config.h"
#include "sdkconfig.h"
#include <inttypes.h>
#include <stdio.h>

#define CONFIG_BROKER_URL "mqtt://192.168.101.166"

void
print_chip_info(void)
{
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154)
             ? ", 802.15.4 (Zigbee/Thread)"
             : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n",
           flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded"
                                                         : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n",
           esp_get_minimum_free_heap_size());
}

esp_mqtt_client_handle_t
create_mqtt()
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL,
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);
    return client;
}

void
app_main(void)
{
    print_chip_info();
    initialise_wifi();
    start_dht11_task();
    float hum = 0;
    float temp = 0;
    vTaskDelay(30000 / portTICK_PERIOD_MS);
    esp_mqtt_client_handle_t mqtt_handle = create_mqtt();

    while (1) {
        read_dht11(&temp, &hum);
        printf("MQTT PUBLISH %fC, %f%%\n", temp, hum);
        char tempstr[50];
        char humstr[50];
        sprintf(tempstr, "%g", temp);
        sprintf(humstr, "%g", hum);

        esp_mqtt_client_publish(mqtt_handle, "/room/temperature", tempstr, 0, 0, 0);
        esp_mqtt_client_publish(mqtt_handle, "/room/humidity", humstr, 0, 0, 0);
        vTaskDelay(30000 / portTICK_PERIOD_MS);
    }
}
