#include "dht.h"

#include <stdio.h>
#include <rom/ets_sys.h>

#include "esp_log.h"

#define DHT_TIMER_INTERVAL 2
#define DHT_DATA_BITS 40
#define DHT_DATA_BYTES (DHT_DATA_BITS / 8)

static const char *TAG = "DHT";

// Private

static esp_err_t dht_await_pin_state(gpio_num_t pin, uint32_t timeout, int expected_pin_state, uint32_t *duration) {
    /* XXX dht_await_pin_state() should save pin direction and restore
     * the direction before return. however, the SDK does not provide
     * gpio_get_direction().
     */
    gpio_set_direction(pin, GPIO_MODE_INPUT);
    for (uint32_t i = 0; i < timeout; i += DHT_TIMER_INTERVAL) {
        // need to wait at least a single interval to prevent reading a jitter
        ets_delay_us(DHT_TIMER_INTERVAL);

        if (gpio_get_level(pin) == expected_pin_state) {
            if (duration) {
                *duration = i;
            }

            return ESP_OK;
        }
    }

    return ESP_ERR_TIMEOUT;
}

static inline esp_err_t dht_fetch_data(dht_sensor_type sensor_type, gpio_num_t pin, uint8_t data[DHT_DATA_BYTES]) {
    uint32_t low_duration;
    uint32_t high_duration;

    // Phase 'A' pulling signal low to initiate read sequence
    gpio_set_direction(pin, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(pin, 0);
    ets_delay_us(20000);
    gpio_set_level(pin, 1);

    // Step through Phase 'B', 40us
    esp_err_t status = dht_await_pin_state(pin, 40, 0, NULL);

    if (status != ESP_OK) {
        return status;
    }

    // Step through Phase 'C', 88us
    status = dht_await_pin_state(pin, 88, 1, NULL);

    if (status != ESP_OK) {
        return status;
    }

    // Step through Phase 'D', 88us
    status = dht_await_pin_state(pin, 88, 0, NULL);

    if (status != ESP_OK) {
        return status;
    }

    // Read in each of the 40 bits of data...
    for (int i = 0; i < DHT_DATA_BITS; i++) {
        status = dht_await_pin_state(pin, 65, 1, &low_duration);

        if (status != ESP_OK) {
            return status;
        }

        status = dht_await_pin_state(pin, 75, 0, &high_duration);

        if (status != ESP_OK) {
           return status;
        }

        uint8_t b = i / 8;
        uint8_t m = i % 8;

        if (!m) {
            data[b] = 0;
        }

        data[b] |= (high_duration > low_duration) << (7 - m);
    }

    return ESP_OK;
}

static inline int16_t dht_convert_data(dht_sensor_type sensor_type, uint8_t msb, uint8_t lsb) {
    int16_t data;

    if (sensor_type == DHT_11) {
        data = msb * 10;
    } else {
        data = msb & 0x7F;
        data <<= 8;
        data |= lsb;

        if (msb & BIT(7)) {
            data = -data;       // convert it to negative
        }
    }

    return data;
}

// Public

esp_err_t dht_read(const dht_t *dht_t, dht_reading *dht_reading) {
    if (dht_reading == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // CHECK_ARG(humidity || temperature);

    uint8_t data[DHT_DATA_BYTES] = { 0 };

    gpio_set_direction(dht_t->gpio, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(dht_t->gpio, 1);


    // PORT_ENTER_CRITICAL();
    esp_err_t result = dht_fetch_data(dht_t->type, dht_t->gpio, data);

    // if (result == ESP_OK)
        // PORT_EXIT_CRITICAL();

    /* restore GPIO direction because, after calling dht_fetch_data(), the
     * GPIO direction mode changes */
    gpio_set_direction(dht_t->gpio, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(dht_t->gpio, 1);

    if (result != ESP_OK)
        return result;

    if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF))
    {
        ESP_LOGE(TAG, "Checksum failed, invalid data received from sensor");
        return ESP_ERR_INVALID_CRC;
    }

    dht_reading->temperature = dht_convert_data(dht_t->gpio, data[2], data[3]) / 10.0;
    dht_reading->humidity = dht_convert_data(dht_t->gpio, data[0], data[1]) / 10.0;

    ESP_LOGD(TAG, "DHT temperature: %.2f humidity: %.2f", dht_reading->temperature, dht_reading->humidity);

    return ESP_OK;
}
