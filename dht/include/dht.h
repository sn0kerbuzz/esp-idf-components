#pragma once

#include "driver/gpio.h"

typedef enum {
    DHT_11,
    DHT_22
} dht_sensor_type;

typedef struct {
    gpio_num_t gpio;
    dht_sensor_type type;
} dht_t;

typedef struct {
    float temperature;
    float humidity;
} dht_reading;

esp_err_t dht_read(const dht_t *dht_device, dht_reading *reading);