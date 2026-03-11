#include "max6675.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "rom/ets_sys.h"

static const char *TAG = "MAX6675";
static portMUX_TYPE max6675_gpio_lock = portMUX_INITIALIZER_UNLOCKED;

// MAX6675 frame sanity check:
// D15 should be 0, D1:D0 should be 00. If any of these bits are set,
// the frame is likely misaligned or corrupted.
static bool max6675_frame_is_valid(uint16_t raw_data) {
    return (raw_data & 0x8003) == 0;
}

static uint16_t max6675_shift_in_frame(max6675_device_t *device, int clk_delay,
                                       int pre_delay) {
    uint16_t value = 0;

    portENTER_CRITICAL(&max6675_gpio_lock);

    gpio_set_level(device->clk_pin, 0);
    gpio_set_level(device->cs_pin, 0);
    ets_delay_us(pre_delay);

    // MAX6675 presents D15 immediately after CS goes low. Each subsequent bit
    // is stable during the low phase after the previous falling edge, so sample
    // while SCK is low and then pulse SCK high-low to advance the shift
    // register.
    for (int i = 0; i < 16; i++) {
        ets_delay_us(clk_delay);
        value = (uint16_t)((value << 1) |
                           (gpio_get_level(device->miso_pin) & 0x01));
        gpio_set_level(device->clk_pin, 1);
        ets_delay_us(clk_delay);
        gpio_set_level(device->clk_pin, 0);
    }

    gpio_set_level(device->cs_pin, 1);
    ets_delay_us(pre_delay);

    portEXIT_CRITICAL(&max6675_gpio_lock);

    return value;
}

esp_err_t max6675_init(max6675_device_t *device, int cs_pin, int clk_pin,
                       int miso_pin) {
    if (device == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (cs_pin < 0 || clk_pin < 0 || miso_pin < 0) {
        return ESP_ERR_INVALID_ARG;
    }

    device->cs_pin = (gpio_num_t)cs_pin;
    device->clk_pin = (gpio_num_t)clk_pin;
    device->miso_pin = (gpio_num_t)miso_pin;

    gpio_config_t out_cfg = {
        .pin_bit_mask = (1ULL << device->cs_pin) | (1ULL << device->clk_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&out_cfg));

    gpio_config_t in_cfg = {
        .pin_bit_mask = (1ULL << device->miso_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&in_cfg));

    gpio_set_level(device->cs_pin, 1);
    gpio_set_level(device->clk_pin, 0);

    ESP_LOGI(TAG, "Initialized (CS=%d CLK=%d MISO=%d)", (int)device->cs_pin,
             (int)device->clk_pin, (int)device->miso_pin);
    return ESP_OK;
}

esp_err_t max6675_read_raw(max6675_device_t *device, uint16_t *raw_data) {
    if (device == NULL || raw_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Keep Sensor 3 (CS=15) on conservative timing. In practice this path is
    // more wiring-sensitive and benefits from longer settling/clock periods.
    int is_sensor3 = ((int)device->cs_pin == 15);
    int clk_delay = is_sensor3 ? 30 : 5;
    int pre_delay = is_sensor3 ? 40 : 10;
    uint16_t value = max6675_shift_in_frame(device, clk_delay, pre_delay);

    *raw_data = value;

    // Diagnostic: log raw data with detailed info
    uint16_t temp_bits = (value >> 3) & 0x0FFF;
    int fault_bit = (value & 0x04) ? 1 : 0; // D2 = open TC fault
    float temp_celsius = temp_bits * 0.25f;

    ESP_LOGI(TAG, "CS=%d: Raw=0x%04X Binary=%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d",
             (int)device->cs_pin, value, (value >> 15) & 1, (value >> 14) & 1,
             (value >> 13) & 1, (value >> 12) & 1, (value >> 11) & 1,
             (value >> 10) & 1, (value >> 9) & 1, (value >> 8) & 1,
             (value >> 7) & 1, (value >> 6) & 1, (value >> 5) & 1,
             (value >> 4) & 1, (value >> 3) & 1, (value >> 2) & 1,
             (value >> 1) & 1, (value >> 0) & 1);
    ESP_LOGI(TAG, "CS=%d: TempBits=0x%03X (%d) Fault=%d → %.2f°C",
             (int)device->cs_pin, temp_bits, (int)temp_bits, fault_bit,
             temp_celsius);

    // DIAGNOSTIC: Do 4 sequential reads with proper inter-read delay to allow
    // MAX6675 conversion
    uint16_t quick_reads[4];
    if ((device->read_count++ % 10) == 0) { // Every 10th read per sensor
        int diag_delay_us = is_sensor3 ? 250000 : 50000;
        ESP_LOGI(TAG,
                 "CS=%d: DIAGNOSTIC - Reading 4x with %dms delay between "
                 "(conversion time):",
                 (int)device->cs_pin, is_sensor3 ? 250 : 50);
        for (int test = 0; test < 4; test++) {
            uint16_t test_val =
                max6675_shift_in_frame(device, clk_delay, pre_delay);

            quick_reads[test] = test_val;
            ESP_LOGI(TAG, "  Read[%d]: 0x%04X", test, test_val);

            // Delay between reads to allow MAX6675 to perform a new conversion
            if (test < 3) {
                ets_delay_us(diag_delay_us);
            }
        }

        // Warn only if all 4 reads are 0x0000 or 0xFFFF — stable non-zero
        // identical reads are expected when temperature is not changing.
        bool all_same = (quick_reads[0] == quick_reads[1]) &&
                        (quick_reads[1] == quick_reads[2]) &&
                        (quick_reads[2] == quick_reads[3]);
        bool is_rail_value =
            (quick_reads[0] == 0x0000 || quick_reads[0] == 0xFFFF);
        if (all_same && is_rail_value) {
            ESP_LOGW(TAG,
                     "CS=%d: **WARNING** All 4 reads = 0x%04X — MISO stuck "
                     "or thermocouple disconnected",
                     (int)device->cs_pin, quick_reads[0]);
        } else if (all_same) {
            ESP_LOGI(
                TAG,
                "CS=%d: Reads stable at 0x%04X (temperature not changing — OK)",
                (int)device->cs_pin, quick_reads[0]);
        } else {
            ESP_LOGI(TAG, "CS=%d: Reads vary — temperature actively changing",
                     (int)device->cs_pin);
        }
    }

    // Extra diagnostics for Sensor 2 default CS (GPIO4):
    // helps confirm that SCK2 is toggled by firmware and whether MISO2 ever
    // changes.
    if ((int)device->cs_pin == 4 && (device->read_count % 20U) == 0U) {
        int sck_after_read = gpio_get_level(device->clk_pin);
        int miso_idle_high = gpio_get_level(device->miso_pin);
        ets_delay_us(20);
        int miso_idle_later = gpio_get_level(device->miso_pin);

        ESP_LOGI(TAG,
                 "CS=4 line-check: SCK=%d (expected 0 idle), MISO idle=%d/%d",
                 sck_after_read, miso_idle_high, miso_idle_later);
        if (miso_idle_high == 0 && miso_idle_later == 0 && value == 0x0000) {
            ESP_LOGW(TAG, "CS=4 line-check: MISO stuck low + raw=0x0000; "
                          "check SO wire/power/GND on MAX2");
        }
    }

    return ESP_OK;
}

esp_err_t max6675_convert(uint16_t raw_data, float *temperature) {
    if (temperature == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (max6675_is_fault(raw_data)) {
        ESP_LOGW(TAG, "Thermocouple open or disconnected");
        return ESP_ERR_INVALID_RESPONSE;
    }

    uint16_t temp_raw = (raw_data >> 3) & 0x0FFF;
    *temperature = temp_raw * MAX6675_RESOLUTION;

    ESP_LOGI(TAG, "Conversion: raw=0x%04X, temp_raw=0x%03X (%d), temp=%.2f°C",
             raw_data, temp_raw, (int)temp_raw, *temperature);

    return ESP_OK;
}

esp_err_t max6675_read_temperature(max6675_device_t *device,
                                   float *temperature) {
    if (device == NULL || temperature == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // First read via max6675_read_raw (handles diagnostics / logging).
    // Retry up to 4 times only for outright-invalid frames (D15/D1/D0 = 1).
    uint16_t raw_data = 0;
    for (int attempt = 0; attempt < 4; attempt++) {
        esp_err_t ret = max6675_read_raw(device, &raw_data);
        if (ret != ESP_OK) {
            return ret;
        }

        if (!max6675_frame_is_valid(raw_data)) {
            ESP_LOGW(TAG,
                     "CS=%d: Invalid frame 0x%04X (attempt %d/4), retrying",
                     (int)device->cs_pin, raw_data, attempt + 1);
            ets_delay_us(10);
            continue;
        }
        break; // got a structurally valid frame
    }

    if (!max6675_frame_is_valid(raw_data)) {
        ESP_LOGW(TAG, "CS=%d: Failed to get valid frame after retries",
                 (int)device->cs_pin);
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Anti-shift filter for Sensor 3 (CS=15):
    //
    // A SCK glitch (noise on GPIO13) can advance the MAX6675 output register
    // by one extra step before we start reading, producing a valid-looking
    // frame that is left-shifted by N bits (temperature reads as 2^N × true).
    // 0x03A0(29°C) → 0x0740(58°C) → 0x0E80(116°C) → 0x1D00(232°C) etc.
    //
    // Because a shift can only INFLATE the reading, taking the minimum of 3
    // independent reads (each after a fresh 250 ms MAX6675 conversion) gives
    // the value closest to the true temperature.
    if ((int)device->cs_pin == 15) {
        uint16_t best_raw = raw_data;
        for (int n = 1; n <= 2; n++) {
            // 250 ms > 220 ms (MAX6675 max conversion time): guarantees a new
            // conversion result rather than re-reading the same register.
            ets_delay_us(250000);
            uint16_t rawn = max6675_shift_in_frame(device, 30, 40);
            ESP_LOGI(TAG, "CS=15: anti-shift read[%d]=0x%04X (best=0x%04X)", n,
                     rawn, best_raw);
            if (max6675_frame_is_valid(rawn) && rawn < best_raw) {
                best_raw = rawn;
            }
        }
        if (best_raw != raw_data) {
            ESP_LOGI(TAG,
                     "CS=15: bit-shift corrected 0x%04X→0x%04X (%.2f°C→%.2f°C)",
                     raw_data, best_raw, ((raw_data >> 3) & 0x0FFF) * 0.25f,
                     ((best_raw >> 3) & 0x0FFF) * 0.25f);
        }
        return max6675_convert(best_raw, temperature);
    }

    return max6675_convert(raw_data, temperature);
}

bool max6675_is_fault(uint16_t raw_data) {
    return (raw_data & MAX6675_FAULT_BIT) != 0;
}
