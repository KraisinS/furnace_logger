#ifndef TEMP_SENSOR_H
#define TEMP_SENSOR_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * MAX6675 Temperature Sensors (3 sensors)
 * - Uses GPIO bit-banged clock/data
 * - Measures 0°C to 1023.75°C
 * - 12-bit resolution: 0.25°C per bit
 * - Per-sensor GPIO assignment for SO, SCK and CS
 */

typedef struct {
    float temp_entrance; // Sensor 1
    float temp_middle;   // Sensor 2
    float temp_exit;     // Sensor 3
} temp_readings_t;

/**
 * @brief Initialize 3 MAX6675 sensors
 * @param s1_miso_pin Sensor 1 SO GPIO pin
 * @param s1_clk_pin Sensor 1 SCK GPIO pin
 * @param s1_cs_pin Sensor 1 CS pin
 * @param s2_miso_pin Sensor 2 SO GPIO pin
 * @param s2_clk_pin Sensor 2 SCK GPIO pin
 * @param s2_cs_pin Sensor 2 CS pin
 * @param s3_miso_pin Sensor 3 SO GPIO pin
 * @param s3_clk_pin Sensor 3 SCK GPIO pin
 * @param s3_cs_pin Sensor 3 CS pin
 * @return ESP_OK on success
 */
esp_err_t temp_sensor_init(int s1_miso_pin, int s1_clk_pin, int s1_cs_pin,
                           int s2_miso_pin, int s2_clk_pin, int s2_cs_pin,
                           int s3_miso_pin, int s3_clk_pin, int s3_cs_pin);

/**
 * @brief Read all 3 temperature sensors
 * @param readings Pointer to temp_readings_t structure
 * @return ESP_OK on success
 */
esp_err_t temp_sensor_read_all(temp_readings_t *readings);

/**
 * @brief Read single temperature sensor
 * @param sensor_id Sensor ID (1, 2, or 3)
 * @param temperature Pointer to temperature in Celsius
 * @return ESP_OK on success
 */
esp_err_t temp_sensor_read(int sensor_id, float *temperature);

/**
 * @brief Get sensor status
 * @param sensor_id Sensor ID (1, 2, or 3)
 * @return true if sensor is ready, false if thermocouple is disconnected
 */
bool temp_sensor_is_ready(int sensor_id);

/**
 * @brief Deinitialize temperature sensors
 * @return ESP_OK on success
 */
esp_err_t temp_sensor_deinit(void);

#endif // TEMP_SENSOR_H
