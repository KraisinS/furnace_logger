#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sd_logging.h"

#include <ctype.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "temp_sensor.h"
#include "tm1637_display.h"

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

#ifndef CONFIG_ENABLE_SD_LOGGING
#define CONFIG_ENABLE_SD_LOGGING 1
#endif
#ifndef CONFIG_SD_SPI_MISO_PIN
#define CONFIG_SD_SPI_MISO_PIN 35
#endif
#ifndef CONFIG_SD_SPI_MOSI_PIN
#define CONFIG_SD_SPI_MOSI_PIN 14
#endif
#ifndef CONFIG_SD_SPI_CLK_PIN
#define CONFIG_SD_SPI_CLK_PIN 12
#endif
#ifndef CONFIG_SD_SPI_CS_PIN
#define CONFIG_SD_SPI_CS_PIN 2
#endif
#ifndef CONFIG_SD_LOG_INTERVAL_SECONDS
#define CONFIG_SD_LOG_INTERVAL_SECONDS 60
#endif
#ifndef CONFIG_SD_LOG_RETENTION_DAYS
#define CONFIG_SD_LOG_RETENTION_DAYS 30
#endif

#ifndef CONFIG_ENABLE_TIME_SYNC
#define CONFIG_ENABLE_TIME_SYNC 0
#endif
#ifndef CONFIG_TIME_SYNC_WIFI_SSID
#define CONFIG_TIME_SYNC_WIFI_SSID ""
#endif
#ifndef CONFIG_TIME_SYNC_WIFI_PASSWORD
#define CONFIG_TIME_SYNC_WIFI_PASSWORD ""
#endif
#ifndef CONFIG_TIME_SYNC_NTP_SERVER
#define CONFIG_TIME_SYNC_NTP_SERVER "pool.ntp.org"
#endif
#ifndef CONFIG_TIME_SYNC_TIMEOUT_SECONDS
#define CONFIG_TIME_SYNC_TIMEOUT_SECONDS 20
#endif
#ifndef CONFIG_ENABLE_SD_LOG_HTTP
#define CONFIG_ENABLE_SD_LOG_HTTP 0
#endif
#ifndef CONFIG_SD_LOG_HTTP_PORT
#define CONFIG_SD_LOG_HTTP_PORT 80
#endif

#define APP_LOCAL_TZ "ICT-7"

static const char *TAG = "OvenTempLogger";
static TaskHandle_t app_task_handle = NULL;
static int (*s_log_vprintf)(const char *fmt, va_list args) = NULL;
static bool s_log_line_start = true;
#if CONFIG_ENABLE_SD_LOG_HTTP
static httpd_handle_t s_http_server = NULL;
#endif

static int app_log_vprintf_with_datetime(const char *fmt, va_list args) {
    char message[512];
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(message, sizeof(message), fmt, args_copy);
    va_end(args_copy);

    if (len <= 0) {
        return len;
    }

    const size_t buf_len = sizeof(message);
    if ((size_t)len >= buf_len) {
        len = (int)(buf_len - 1);
        message[len] = '\0';
    }

    int written = 0;
    for (int i = 0; i < len; i++) {
        if (s_log_line_start) {
            char ts[24] = {0};
            time_t now = time(NULL);
            struct tm tm_info;
            struct tm *tm_ptr = localtime_r(&now, &tm_info);

            if (tm_ptr != NULL && tm_ptr->tm_year >= (2024 - 1900)) {
                strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_ptr);
            } else {
                snprintf(ts, sizeof(ts), "UNSYNCED");
            }

            written += printf("[%s] ", ts);
            s_log_line_start = false;
        }

        written += putchar(message[i]);
        if (message[i] == '\n') {
            s_log_line_start = true;
        }
    }

    return written;
}

#if CONFIG_ENABLE_TIME_SYNC
static EventGroupHandle_t s_wifi_event_group = NULL;
static esp_event_handler_instance_t s_wifi_any_id_handler;
static esp_event_handler_instance_t s_ip_got_ip_handler;
static esp_log_level_t s_wifi_log_level_before_sync = ESP_LOG_INFO;

#define WIFI_CONNECTED_BIT BIT0

static void app_set_timezone_bangkok(void) {
    setenv("TZ", APP_LOCAL_TZ, 1);
    tzset();
}

static bool app_time_is_valid(void) {
    time_t now = time(NULL);
    struct tm time_info;
    struct tm *tm_ptr = localtime_r(&now, &time_info);
    return tm_ptr != NULL && tm_ptr->tm_year >= (2024 - 1900);
}

static void app_wifi_event_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data) {
    (void)arg;
    (void)event_data;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        // Keep retrying during the timeout window instead of failing fast.
        esp_wifi_connect();
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        if (s_wifi_event_group != NULL) {
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}

static esp_err_t app_start_wifi_for_time_sync(void) {
    const char *ssid = CONFIG_TIME_SYNC_WIFI_SSID;
    const char *password = CONFIG_TIME_SYNC_WIFI_PASSWORD;

    s_wifi_log_level_before_sync = esp_log_level_get("wifi");
    esp_log_level_set("wifi", ESP_LOG_WARN);

    if (ssid == NULL || ssid[0] == '\0') {
        ESP_LOGW(TAG, "Time sync skipped: WiFi SSID is empty in config");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Time sync skipped: nvs_flash_init failed: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Time sync skipped: esp_netif_init failed: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Time sync skipped: event loop init failed: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    (void)esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Time sync skipped: esp_wifi_init failed: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        ESP_LOGW(TAG, "Time sync skipped: failed to create WiFi event group");
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &app_wifi_event_handler, NULL,
        &s_wifi_any_id_handler));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &app_wifi_event_handler, NULL,
        &s_ip_got_ip_handler));

    wifi_config_t wifi_cfg = {0};
    strlcpy((char *)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid));
    strlcpy((char *)wifi_cfg.sta.password, password,
            sizeof(wifi_cfg.sta.password));
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_cfg.sta.pmf_cfg.capable = true;
    wifi_cfg.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting WiFi for time sync (SSID: %s)", ssid);
    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group, WIFI_CONNECTED_BIT, pdTRUE, pdFALSE,
        pdMS_TO_TICKS(CONFIG_TIME_SYNC_TIMEOUT_SECONDS * 1000));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi connected for time sync");
        return ESP_OK;
    }

    ESP_LOGW(TAG, "WiFi connect timeout for time sync");
    return ESP_ERR_TIMEOUT;
}

static void app_stop_wifi_for_time_sync(void) {
    (void)esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                s_ip_got_ip_handler);
    (void)esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                s_wifi_any_id_handler);
    (void)esp_wifi_stop();
    (void)esp_wifi_deinit();

    if (s_wifi_event_group != NULL) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
    }

    esp_log_level_set("wifi", s_wifi_log_level_before_sync);
}

static esp_err_t app_sync_time_once(bool keep_wifi_running) {
    app_set_timezone_bangkok();

    if (app_time_is_valid()) {
        ESP_LOGI(TAG, "System time already valid");
        return ESP_OK;
    }

    esp_err_t ret = app_start_wifi_for_time_sync();
    if (ret != ESP_OK) {
        return ret;
    }

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, CONFIG_TIME_SYNC_NTP_SERVER);
    sntp_init();

    ESP_LOGI(TAG, "Waiting for SNTP time sync from %s...",
             CONFIG_TIME_SYNC_NTP_SERVER);
    bool synced = false;
    for (int i = 0; i < CONFIG_TIME_SYNC_TIMEOUT_SECONDS; i++) {
        if (app_time_is_valid()) {
            synced = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (synced) {
        time_t now = time(NULL);
        struct tm time_info;
        localtime_r(&now, &time_info);
        ESP_LOGI(TAG, "Time sync success: %04d-%02d-%02d %02d:%02d:%02d",
                 time_info.tm_year + 1900, time_info.tm_mon + 1,
                 time_info.tm_mday, time_info.tm_hour, time_info.tm_min,
                 time_info.tm_sec);
    } else {
        ESP_LOGW(TAG, "Time sync timeout; logging may use UNSYNCED timestamps");
    }

    sntp_stop();
    if (!keep_wifi_running) {
        app_stop_wifi_for_time_sync();
    }

    return synced ? ESP_OK : ESP_ERR_TIMEOUT;
}
#endif

#if CONFIG_ENABLE_SD_LOGGING
static bool s_sd_logging_ready = false;
#endif

#if CONFIG_ENABLE_SD_LOG_HTTP
static bool app_is_safe_filename(const char *name) {
    if (name == NULL || name[0] == '\0') {
        return false;
    }
    if (strstr(name, "..") != NULL || strchr(name, '/') != NULL ||
        strchr(name, '\\') != NULL) {
        return false;
    }

    for (const char *p = name; *p != '\0'; p++) {
        if (!isalnum((unsigned char)*p) && *p != '-' && *p != '_' &&
            *p != '.') {
            return false;
        }
    }

    const char *ext = strrchr(name, '.');
    return ext != NULL && strcmp(ext, ".csv") == 0;
}

static esp_err_t app_http_root_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(
        req, "<html><body><h2>OvenTempLogger</h2><p>Open <a "
             "href=\"/logs\">/logs</a> to view SD files.</p></body></html>");
    return ESP_OK;
}

static esp_err_t app_http_logs_handler(httpd_req_t *req) {
    DIR *dir = opendir("/sdcard/logs");
    if (dir == NULL) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "Failed to open /sdcard/logs");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr_chunk(req, "<html><body><h2>SD Log Files</h2><ul "
                                  "style=\"font-family:monospace\">");

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (!app_is_safe_filename(entry->d_name)) {
            continue;
        }

        char row[600];
        snprintf(row, sizeof(row), "<li><a href=\"/download?f=%s\">%s</a></li>",
                 entry->d_name, entry->d_name);
        httpd_resp_sendstr_chunk(req, row);
    }

    closedir(dir);
    httpd_resp_sendstr_chunk(req, "</ul></body></html>");
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

static esp_err_t app_http_download_handler(httpd_req_t *req) {
    char query[128] = {0};
    char filename[96] = {0};

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "f", filename, sizeof(filename)) !=
            ESP_OK ||
        !app_is_safe_filename(filename)) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "Invalid file request");
        return ESP_FAIL;
    }

    char path[160] = {0};
    snprintf(path, sizeof(path), "/sdcard/logs/%s", filename);

    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_sendstr(req, "File not found");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/csv");
    char content_disposition[160];
    snprintf(content_disposition, sizeof(content_disposition),
             "attachment; filename=\"%s\"", filename);
    httpd_resp_set_hdr(req, "Content-Disposition", content_disposition);

    char buf[512];
    while (fgets(buf, sizeof(buf), fp) != NULL) {
        if (httpd_resp_sendstr_chunk(req, buf) != ESP_OK) {
            fclose(fp);
            return ESP_FAIL;
        }
    }

    fclose(fp);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

static esp_err_t app_start_sd_log_http_server(void) {
    if (s_http_server != NULL) {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = CONFIG_SD_LOG_HTTP_PORT;

    esp_err_t ret = httpd_start(&s_http_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = app_http_root_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t logs_uri = {
        .uri = "/logs",
        .method = HTTP_GET,
        .handler = app_http_logs_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t download_uri = {
        .uri = "/download",
        .method = HTTP_GET,
        .handler = app_http_download_handler,
        .user_ctx = NULL,
    };

    ESP_ERROR_CHECK(httpd_register_uri_handler(s_http_server, &root_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_http_server, &logs_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_http_server, &download_uri));

    ESP_LOGI(TAG, "SD log HTTP server started on port %d",
             CONFIG_SD_LOG_HTTP_PORT);
    ESP_LOGI(TAG, "Open: http://<esp32-ip>:%d/logs", CONFIG_SD_LOG_HTTP_PORT);
    return ESP_OK;
}
#endif

static void temperature_display_task(void *pvParameters) {
    (void)pvParameters;

    ESP_LOGI(TAG, "Temperature display task started");
    ESP_LOGI(TAG, "Sensor read schedule (1 second stagger):");
    ESP_LOGI(TAG, "  Sensor 1: read at T+0s, T+3s, T+6s, ...");
    ESP_LOGI(TAG, "  Sensor 2: read at T+1s, T+4s, T+7s, ...");
    ESP_LOGI(TAG, "  Sensor 3: read at T+2s, T+5s, T+8s, ...");

    const TickType_t stagger_interval = pdMS_TO_TICKS(3000);
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

#if CONFIG_ENABLE_SD_LOGGING
    TickType_t next_log_due =
        now + pdMS_TO_TICKS(CONFIG_SD_LOG_INTERVAL_SECONDS * 1000);
    float latest_temps[3] = {0.0f, 0.0f, 0.0f};
    bool latest_valid[3] = {false, false, false};
#endif

    while (1) {
        TickType_t current_time = xTaskGetTickCount();

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
#if CONFIG_ENABLE_SD_LOGGING
                    latest_temps[i] = temperature;
                    latest_valid[i] = true;
#endif
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

#if CONFIG_ENABLE_SD_LOGGING
        if (s_sd_logging_ready && (int32_t)(current_time - next_log_due) >= 0) {
            if (latest_valid[0] && latest_valid[1] && latest_valid[2]) {
                temp_readings_t readings = {
                    .temp_entrance = latest_temps[0],
                    .temp_middle = latest_temps[1],
                    .temp_exit = latest_temps[2],
                };

                esp_err_t log_ret = sd_logging_record_data(&readings);
                if (log_ret != ESP_OK) {
                    ESP_LOGW(TAG, "SD log write failed: %s",
                             esp_err_to_name(log_ret));
                }
            } else {
                ESP_LOGW(TAG, "SD log skipped: waiting for valid readings from "
                              "all sensors");
            }

            next_log_due +=
                pdMS_TO_TICKS(CONFIG_SD_LOG_INTERVAL_SECONDS * 1000);
        }
#endif

        vTaskDelay(check_interval);
    }
}

void app_main(void) {
    if (s_log_vprintf == NULL) {
        s_log_vprintf = esp_log_set_vprintf(app_log_vprintf_with_datetime);
    }

#if CONFIG_ENABLE_SD_LOGGING
    // Keep SD CS idle-high early in runtime to avoid spurious card select.
    gpio_reset_pin((gpio_num_t)CONFIG_SD_SPI_CS_PIN);
    gpio_set_direction((gpio_num_t)CONFIG_SD_SPI_CS_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)CONFIG_SD_SPI_CS_PIN, 1);
    gpio_set_pull_mode((gpio_num_t)CONFIG_SD_SPI_CS_PIN, GPIO_PULLUP_ONLY);
#endif

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

#if CONFIG_ENABLE_TIME_SYNC
    bool keep_wifi_for_http = CONFIG_ENABLE_SD_LOG_HTTP;
    (void)app_sync_time_once(keep_wifi_for_http);
#endif

#if CONFIG_ENABLE_SD_LOGGING
    ESP_LOGI(TAG, "Initializing SD logging...");
    ret = sd_logging_init(CONFIG_SD_SPI_MISO_PIN, CONFIG_SD_SPI_MOSI_PIN,
                          CONFIG_SD_SPI_CLK_PIN, CONFIG_SD_SPI_CS_PIN);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG,
                 "SD logging init failed (%s). Continuing without SD logging.",
                 esp_err_to_name(ret));
        s_sd_logging_ready = false;
    } else {
        s_sd_logging_ready = true;
        (void)sd_logging_cleanup_old_files(CONFIG_SD_LOG_RETENTION_DAYS);
        (void)sd_logging_list_files();
        ESP_LOGI(TAG,
                 "  - SD logging: enabled (MISO=%d MOSI=%d CLK=%d CS=%d, every "
                 "%d sec)",
                 CONFIG_SD_SPI_MISO_PIN, CONFIG_SD_SPI_MOSI_PIN,
                 CONFIG_SD_SPI_CLK_PIN, CONFIG_SD_SPI_CS_PIN,
                 CONFIG_SD_LOG_INTERVAL_SECONDS);
    }
#else
    ESP_LOGI(TAG, "  - SD logging: disabled");
#endif

#if CONFIG_ENABLE_SD_LOG_HTTP
#if CONFIG_ENABLE_SD_LOGGING && CONFIG_ENABLE_TIME_SYNC
    if (s_sd_logging_ready) {
        (void)app_start_sd_log_http_server();
    } else {
        ESP_LOGW(
            TAG,
            "HTTP log server not started because SD logging is unavailable");
    }
#else
    ESP_LOGW(TAG, "Enable SD logging and time sync to use SD log HTTP server");
#endif
#endif

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
