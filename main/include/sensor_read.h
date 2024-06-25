#ifndef SENSOR_READ_H
#define SENROR_READ_H

int
read_dht11(float* temperature, float* humidity);

int
start_dht11_task();

#endif