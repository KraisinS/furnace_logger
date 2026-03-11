#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include "esp_wifi.h"
#include "temp_sensor.h"

/**
 * WiFi event callback types
 */
typedef void (*wifi_event_callback_t)(bool connected);

/**
 * @brief Initialize WiFi in station mode
 * @param ssid WiFi network SSID
 * @param password WiFi password
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_init(const char *ssid, const char *password);

/**
 * @brief Register WiFi event callback
 * @param callback Function to call on WiFi connection/disconnection
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_register_callback(wifi_event_callback_t callback);

/**
 * @brief Check if WiFi is connected
 * @return true if connected, false otherwise
 */
bool wifi_manager_is_connected(void);

/**
 * @brief Get current WiFi connection info
 * @param ip_addr Pointer to store IP address (as string)
 * @param rssi Pointer to store RSSI (signal strength)
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_get_info(char *ip_addr, int8_t *rssi);

/**
 * @brief Send temperature data to remote server/MQTT
 * @param readings Temperature readings to send
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_send_data(const temp_readings_t *readings);

/**
 * @brief Deinitialize WiFi
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_deinit(void);

#endif // WIFI_MANAGER_H
