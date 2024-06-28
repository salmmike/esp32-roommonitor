#ifndef SENSOR_READ_H
#define SENROR_READ_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

int
read_dht11(float* temperature, float* humidity);

TaskHandle_t
start_dht11_task();

#endif