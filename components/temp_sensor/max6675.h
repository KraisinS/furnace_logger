#ifndef MAX6675_H
#define MAX6675_H

#include "driver/gpio.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * MAX6675 frame format
 * - 16-bit read cycle
 * - Bits 14..3: Temperature value (0.25C/LSB)
 * - Bit 2 (D2): Thermocouple open/fault indicator
 * - Bit 1 (D1): Device ID bit (always 0 for MAX6675)
 * - Bit 0 (D0): Three-state output bit (always 0)
 */

#define MAX6675_FAULT_BIT 0x0004
#define MAX6675_RESOLUTION 0.25f

typedef struct {
    gpio_num_t cs_pin;
    gpio_num_t clk_pin;
    gpio_num_t miso_pin;
    uint32_t read_count; // Number of reads performed by this device
} max6675_device_t;

/**
 * @brief Initialize MAX6675 GPIO interface
 * @param device Device context
 * @param cs_pin Chip Select pin
 * @param clk_pin Clock pin
 * @param miso_pin Data output pin from MAX6675 (SO)
 * @return ESP_OK on success
 */
esp_err_t max6675_init(max6675_device_t *device, int cs_pin, int clk_pin,
                       int miso_pin);

/**
 * @brief Read raw temperature data from MAX6675
 * @param device Device handle
 * @param raw_data Pointer to store raw 16-bit data
 * @return ESP_OK on success
 */
esp_err_t max6675_read_raw(max6675_device_t *device, uint16_t *raw_data);

/**
 * @brief Convert raw MAX6675 data to temperature in Celsius
 * @param raw_data Raw 16-bit data from sensor
 * @param temperature Pointer to store temperature in °C
 * @return ESP_OK on success, ESP_ERR_INVALID_RESPONSE if thermocouple open
 */
esp_err_t max6675_convert(uint16_t raw_data, float *temperature);

/**
 * @brief Read temperature in °C from MAX6675
 * @param device Device handle
 * @param temperature Pointer to store temperature
 * @return ESP_OK on success
 */
esp_err_t max6675_read_temperature(max6675_device_t *device,
                                   float *temperature);

/**
 * @brief Check if thermocouple is open (fault)
 * @param raw_data Raw 16-bit data
 * @return true if fault detected
 */
bool max6675_is_fault(uint16_t raw_data);

#endif // MAX6675_H
