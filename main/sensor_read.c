#include "include/sensor_read.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <inttypes.h>
#include <rom/ets_sys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GPIO_OUTPUT_IO_0 18
#define GPIO_OUTPUT_PIN_SEL ((1ULL << GPIO_OUTPUT_IO_0))

#define GPIO_INPUT_IO_0 18
#define GPIO_INPUT_PIN_SEL ((1ULL << GPIO_INPUT_IO_0))

#define BIT_ONE_TRESHOLD 3
#define DHT11_RESPONSE_TIMEOUT 80

#define DHT11_READ_SUCCESS 0
#define DHT11_READ_FAIL 1

volatile static float c_humidity = 0.0f;
volatile static float c_temperature = 0.0f;

static const char* TAG = "sensor_read";

static int
dht11_read_bit()
{

    size_t us_period = 0;
    size_t us_polled = 0;

    while (gpio_get_level(GPIO_INPUT_IO_0) == 0 && us_polled < 1000) {
        ets_delay_us(10);
        us_polled += 1;
    }

    while (gpio_get_level(GPIO_INPUT_IO_0) == 1 && us_polled < 1000) {
        ets_delay_us(10);
        us_polled += 1;
        us_period += 1;
    }

    return us_period > BIT_ONE_TRESHOLD;
}

static uint8_t
dht11_read_byte()
{
    // ESP_LOGI(TAG, "Reading byte!");
    uint8_t byte = 0;
    for (int i = 7; i >= 0; --i) {
        byte |= (dht11_read_bit() << i);
    }
    return byte;
}

static int
dht11_wait_for_response()
{
    int cycles = 0;

    while (gpio_get_level(GPIO_INPUT_IO_0) == 0 && cycles < 200) {
        ets_delay_us(1);
        cycles++;
    }

    while (gpio_get_level(GPIO_INPUT_IO_0) == 1 && cycles < 200) {
        ets_delay_us(1);
        cycles++;
    }
    return cycles < 220;
}

static esp_err_t
set_as_output()
{
    return gpio_set_direction(GPIO_INPUT_IO_0, GPIO_MODE_OUTPUT);
}

static esp_err_t
set_as_input()
{
    return gpio_set_direction(GPIO_INPUT_IO_0, GPIO_MODE_INPUT);
}

static int
dht11_read_data()
{
    uint8_t data[5] = {};
    int sum = 0;

    ESP_LOGI(TAG, "Reading sensor data");
    set_as_output();
    gpio_set_level(GPIO_OUTPUT_IO_0, 0);

    vTaskDelay(30 / portTICK_PERIOD_MS);
    gpio_set_level(GPIO_OUTPUT_IO_0, 1);
    ets_delay_us(40);
    set_as_input();

    if (!dht11_wait_for_response()) {
        ESP_LOGI(TAG, "Failed to get response. Retrying.");
        return DHT11_READ_FAIL;
    }

    for (size_t i = 0; i < 200; ++i) {
        if (gpio_get_level(GPIO_INPUT_IO_0) == 1) {
            break;
        }
        ets_delay_us(1);
    }

    for (size_t i = 0; i < 5; ++i) {
        data[i] = dht11_read_byte();
    }

    for (int i = 0; i < 4; ++i) {
        sum += data[i];
    }

    if (sum == data[4]) {
        c_temperature = data[2] + (data[3] / 10.0f);
        c_humidity = data[0] + (data[1] / 10.0f);

        ESP_LOGI(TAG,
                 "Read sensor data SUCCESS! Temp: %fC Hum: %fRH",
                 c_temperature,
                 c_humidity);

        return DHT11_READ_SUCCESS;
    }
    ESP_LOGI(
      TAG, "Reading sensor data failed. Sum: %d, checkbit: %d", sum, data[3]);
    ESP_LOGI(TAG, "FAILED READ: temp: %dC hum: %dRH", data[0], data[2]);

    // set_as_output();
    // gpio_set_level(GPIO_OUTPUT_IO_0, 0);

    return DHT11_READ_FAIL;
}

void
read_values_task(void* data)
{
    while (1) {
        (void)data;
        if (dht11_read_data() == DHT11_READ_SUCCESS) {
            vTaskDelay(15000 / portTICK_PERIOD_MS);
        } else {
            vTaskDelay(3000 / portTICK_PERIOD_MS);
        }
    }
}

int
read_dht11(float* temperature, float* humidity)
{
    *temperature = c_temperature;
    *humidity = c_humidity;
    return 0;
}

TaskHandle_t
start_dht11_task()
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    TaskHandle_t task_handle;

    xTaskCreate(
      read_values_task, "read_dht11_task", 4096, NULL, 2, &task_handle);
    return task_handle;
}
