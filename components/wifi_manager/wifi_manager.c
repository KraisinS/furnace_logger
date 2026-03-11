#include "wifi_manager.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_types.h"
#include "esp_wifi.h"
#include <string.h>

static const char *TAG = "WiFiManager";

typedef struct {
    bool initialized;
    bool connected;
    wifi_event_callback_t callback;
} wifi_state_t;

static wifi_state_t wifi_state = {
    .initialized = false,
    .connected = false,
    .callback = NULL,
};

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi started, attempting connection...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi disconnected, reconnecting...");
        wifi_state.connected = false;
        if (wifi_state.callback) {
            wifi_state.callback(false);
        }
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "WiFi connected! IP: " IPSTR, IP2STR(&event->ip_info.ip));
        wifi_state.connected = true;
        if (wifi_state.callback) {
            wifi_state.callback(true);
        }
    }
}

esp_err_t wifi_manager_init(const char *ssid, const char *password) {
    if (ssid == NULL || password == NULL) {
        ESP_LOGE(TAG, "Invalid SSID or password");
        return ESP_ERR_INVALID_ARG;
    }

    if (wifi_state.initialized) {
        ESP_LOGW(TAG, "WiFi already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing WiFi");
    ESP_LOGI(TAG, "SSID: %s", ssid);

    // TODO: Initialize WiFi in actual implementation
    // 1. esp_netif_create_default_wifi_sta();
    // 2. wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    // 3. esp_wifi_init(&cfg);
    // 4. Setup event handlers
    // 5. Configure SSID and password
    // 6. esp_wifi_start();

    wifi_state.initialized = true;
    ESP_LOGI(TAG, "WiFi initialization complete");

    return ESP_OK;
}

esp_err_t wifi_manager_register_callback(wifi_event_callback_t callback) {
    if (callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_state.callback = callback;
    ESP_LOGI(TAG, "WiFi event callback registered");

    return ESP_OK;
}

bool wifi_manager_is_connected(void) {
    return wifi_state.initialized && wifi_state.connected;
}

esp_err_t wifi_manager_get_info(char *ip_addr, int8_t *rssi) {
    if (!wifi_state.initialized) {
        ESP_LOGE(TAG, "WiFi not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // TODO: Get actual WiFi info in real implementation
    if (ip_addr != NULL) {
        strcpy(ip_addr, "192.168.1.100");
    }

    if (rssi != NULL) {
        *rssi = -45; // Example RSSI value
    }

    return ESP_OK;
}

esp_err_t wifi_manager_send_data(const temp_readings_t *readings) {
    if (!wifi_state.connected) {
        ESP_LOGW(TAG, "WiFi not connected, cannot send data");
        return ESP_ERR_INVALID_STATE;
    }

    if (readings == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Sending temperature data via WiFi");
    ESP_LOGI(TAG, "  Entrance: %.2f°C", readings->temp_entrance);
    ESP_LOGI(TAG, "  Middle: %.2f°C", readings->temp_middle);
    ESP_LOGI(TAG, "  Exit: %.2f°C", readings->temp_exit);

    // TODO: Implement actual data transmission in real implementation
    // Could use MQTT, HTTP, or custom protocol

    return ESP_OK;
}

esp_err_t wifi_manager_deinit(void) {
    if (!wifi_state.initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deinitializing WiFi");
    // TODO: Implement actual deinit in real implementation
    // 1. esp_wifi_stop();
    // 2. esp_wifi_deinit();
    // 3. esp_netif_destroy_default_wifi_sta();

    wifi_state.initialized = false;
    wifi_state.connected = false;
    wifi_state.callback = NULL;

    return ESP_OK;
}
