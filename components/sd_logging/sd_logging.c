#include "sd_logging.h"

#include "driver/gpio.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static const char *TAG = "SDLogging";

#define SD_MOUNT_POINT "/sdcard"
#define SD_LOG_DIR "/sdcard/logs"

typedef struct {
    bool initialized;
    int miso_pin;
    int mosi_pin;
    int clk_pin;
    int cs_pin;
    sdmmc_card_t *card;
    sdmmc_host_t host;
} sd_logging_state_t;

static sd_logging_state_t sd_state = {
    .initialized = false,
    .miso_pin = -1,
    .mosi_pin = -1,
    .clk_pin = -1,
    .cs_pin = -1,
    .card = NULL,
};

static bool sd_logging_time_is_valid(struct tm *tm_info) {
    return tm_info != NULL && tm_info->tm_year >= (2024 - 1900);
}

static void sd_logging_make_iso8601(char *out, size_t out_size, time_t now,
                                    struct tm *tm_info) {
    if (sd_logging_time_is_valid(tm_info)) {
        strftime(out, out_size, "%Y-%m-%d %H:%M:%S", tm_info);
        return;
    }

    snprintf(out, out_size, "UNSYNCED_%lld", (long long)now);
}

static esp_err_t sd_logging_ensure_log_dir(void) {
    struct stat st;
    if (stat(SD_LOG_DIR, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return ESP_OK;
        }
        ESP_LOGE(TAG, "%s exists but is not a directory", SD_LOG_DIR);
        return ESP_FAIL;
    }

    if (mkdir(SD_LOG_DIR, 0755) != 0) {
        ESP_LOGE(TAG, "Failed to create %s: errno=%d", SD_LOG_DIR, errno);
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t sd_logging_write_header_if_empty(FILE *fp) {
    long pos = ftell(fp);
    if (pos < 0) {
        return ESP_FAIL;
    }

    if (pos == 0) {
        if (fprintf(fp, "timestamp,epoch,entrance_c,middle_c,exit_c\n") < 0) {
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

esp_err_t sd_logging_init(int miso_pin, int mosi_pin, int clk_pin, int cs_pin) {
    if (sd_state.initialized) {
        ESP_LOGW(TAG, "SD card already initialized");
        return ESP_OK;
    }

    if (miso_pin < 0 || mosi_pin < 0 || clk_pin < 0 || cs_pin < 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing SD SPI: MISO=%d MOSI=%d CLK=%d CS=%d", miso_pin,
             mosi_pin, clk_pin, cs_pin);

    // Keep CS in a known idle state before starting SPI/SD init.
    // This avoids accidental card selection while lines settle.
    gpio_reset_pin((gpio_num_t)cs_pin);
    gpio_set_direction((gpio_num_t)cs_pin, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)cs_pin, 1);

    // SD modules on jumper wires can be sensitive to signal integrity.
    // Use pull-ups and a conservative SPI clock to improve CRC robustness.
    gpio_set_pull_mode((gpio_num_t)miso_pin, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)mosi_pin, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)clk_pin, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)cs_pin, GPIO_PULLUP_ONLY);

    sd_state.host = (sdmmc_host_t)SDSPI_HOST_DEFAULT();
    sd_state.host.slot = SPI3_HOST;
    sd_state.host.max_freq_khz = 1000; // 1 MHz for stable init on long wires

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = mosi_pin,
        .miso_io_num = miso_pin,
        .sclk_io_num = clk_pin,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };

    esp_err_t ret =
        spi_bus_initialize(sd_state.host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(ret));
        return ret;
    }

    sdspi_device_config_t slot_cfg = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_cfg.gpio_cs = cs_pin;
    slot_cfg.host_id = sd_state.host.slot;

    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files = 8,
        .allocation_unit_size = 16 * 1024,
    };

    ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &sd_state.host, &slot_cfg,
                                  &mount_cfg, &sd_state.card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
        spi_bus_free(sd_state.host.slot);
        return ret;
    }

    ret = sd_logging_ensure_log_dir();
    if (ret != ESP_OK) {
        esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, sd_state.card);
        spi_bus_free(sd_state.host.slot);
        sd_state.card = NULL;
        return ret;
    }

    sd_state.miso_pin = miso_pin;
    sd_state.mosi_pin = mosi_pin;
    sd_state.clk_pin = clk_pin;
    sd_state.cs_pin = cs_pin;
    sd_state.initialized = true;

    ESP_LOGI(TAG, "SD card mounted at %s", SD_MOUNT_POINT);
    sdmmc_card_print_info(stdout, sd_state.card);

    return ESP_OK;
}

esp_err_t sd_logging_record_data(const temp_readings_t *readings) {
    if (!sd_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (readings == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    time_t now = time(NULL);
    struct tm tm_info;
    struct tm *tm_ptr = localtime_r(&now, &tm_info);

    char day_file[96] = {0};
    if (sd_logging_time_is_valid(tm_ptr)) {
        size_t written = strftime(day_file, sizeof(day_file),
                                  SD_LOG_DIR "/%Y-%m-%d.csv", tm_ptr);
        if (written == 0) {
            ESP_LOGW(TAG, "Failed to format dated filename; using unsynced.csv "
                          "fallback");
            snprintf(day_file, sizeof(day_file), SD_LOG_DIR "/unsynced.csv");
        }
    } else {
        snprintf(day_file, sizeof(day_file), SD_LOG_DIR "/unsynced.csv");
    }

    esp_err_t ret = sd_logging_ensure_log_dir();
    if (ret != ESP_OK) {
        return ret;
    }

    char write_file[96] = {0};
    strlcpy(write_file, day_file, sizeof(write_file));

    FILE *fp = fopen(write_file, "a");
    if (fp == NULL) {
        int primary_errno = errno;
        const char *fallback_file = SD_LOG_DIR "/unsynced.csv";

        // Fallback keeps data logging alive if dated filename open fails.
        if (strcmp(write_file, fallback_file) != 0) {
            strlcpy(write_file, fallback_file, sizeof(write_file));
            fp = fopen(write_file, "a");
            if (fp != NULL) {
                ESP_LOGW(TAG,
                         "Failed to open daily file (errno=%d: %s), using %s",
                         primary_errno, strerror(primary_errno), write_file);
            }
        }

        if (fp == NULL) {
            ESP_LOGE(TAG, "Failed to open %s: errno=%d (%s)", write_file, errno,
                     strerror(errno));
            return ESP_FAIL;
        }
    }

    ret = sd_logging_write_header_if_empty(fp);
    if (ret != ESP_OK) {
        fclose(fp);
        return ret;
    }

    char timestamp[40];
    sd_logging_make_iso8601(timestamp, sizeof(timestamp), now, tm_ptr);

    if (fprintf(fp, "%s,%lld,%.2f,%.2f,%.2f\n", timestamp, (long long)now,
                readings->temp_entrance, readings->temp_middle,
                readings->temp_exit) < 0) {
        fclose(fp);
        return ESP_FAIL;
    }

    fflush(fp);
    fclose(fp);

    ESP_LOGI(TAG,
             "Logged row -> file=%s timestamp=%s entrance=%.2fC middle=%.2fC "
             "exit=%.2fC",
             write_file, timestamp, readings->temp_entrance,
             readings->temp_middle, readings->temp_exit);
    return ESP_OK;
}

esp_err_t sd_logging_get_info(uint64_t *total_size, uint64_t *used_size) {
    if (!sd_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (total_size == NULL || used_size == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // ESP-IDF v4.4 does not expose a stable free-space helper for SDSPI mount
    // in all configurations. Report card capacity and mark used size unknown.
    *total_size =
        (uint64_t)sd_state.card->csd.capacity * sd_state.card->csd.sector_size;
    *used_size = 0;

    return ESP_OK;
}

esp_err_t sd_logging_list_files(void) {
    if (!sd_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    DIR *dir = opendir(SD_LOG_DIR);
    if (dir == NULL) {
        ESP_LOGE(TAG, "Failed to open %s: errno=%d", SD_LOG_DIR, errno);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Log files in %s:", SD_LOG_DIR);
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        ESP_LOGI(TAG, "  %s", entry->d_name);
    }

    closedir(dir);
    return ESP_OK;
}

esp_err_t sd_logging_cleanup_old_files(int days_to_keep) {
    if (!sd_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (days_to_keep <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    time_t now = time(NULL);
    double max_age_seconds = (double)days_to_keep * 86400.0;

    DIR *dir = opendir(SD_LOG_DIR);
    if (dir == NULL) {
        ESP_LOGE(TAG, "Failed to open %s: errno=%d", SD_LOG_DIR, errno);
        return ESP_FAIL;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        int year = 0;
        int month = 0;
        int day = 0;
        if (sscanf(entry->d_name, "%4d-%2d-%2d.csv", &year, &month, &day) !=
            3) {
            continue;
        }

        struct tm file_tm = {
            .tm_year = year - 1900,
            .tm_mon = month - 1,
            .tm_mday = day,
            .tm_hour = 0,
            .tm_min = 0,
            .tm_sec = 0,
            .tm_isdst = -1,
        };

        time_t file_time = mktime(&file_tm);
        if (file_time <= 0) {
            continue;
        }

        if (difftime(now, file_time) <= max_age_seconds) {
            continue;
        }

        char full_path[320];
        int written = snprintf(full_path, sizeof(full_path), SD_LOG_DIR "/%s",
                               entry->d_name);
        if (written < 0 || written >= (int)sizeof(full_path)) {
            ESP_LOGW(TAG, "Skipping long path for file: %s", entry->d_name);
            continue;
        }
        if (unlink(full_path) == 0) {
            ESP_LOGI(TAG, "Deleted old log file: %s", full_path);
        } else {
            ESP_LOGW(TAG, "Failed to delete %s (errno=%d)", full_path, errno);
        }
    }

    closedir(dir);
    return ESP_OK;
}

esp_err_t sd_logging_deinit(void) {
    if (!sd_state.initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Unmounting SD card");
    esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, sd_state.card);
    spi_bus_free(sd_state.host.slot);

    sd_state.initialized = false;
    sd_state.card = NULL;
    sd_state.miso_pin = -1;
    sd_state.mosi_pin = -1;
    sd_state.clk_pin = -1;
    sd_state.cs_pin = -1;

    return ESP_OK;
}
