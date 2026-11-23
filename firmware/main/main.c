#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"

#include "board_config.h"

static const char *TAG = "xover";

#if defined(CONFIG_XOVER_BRINGUP_MAX_DBFS)
_Static_assert(CONFIG_XOVER_BRINGUP_MAX_DBFS <= -30, "XOVER_BRINGUP_MAX_DBFS must not exceed -30 dBFS (AGENTS S3)");
_Static_assert(CONFIG_XOVER_BRINGUP_MAX_DBFS >= -120, "XOVER_BRINGUP_MAX_DBFS must be >= -120 dBFS");
#endif

#if defined(CONFIG_XOVER_BRINGUP_MODE_PINWALK)
static const uint32_t PINWALK_HOLD_MS = 3000U;
static const gpio_num_t pinwalk_pins[] = {
    (gpio_num_t)XOVER_PIN_I2S_MCLK,
    (gpio_num_t)XOVER_PIN_I2S_BCLK,
    (gpio_num_t)XOVER_PIN_I2S_WS,
    (gpio_num_t)XOVER_PIN_I2S_DOUT,
    (gpio_num_t)XOVER_PIN_I2C_SDA,
    (gpio_num_t)XOVER_PIN_I2C_SCL,
    (gpio_num_t)XOVER_PIN_ADMIN_LED,
#if defined(CONFIG_XOVER_BRINGUP_PINWALK_MUTE)
    (gpio_num_t)XOVER_PIN_DAC_MUTE_N,
#endif
};
#elif defined(CONFIG_XOVER_BRINGUP_MODE_I2CSCAN)
static const uint32_t I2CSCAN_INTERVAL_MS = 2000U;
static const int I2C_PROBE_TIMEOUT_MS = 50;
static const uint16_t I2C_SCAN_ADDR_FIRST = 0x08U;
static const uint16_t I2C_SCAN_ADDR_LAST = 0x77U;
#else
static const uint32_t IDLE_DELAY_MS = 1000U;
#endif

void app_main(void)
{
    /* 1. First statement: configure DAC mute pin as output and drive it low (AGENTS S2) */
    ESP_ERROR_CHECK(gpio_set_level((gpio_num_t)XOVER_PIN_DAC_MUTE_N, 0));
    const gpio_config_t mute_cfg = {
        .pin_bit_mask = (1ULL << XOVER_PIN_DAC_MUTE_N),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&mute_cfg));
    ESP_ERROR_CHECK(gpio_set_level((gpio_num_t)XOVER_PIN_DAC_MUTE_N, 0));

#if defined(CONFIG_XOVER_BRINGUP_MODE_PINWALK)
    /* 2. PINWALK mode */
    const size_t pinwalk_count = sizeof(pinwalk_pins) / sizeof(pinwalk_pins[0]);

    const gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << XOVER_PIN_ADMIN_BUTTON),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&btn_cfg));

    ESP_LOGI(TAG, "bringup mode: PINWALK");
    while (1) {
        for (size_t i = 0U; i < pinwalk_count; i++) {
            const gpio_num_t pin = pinwalk_pins[i];
            const int btn = gpio_get_level((gpio_num_t)XOVER_PIN_ADMIN_BUTTON);
            ESP_LOGI(TAG, "PINWALK button gpio0=%d", btn);

            const gpio_config_t out_cfg = {
                .pin_bit_mask = (1ULL << pin),
                .mode = GPIO_MODE_OUTPUT,
                .pull_up_en = GPIO_PULLUP_DISABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .intr_type = GPIO_INTR_DISABLE,
            };
            ESP_ERROR_CHECK(gpio_config(&out_cfg));
            ESP_ERROR_CHECK(gpio_set_level(pin, 1));
            ESP_LOGI(TAG, "PINWALK gpio=%d high", (int)pin);
            vTaskDelay(pdMS_TO_TICKS(PINWALK_HOLD_MS));

            ESP_ERROR_CHECK(gpio_set_level(pin, 0));
            ESP_LOGI(TAG, "PINWALK gpio=%d low", (int)pin);
            ESP_ERROR_CHECK(gpio_set_direction(pin, GPIO_MODE_INPUT));
        }
    }

#elif defined(CONFIG_XOVER_BRINGUP_MODE_I2CSCAN)
    /* 3. I2CSCAN mode */
    _Static_assert(XOVER_I2C_FREQ_HZ == 100000, "XOVER_I2C_FREQ_HZ must be 100000 Hz");

    const i2c_master_bus_config_t i2c_bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = (gpio_num_t)XOVER_PIN_I2C_SDA,
        .scl_io_num = (gpio_num_t)XOVER_PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7U,
        .intr_priority = 0,
        .trans_queue_depth = 0U,
        .flags.enable_internal_pullup = false,
    };
    i2c_master_bus_handle_t bus_handle = NULL;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &bus_handle));

    ESP_LOGI(TAG, "bringup mode: I2CSCAN");
    while (1) {
        uint32_t hit_count = 0U;
        bool hit_dac0 = false;
        bool hit_dac1 = false;
        bool hit_dac2 = false;
        bool hit_other = false;

        for (uint16_t addr = I2C_SCAN_ADDR_FIRST; addr <= I2C_SCAN_ADDR_LAST; addr++) {
            const esp_err_t ret = i2c_master_probe(bus_handle, addr, I2C_PROBE_TIMEOUT_MS);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "I2CSCAN found 0x%02x", (unsigned int)addr);
                hit_count++;
                if (addr == (uint16_t)XOVER_DAC0_I2C_ADDR) {
                    hit_dac0 = true;
                } else if (addr == (uint16_t)XOVER_DAC1_I2C_ADDR) {
                    hit_dac1 = true;
                } else if (addr == (uint16_t)XOVER_DAC2_I2C_ADDR) {
                    hit_dac2 = true;
                } else {
                    hit_other = true;
                }
            }
        }

        if (hit_count == (uint32_t)XOVER_DAC_COUNT && hit_dac0 && hit_dac1 && hit_dac2 && !hit_other) {
            ESP_LOGI(TAG, "I2CSCAN PASS");
        } else {
            ESP_LOGI(TAG, "I2CSCAN FAIL");
        }

        vTaskDelay(pdMS_TO_TICKS(I2CSCAN_INTERVAL_MS));
    }

#else
    /* 4. NONE mode (default) */
    ESP_LOGI(TAG, "bringup mode: NONE (idle)");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(IDLE_DELAY_MS));
    }
#endif
}
