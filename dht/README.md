# COMPONENT: DHT SENSOR

A DHT sensor driver for esp-idf.

## USAGE

```c
// Create the DHT sensor
dht_t dht = {
    .gpio = 1,
    .type = DHT_22
};

dht_reading reading;

while(true) {
    // Read the data from the DHT sensor.
    esp_err_t status = dht_read(&dht, &reading);

    if (status == ESP_OK) {
        ESP_LOGI(TAG, "Temperature: %f Humidity: %f", reading.temperature, reading.humidity);
    } else {
        ESP_LOGE(TAG, "%s", esp_err_to_name(status));
    }

    vTaskDelay(pdMS_TO_TICKS(2000));
}
```
