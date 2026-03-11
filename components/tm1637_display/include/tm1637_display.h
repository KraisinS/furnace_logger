#ifndef TM1637_DISPLAY_H
#define TM1637_DISPLAY_H

#include "esp_err.h"

/**
 * @brief Initialize all 3 TM1637 displays
 * @param clk1_pin Display 1 CLK GPIO pin
 * @param dio1_pin Display 1 DIO GPIO pin
 * @param clk2_pin Display 2 CLK GPIO pin
 * @param dio2_pin Display 2 DIO GPIO pin
 * @param clk3_pin Display 3 CLK GPIO pin
 * @param dio3_pin Display 3 DIO GPIO pin
 * @return ESP_OK on success
 */
esp_err_t tm1637_display_init_all(int clk1_pin, int dio1_pin, int clk2_pin,
                                  int dio2_pin, int clk3_pin, int dio3_pin);

/**
 * @brief Set brightness for all displays
 * @param brightness Brightness level (0-7)
 * @return ESP_OK on success
 */
esp_err_t tm1637_display_set_brightness(int brightness);

/**
 * @brief Display temperature on specific display
 * @param display_id Display ID (1, 2, or 3)
 * @param temperature_c Temperature in Celsius
 * @return ESP_OK on success
 */
esp_err_t tm1637_display_temperature(int display_id, float temperature_c);

/**
 * @brief Show error on specific display
 * @param display_id Display ID (1, 2, or 3)
 * @return ESP_OK on success
 */
esp_err_t tm1637_display_show_error(int display_id);

/**
 * @brief Deinitialize all displays
 * @return ESP_OK on success
 */
esp_err_t tm1637_display_deinit(void);

#endif // TM1637_DISPLAY_H
