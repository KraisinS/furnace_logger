#include "tm1637_display.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "rom/ets_sys.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

static const char *TAG = "TM1637";

#define TM1637_CMD_DATA_AUTO 0x40
#define TM1637_CMD_ADDR 0xC0
#define TM1637_CMD_DISPLAY 0x88

#define SEG_BLANK 0x00
#define SEG_MINUS 0x40
#define SEG_C 0x39
#define SEG_E 0x79
#define SEG_R 0x50

typedef struct {
    gpio_num_t clk_pin;
    gpio_num_t dio_pin;
    int brightness;
    bool initialized;
} tm1637_state_t;

#define NUM_DISPLAYS 3

static tm1637_state_t s_states[NUM_DISPLAYS] = {
    {.clk_pin = GPIO_NUM_NC,
     .dio_pin = GPIO_NUM_NC,
     .brightness = 4,
     .initialized = false},
    {.clk_pin = GPIO_NUM_NC,
     .dio_pin = GPIO_NUM_NC,
     .brightness = 4,
     .initialized = false},
    {.clk_pin = GPIO_NUM_NC,
     .dio_pin = GPIO_NUM_NC,
     .brightness = 4,
     .initialized = false},
};

static const uint8_t k_digit_to_segments[10] = {
    0x3F, // 0
    0x06, // 1
    0x5B, // 2
    0x4F, // 3
    0x66, // 4
    0x6D, // 5
    0x7D, // 6
    0x07, // 7
    0x7F, // 8
    0x6F  // 9
};

static inline void tm1637_delay(void) { ets_delay_us(5); }

static void dio_mode_output(tm1637_state_t *state) {
    gpio_set_direction(state->dio_pin, GPIO_MODE_OUTPUT);
}

static void dio_mode_input(tm1637_state_t *state) {
    gpio_set_direction(state->dio_pin, GPIO_MODE_INPUT);
}

static void clk_high(tm1637_state_t *state) {
    gpio_set_level(state->clk_pin, 1);
}

static void clk_low(tm1637_state_t *state) {
    gpio_set_level(state->clk_pin, 0);
}

static void dio_high(tm1637_state_t *state) {
    gpio_set_level(state->dio_pin, 1);
}

static void dio_low(tm1637_state_t *state) {
    gpio_set_level(state->dio_pin, 0);
}

static void start_condition(tm1637_state_t *state) {
    dio_mode_output(state);
    dio_high(state);
    clk_high(state);
    tm1637_delay();
    dio_low(state);
    tm1637_delay();
    clk_low(state);
}

static void stop_condition(tm1637_state_t *state) {
    dio_mode_output(state);
    clk_low(state);
    dio_low(state);
    tm1637_delay();
    clk_high(state);
    tm1637_delay();
    dio_high(state);
    tm1637_delay();
}

static void write_byte(tm1637_state_t *state, uint8_t value) {
    dio_mode_output(state);

    for (int i = 0; i < 8; i++) {
        clk_low(state);
        tm1637_delay();

        if (value & 0x01) {
            dio_high(state);
        } else {
            dio_low(state);
        }

        tm1637_delay();
        clk_high(state);
        tm1637_delay();
        value >>= 1;
    }

    // ACK cycle
    clk_low(state);
    dio_mode_input(state);
    tm1637_delay();
    clk_high(state);
    tm1637_delay();
    (void)gpio_get_level(state->dio_pin);
    clk_low(state);
    dio_mode_output(state);
}

static esp_err_t write_display_raw(tm1637_state_t *state,
                                   const uint8_t digits[4]) {
    if (!state->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    start_condition(state);
    write_byte(state, TM1637_CMD_DATA_AUTO);
    stop_condition(state);

    start_condition(state);
    write_byte(state, TM1637_CMD_ADDR);
    for (int i = 0; i < 4; i++) {
        write_byte(state, digits[i]);
    }
    stop_condition(state);

    start_condition(state);
    write_byte(state,
               (uint8_t)(TM1637_CMD_DISPLAY | (state->brightness & 0x07)));
    stop_condition(state);

    return ESP_OK;
}

static uint8_t digit_segment(int n) {
    if (n < 0 || n > 9) {
        return SEG_BLANK;
    }
    return k_digit_to_segments[n];
}

esp_err_t tm1637_display_init_all(int clk1_pin, int dio1_pin, int clk2_pin,
                                  int dio2_pin, int clk3_pin, int dio3_pin) {
    const int pins[][2] = {
        {clk1_pin, dio1_pin}, {clk2_pin, dio2_pin}, {clk3_pin, dio3_pin}};

    for (int i = 0; i < NUM_DISPLAYS; i++) {
        if (pins[i][0] < 0 || pins[i][1] < 0) {
            return ESP_ERR_INVALID_ARG;
        }

        s_states[i].clk_pin = (gpio_num_t)pins[i][0];
        s_states[i].dio_pin = (gpio_num_t)pins[i][1];

        gpio_config_t cfg = {
            .pin_bit_mask =
                (1ULL << s_states[i].clk_pin) | (1ULL << s_states[i].dio_pin),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&cfg));

        clk_high(&s_states[i]);
        dio_high(&s_states[i]);
        s_states[i].initialized = true;

        ESP_LOGI(TAG, "Display %d initialized (CLK=%d DIO=%d)", i + 1,
                 (int)s_states[i].clk_pin, (int)s_states[i].dio_pin);

        uint8_t blank[4] = {SEG_BLANK, SEG_BLANK, SEG_BLANK, SEG_BLANK};
        write_display_raw(&s_states[i], blank);
    }

    return ESP_OK;
}

esp_err_t tm1637_display_set_brightness(int brightness) {
    if (brightness < 0 || brightness > 7) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < NUM_DISPLAYS; i++) {
        if (!s_states[i].initialized) {
            return ESP_ERR_INVALID_STATE;
        }

        s_states[i].brightness = brightness;

        // Re-apply display control command for each display
        start_condition(&s_states[i]);
        write_byte(&s_states[i], (uint8_t)(TM1637_CMD_DISPLAY |
                                           (s_states[i].brightness & 0x07)));
        stop_condition(&s_states[i]);
    }

    return ESP_OK;
}

esp_err_t tm1637_display_temperature(int display_id, float temperature_c) {
    if (display_id < 1 || display_id > NUM_DISPLAYS) {
        return ESP_ERR_INVALID_ARG;
    }

    tm1637_state_t *state = &s_states[display_id - 1];
    if (!state->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    int t = (int)lroundf(temperature_c);
    uint8_t digits[4] = {SEG_BLANK, SEG_BLANK, SEG_BLANK, SEG_C};

    if (t > 1023 || t < -99) {
        return tm1637_display_show_error(display_id);
    }

    if (t < 0) {
        int abs_t = -t;
        digits[0] = SEG_MINUS;
        if (abs_t >= 10) {
            digits[1] = digit_segment(abs_t / 10);
        }
        digits[2] = digit_segment(abs_t % 10);
    } else if (t >= 100) {
        digits[0] = digit_segment((t / 100) % 10);
        digits[1] = digit_segment((t / 10) % 10);
        digits[2] = digit_segment(t % 10);
    } else if (t >= 10) {
        digits[1] = digit_segment((t / 10) % 10);
        digits[2] = digit_segment(t % 10);
    } else {
        digits[2] = digit_segment(t);
    }

    return write_display_raw(state, digits);
}

esp_err_t tm1637_display_show_error(int display_id) {
    if (display_id < 1 || display_id > NUM_DISPLAYS) {
        return ESP_ERR_INVALID_ARG;
    }

    tm1637_state_t *state = &s_states[display_id - 1];
    if (!state->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t err_digits[4] = {SEG_E, SEG_R, SEG_R, SEG_BLANK};
    return write_display_raw(state, err_digits);
}

esp_err_t tm1637_display_deinit(void) {
    uint8_t blank[4] = {SEG_BLANK, SEG_BLANK, SEG_BLANK, SEG_BLANK};

    for (int i = 0; i < NUM_DISPLAYS; i++) {
        if (s_states[i].initialized) {
            (void)write_display_raw(&s_states[i], blank);
            s_states[i].initialized = false;
        }
    }

    return ESP_OK;
}
