#include "temp_sensor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "max6675.h"

static const char *TAG = "TempSensor";

typedef struct {
    max6675_device_t sensors[3];
    StaticSemaphore_t mutex_buffer;
    SemaphoreHandle_t mutex;
    bool initialized;
} temp_sensor_state_t;

static temp_sensor_state_t sensor_state = {0};

esp_err_t temp_sensor_init(int s1_miso_pin, int s1_clk_pin, int s1_cs_pin,
                           int s2_miso_pin, int s2_clk_pin, int s2_cs_pin,
                           int s3_miso_pin, int s3_clk_pin, int s3_cs_pin) {
    ESP_LOGI(TAG, "Initializing 3 MAX6675 sensors");
    ESP_LOGI(TAG, "Sensor 1: MISO=%d CLK=%d CS=%d", s1_miso_pin, s1_clk_pin,
             s1_cs_pin);
    ESP_LOGI(TAG, "Sensor 2: MISO=%d CLK=%d CS=%d", s2_miso_pin, s2_clk_pin,
             s2_cs_pin);
    ESP_LOGI(TAG, "Sensor 3: MISO=%d CLK=%d CS=%d", s3_miso_pin, s3_clk_pin,
             s3_cs_pin);

    sensor_state.mutex =
        xSemaphoreCreateMutexStatic(&sensor_state.mutex_buffer);
    if (sensor_state.mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_FAIL;
    }

    // Initialize all 3 sensors
    esp_err_t ret;
    ret = max6675_init(&sensor_state.sensors[0], s1_cs_pin, s1_clk_pin,
                       s1_miso_pin);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Sensor 1 init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = max6675_init(&sensor_state.sensors[1], s2_cs_pin, s2_clk_pin,
                       s2_miso_pin);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Sensor 2 init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = max6675_init(&sensor_state.sensors[2], s3_cs_pin, s3_clk_pin,
                       s3_miso_pin);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Sensor 3 init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    sensor_state.initialized = true;
    ESP_LOGI(TAG, "All 3 temperature sensors initialized successfully");

    // *** STARTUP THERMOCOUPLE CONNECTIVITY TEST (Sensor 3 / CS=15 only) ***
    // Read 5 times with 300ms delay between reads to allow MAX6675 conversion.
    // If all reads are identical, thermocouple is likely disconnected.
    ESP_LOGI(TAG, "=== Sensor 3 (CS=15) Connectivity Test: 5x reads with 300ms "
                  "between ===");
    float test_temps[5] = {0};
    for (int i = 0; i < 5; i++) {
        esp_err_t ret =
            max6675_read_temperature(&sensor_state.sensors[2], &test_temps[i]);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "  Read[%d]: %.2f°C", i, test_temps[i]);
        } else {
            ESP_LOGE(TAG, "  Read[%d]: FAILED", i);
        }
        if (i < 4) {
            vTaskDelay(pdMS_TO_TICKS(300)); // Allow MAX6675 conversion time
        }
    }

    // Check if all reads are identical (stuck/disconnected thermocouple)
    bool all_same = true;
    for (int i = 1; i < 5; i++) {
        if (test_temps[i] != test_temps[0]) {
            all_same = false;
            break;
        }
    }

    if (all_same) {
        ESP_LOGW(
            TAG,
            "*** ALERT: Sensor 3 all 5 reads identical (%.2f°C) ***\n"
            "    Thermocouple may be DISCONNECTED or MAX6675 defective.\n"
            "    Verify: 1) Thermocouple plugged into MAX1 module\n"
            "             2) Thermocouple red/yellow wires correct polarity\n"
            "             3) MAX1 module receiving 3.3V power\n"
            "             4) GND connection to MAX1 is solid",
            test_temps[0]);
    } else {
        ESP_LOGI(TAG,
                 "*** OK: Sensor 3 reads vary (responsive thermocouple) ***");
    }
    ESP_LOGI(TAG, "=== End Connectivity Test ===\n");

    return ESP_OK;
}

esp_err_t temp_sensor_read_all(temp_readings_t *readings) {
    if (readings == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!sensor_state.initialized || sensor_state.mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(sensor_state.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire mutex");
        return ESP_FAIL;
    }

    esp_err_t ret1 = max6675_read_temperature(&sensor_state.sensors[0],
                                              &readings->temp_entrance);
    vTaskDelay(pdMS_TO_TICKS(100)); // 100ms delay between sensors

    esp_err_t ret2 = max6675_read_temperature(&sensor_state.sensors[1],
                                              &readings->temp_middle);
    vTaskDelay(pdMS_TO_TICKS(100)); // 100ms delay between sensors

    esp_err_t ret3 = max6675_read_temperature(&sensor_state.sensors[2],
                                              &readings->temp_exit);

    xSemaphoreGive(sensor_state.mutex);

    if (ret1 != ESP_OK) {
        ESP_LOGW(TAG, "Sensor 1 read failed: %s", esp_err_to_name(ret1));
    }
    if (ret2 != ESP_OK) {
        ESP_LOGW(TAG, "Sensor 2 read failed: %s", esp_err_to_name(ret2));
    }
    if (ret3 != ESP_OK) {
        ESP_LOGW(TAG, "Sensor 3 read failed: %s", esp_err_to_name(ret3));
    }

    // Return OK if at least one sensor succeeded
    if (ret1 == ESP_OK || ret2 == ESP_OK || ret3 == ESP_OK) {
        return ESP_OK;
    }

    return ESP_FAIL;
}

esp_err_t temp_sensor_read(int sensor_id, float *temperature) {
    if (temperature == NULL || sensor_id < 1 || sensor_id > 3) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!sensor_state.initialized || sensor_state.mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(sensor_state.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire mutex");
        return ESP_FAIL;
    }

    esp_err_t ret = max6675_read_temperature(
        &sensor_state.sensors[sensor_id - 1], temperature);

    vTaskDelay(pdMS_TO_TICKS(10)); // Small delay after read

    xSemaphoreGive(sensor_state.mutex);

    return ret;
}

bool temp_sensor_is_ready(int sensor_id) {
    if (!sensor_state.initialized || sensor_id < 1 || sensor_id > 3) {
        return false;
    }

    float temperature = 0.0f;
    if (temp_sensor_read(sensor_id, &temperature) != ESP_OK) {
        return false;
    }

    return true;
}

esp_err_t temp_sensor_deinit(void) {
    ESP_LOGI(TAG, "Deinitializing temperature sensors");
    if (sensor_state.mutex != NULL) {
        vSemaphoreDelete(sensor_state.mutex);
        sensor_state.mutex = NULL;
    }
    sensor_state.initialized = false;

    return ESP_OK;
}
