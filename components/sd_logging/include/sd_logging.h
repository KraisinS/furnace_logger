#ifndef SD_LOGGING_H
#define SD_LOGGING_H

#include "esp_err.h"
#include "temp_sensor.h"
#include <time.h>

/**
 * @brief Initialize SD card for data logging
 * @param miso_pin SD SPI MISO GPIO pin
 * @param mosi_pin SD SPI MOSI GPIO pin
 * @param clk_pin SD SPI CLK GPIO pin
 * @param cs_pin SD SPI CS GPIO pin
 * @return ESP_OK on success
 */
esp_err_t sd_logging_init(int miso_pin, int mosi_pin, int clk_pin, int cs_pin);

/**
 * @brief Log temperature readings to SD card with timestamp
 * @param readings Temperature readings from all three sensors
 * @return ESP_OK on success
 */
esp_err_t sd_logging_record_data(const temp_readings_t *readings);

/**
 * @brief Get SD card information (remaining space, etc.)
 * @param total_size Total card size in bytes
 * @param used_size Used space in bytes
 * @return ESP_OK on success
 */
esp_err_t sd_logging_get_info(uint64_t *total_size, uint64_t *used_size);

/**
 * @brief List all log files on SD card
 * @return ESP_OK on success
 */
esp_err_t sd_logging_list_files(void);

/**
 * @brief Delete old log files based on retention policy
 * @param days_to_keep Number of days of logs to keep
 * @return ESP_OK on success
 */
esp_err_t sd_logging_cleanup_old_files(int days_to_keep);

/**
 * @brief Deinitialize SD card
 * @return ESP_OK on success
 */
esp_err_t sd_logging_deinit(void);

#endif // SD_LOGGING_H
