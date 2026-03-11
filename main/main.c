#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "temp_sensor.h"
#include "tm1637_display.h"

// Backward compatibility: if per-sensor MISO/CLK configs are not present yet,
// fall back to legacy shared SPI pin settings.
#ifndef CONFIG_MAX6675_SENSOR1_MISO_PIN
#define CONFIG_MAX6675_SENSOR1_MISO_PIN CONFIG_SPI_MISO_PIN
#endif
#ifndef CONFIG_MAX6675_SENSOR2_MISO_PIN
#define CONFIG_MAX6675_SENSOR2_MISO_PIN CONFIG_SPI_MISO_PIN
#endif
#ifndef CONFIG_MAX6675_SENSOR3_MISO_PIN
#define CONFIG_MAX6675_SENSOR3_MISO_PIN CONFIG_SPI_MISO_PIN
#endif
#ifndef CONFIG_MAX6675_SENSOR1_CLK_PIN
#define CONFIG_MAX6675_SENSOR1_CLK_PIN CONFIG_SPI_CLK_PIN
#endif
#ifndef CONFIG_MAX6675_SENSOR2_CLK_PIN
#define CONFIG_MAX6675_SENSOR2_CLK_PIN CONFIG_SPI_CLK_PIN
#endif
#ifndef CONFIG_MAX6675_SENSOR3_CLK_PIN
#define CONFIG_MAX6675_SENSOR3_CLK_PIN CONFIG_SPI_CLK_PIN
#endif

static const char *TAG = "OvenTempLogger";

static TaskHandle_t app_task_handle = NULL;

static void temperature_display_task(void *pvParameters) {
    ESP_LOGI(TAG, "Temperature display task started");
    ESP_LOGI(TAG, "Sensor read schedule (1 second stagger):");
    ESP_LOGI(TAG, "  Sensor 1: read at T+0s, T+3s, T+6s, ...");
    ESP_LOGI(TAG, "  Sensor 2: read at T+1s, T+4s, T+7s, ...");
    ESP_LOGI(TAG, "  Sensor 3: read at T+2s, T+5s, T+8s, ...");

    const TickType_t stagger_interval = pdMS_TO_TICKS(3000); // 3 seconds/cycle
    const TickType_t sensor_offsets[3] = {
        pdMS_TO_TICKS(0),
        pdMS_TO_TICKS(1000),
        pdMS_TO_TICKS(2000),
    };
    const TickType_t check_interval = pdMS_TO_TICKS(50);

    TickType_t now = xTaskGetTickCount();
    TickType_t next_due[3] = {
        now + sensor_offsets[0],
        now + sensor_offsets[1],
        now + sensor_offsets[2],
    };

    while (1) {
        TickType_t current_time = xTaskGetTickCount();

        // Read each sensor at its own due time: T+0s, T+1s, T+2s, then repeat.
        for (int i = 0; i < 3; i++) {
            if ((int32_t)(current_time - next_due[i]) >= 0) {
                int sensor_num = i + 1;
                float temperature = 0.0f;
                esp_err_t ret = temp_sensor_read(sensor_num, &temperature);

                if (ret != ESP_OK) {
                    ESP_LOGW(TAG, "Sensor %d read failed: %s", sensor_num,
                             esp_err_to_name(ret));
                    (void)tm1637_display_show_error(sensor_num);
                } else {
                    ESP_LOGI(TAG, "Sensor %d: %.2f C", sensor_num, temperature);
                    ret = tm1637_display_temperature(sensor_num, temperature);
                    if (ret != ESP_OK) {
                        ESP_LOGW(TAG, "Display %d update failed: %s",
                                 sensor_num, esp_err_to_name(ret));
                    }
                }

                next_due[i] += stagger_interval;
            }
        }

        vTaskDelay(check_interval);
    }
}

void app_main(void) {
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    printf("\n");
    printf("=====================================\n");
    printf("   3-Zone Oven Temperature Monitor\n");
    printf("    3x MAX6675 + 3x TM1637 v2.0\n");
    printf("    ESP32-WROOM-IE\n");
    printf("    ESP-IDF Version: %s\n", esp_get_idf_version());
    printf("=====================================\n");
    printf("Chip: ESP32 with %d CPU cores\n", chip_info.cores);
    printf("Flash: %u MB %s\n",
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? 2 : 0,
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded"
                                                         : "external");
    printf("Features: WiFi=%s, BLE=%s\n",
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "yes" : "no",
           (chip_info.features & CHIP_FEATURE_BLE) ? "yes" : "no");
    printf("=====================================\n\n");

    ESP_LOGI(TAG, "Starting initialization sequence...");

    ESP_LOGI(TAG, "Initializing 3x MAX6675 sensors...");
    esp_err_t ret = temp_sensor_init(
        CONFIG_MAX6675_SENSOR1_MISO_PIN, CONFIG_MAX6675_SENSOR1_CLK_PIN,
        CONFIG_MAX6675_SENSOR1_CS_PIN, CONFIG_MAX6675_SENSOR2_MISO_PIN,
        CONFIG_MAX6675_SENSOR2_CLK_PIN, CONFIG_MAX6675_SENSOR2_CS_PIN,
        CONFIG_MAX6675_SENSOR3_MISO_PIN, CONFIG_MAX6675_SENSOR3_CLK_PIN,
        CONFIG_MAX6675_SENSOR3_CS_PIN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize MAX6675 sensors: %s",
                 esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "Initializing 3x TM1637 displays...");
    ret = tm1637_display_init_all(
        CONFIG_TM1637_DISPLAY1_CLK_PIN, CONFIG_TM1637_DISPLAY1_DIO_PIN,
        CONFIG_TM1637_DISPLAY2_CLK_PIN, CONFIG_TM1637_DISPLAY2_DIO_PIN,
        CONFIG_TM1637_DISPLAY3_CLK_PIN, CONFIG_TM1637_DISPLAY3_DIO_PIN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize TM1637 displays: %s",
                 esp_err_to_name(ret));
        return;
    }

    ret = tm1637_display_set_brightness(CONFIG_TM1637_BRIGHTNESS);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set TM1637 brightness: %s",
                 esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "Initialization complete!");
    ESP_LOGI(TAG, "Configuration:");
    ESP_LOGI(TAG, "  - MAX6675 Sensor 1: MISO=%d CLK=%d CS=%d",
             CONFIG_MAX6675_SENSOR1_MISO_PIN, CONFIG_MAX6675_SENSOR1_CLK_PIN,
             CONFIG_MAX6675_SENSOR1_CS_PIN);
    ESP_LOGI(TAG, "  - MAX6675 Sensor 2: MISO=%d CLK=%d CS=%d",
             CONFIG_MAX6675_SENSOR2_MISO_PIN, CONFIG_MAX6675_SENSOR2_CLK_PIN,
             CONFIG_MAX6675_SENSOR2_CS_PIN);
    ESP_LOGI(TAG, "  - MAX6675 Sensor 3: MISO=%d CLK=%d CS=%d",
             CONFIG_MAX6675_SENSOR3_MISO_PIN, CONFIG_MAX6675_SENSOR3_CLK_PIN,
             CONFIG_MAX6675_SENSOR3_CS_PIN);
    ESP_LOGI(TAG, "  - TM1637 Displays:");
    ESP_LOGI(TAG, "    Display 1: CLK=%d DIO=%d",
             CONFIG_TM1637_DISPLAY1_CLK_PIN, CONFIG_TM1637_DISPLAY1_DIO_PIN);
    ESP_LOGI(TAG, "    Display 2: CLK=%d DIO=%d",
             CONFIG_TM1637_DISPLAY2_CLK_PIN, CONFIG_TM1637_DISPLAY2_DIO_PIN);
    ESP_LOGI(TAG, "    Display 3: CLK=%d DIO=%d",
             CONFIG_TM1637_DISPLAY3_CLK_PIN, CONFIG_TM1637_DISPLAY3_DIO_PIN);
    ESP_LOGI(TAG, "  - Brightness: %d", CONFIG_TM1637_BRIGHTNESS);
    ESP_LOGI(TAG, "  - Update interval: %d ms", CONFIG_SENSOR_READ_INTERVAL);

    ret = xTaskCreate(temperature_display_task, "TempDisplayTask", 3072, NULL,
                      5, &app_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create display task");
        return;
    }

    ESP_LOGI(
        TAG,
        "Application running: monitoring 3 oven zones with MAX6675 sensors.");
}
