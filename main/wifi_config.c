/* Esptouch example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "include/wifi_config.h"
#include "esp_eap_client.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "wifi_config.h"
#include <stdlib.h>
#include <string.h>

#define WIFI_STORAGE_NAME "wifi_storage"
#define WIFI_PASSWORD "wifi_password"
#define WIFI_SSID "wifi_ssid"

static EventGroupHandle_t s_wifi_event_group;

static const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;
static const char* TAG = "smartconfig";

static void
smartconfig_task(void* parm);

uint8_t*
get_wifi_nvs_str(const char* value_name)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;
    uint8_t* value = NULL;
    size_t required_size = 0;

    err = nvs_open(WIFI_STORAGE_NAME, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return NULL;
    }

    // Read the size of memory space required for blob
    err = nvs_get_str(nvs_handle, value_name, NULL, &required_size);
    if (required_size == 0 || err != ESP_OK) {
        nvs_close(nvs_handle);
        return NULL;
    }

    // Read previously saved blob if available
    value = malloc(required_size + sizeof(char));
    if (required_size > 0) {
        err = nvs_get_str(nvs_handle, value_name, (char*)value, &required_size);
        if (err != ESP_OK) {
            free(value);
            value = NULL;
        }
    }
    ESP_LOGI(TAG, "Getting %s: %s, %d", value_name, value, value == NULL);

    nvs_close(nvs_handle);
    return value;
}

int
set_wifi_nvs_str(const char* value_name, const uint8_t* value)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    ESP_LOGI(TAG, "Storing %s: %s", value_name, value);

    err = nvs_open(WIFI_STORAGE_NAME, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
        return err;

    err = nvs_set_str(nvs_handle, value_name, (char*)value);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        return err;

    nvs_close(nvs_handle);
    return err;
}

int
store_wifi_password(const uint8_t* password)
{
    return set_wifi_nvs_str(WIFI_PASSWORD, password);
}

uint8_t*
get_wifi_password()
{
    return get_wifi_nvs_str(WIFI_PASSWORD);
}

int
store_wifi_ssid(const uint8_t* password)
{
    return set_wifi_nvs_str(WIFI_SSID, password);
}

uint8_t*
get_wifi_ssid()
{
    return get_wifi_nvs_str(WIFI_SSID);
}

int
wifi_connect(const uint8_t* ssid, const uint8_t* password)
{
    wifi_config_t wifi_config;

    bzero(&wifi_config, sizeof(wifi_config_t));
    memcpy(wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    memcpy(
      wifi_config.sta.password, password, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    esp_wifi_connect();
    return 0;
}

static void
event_handler(void* arg,
              esp_event_base_t event_base,
              int32_t event_id,
              void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        xTaskCreate(
          smartconfig_task, "smartconfig_example_task", 4096, NULL, 3, NULL);
    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
        ESP_LOGI(TAG, "Scan done");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
        ESP_LOGI(TAG, "Found channel");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
        ESP_LOGI(TAG, "Got SSID and password");

        smartconfig_event_got_ssid_pswd_t* evt =
          (smartconfig_event_got_ssid_pswd_t*)event_data;
        uint8_t ssid[33] = { 0 };
        uint8_t password[65] = { 0 };

        memcpy(ssid, evt->ssid, sizeof(evt->ssid));
        memcpy(password, evt->password, sizeof(evt->password));
        ESP_LOGI(TAG, "SSID:%s", ssid);
        ESP_LOGI(TAG, "PASSWORD:%s", password);
        store_wifi_ssid(ssid);
        store_wifi_password(password);
        wifi_connect(ssid, password);

    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
        xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
    }
}

void
initialise_wifi(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t* sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    uint8_t* ssid = get_wifi_ssid();
    uint8_t* password = get_wifi_password();

    if (ssid == NULL) {
        ESP_LOGI(TAG, "SSID is NULL. Fetch new!\n");
        ESP_ERROR_CHECK(esp_event_handler_register(
          WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
        ESP_ERROR_CHECK(esp_event_handler_register(
          IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));
        ESP_ERROR_CHECK(esp_event_handler_register(
          SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());

    } else {
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());

        wifi_connect(ssid, password);
        free(ssid);
        free(password);
    }
}

void
reset_wifi(void)
{
    store_wifi_ssid(NULL);
    store_wifi_password(NULL);
}

static void
smartconfig_task(void* parm)
{
    EventBits_t uxBits;
    ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));
    while (1) {
        uxBits = xEventGroupWaitBits(s_wifi_event_group,
                                     CONNECTED_BIT | ESPTOUCH_DONE_BIT,
                                     true,
                                     false,
                                     portMAX_DELAY);
        if (uxBits & CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi Connected to ap");
        }
        if (uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(TAG, "smartconfig over");
            esp_smartconfig_stop();
            vTaskDelete(NULL);
        }
    }
}
