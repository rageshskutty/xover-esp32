/*
 * GATE-SPIKE bench firmware (ADR 0015). No DAC and no amplifier needed.
 * Status: UNVERIFIED. Not compiled by the author; see docs/gate-spike.md.
 *
 * I2S0 = TDM master (MCLK, BCLK, WS, DOUT0). I2S1 = TDM slave on the same
 * BCLK/WS pads (DOUT1). Both: 4 slots x 32 bit, 48 kHz, mclk_multiple 384,
 * 160 MHz PLL clock source. I2S1 is enabled first, then I2S0.
 * A fixed pattern is preloaded into the DMA buffers and repeats, so edges are
 * easy to find on a logic analyzer.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "driver/gpio.h"
#include "driver/i2s_tdm.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "spike_config.h"

#define DMA_DESC_NUM 6U
#define DMA_FRAME_NUM 240U
#define PRELOAD_FRAMES (DMA_DESC_NUM * DMA_FRAME_NUM)
#define HEARTBEAT_MS 5000U
#define SETTLE_MS 200U

static const char *TAG = "gate_spike";

static const int k_pins[] = {
    SPIKE_PIN_I2S_MCLK, SPIKE_PIN_I2S_BCLK, SPIKE_PIN_I2S_WS,
    SPIKE_PIN_I2S0_DOUT, SPIKE_PIN_I2S1_DOUT,
};
#define PIN_COUNT (sizeof(k_pins) / sizeof(k_pins[0]))

/* One value per slot. Slot 0 has MSB and LSB set so bit order is visible. */
static const uint32_t k_slot_pattern[SPIKE_SLOTS] = {
    0x80000001U, 0x00000000U, 0xAAAAAAAAU, 0x0000FFFFU,
};

static uint32_t s_pattern[PRELOAD_FRAMES * SPIKE_SLOTS];

static bool pins_distinct(void)
{
    for (size_t i = 0; i < PIN_COUNT; i++) {
        for (size_t j = i + 1U; j < PIN_COUNT; j++) {
            if (k_pins[i] == k_pins[j]) {
                return false;
            }
        }
    }
    return true;
}

static void dump_pads(const char *when)
{
    uint64_t mask = 0;
    for (size_t i = 0; i < PIN_COUNT; i++) {
        mask |= 1ULL << (unsigned)k_pins[i];
    }
    ESP_LOGI(TAG, "PAD STATE %s (driver dump; record the IE and OE of BCLK and WS)", when);
    /* UNVERIFIED: gpio_dump_io_configuration() exists in v5.5.x. */
    ESP_ERROR_CHECK(gpio_dump_io_configuration(stdout, mask));
}

static void lane_init(i2s_port_t port, i2s_role_t role, i2s_tdm_slot_mask_t mask,
                      gpio_num_t mclk, gpio_num_t dout, i2s_chan_handle_t *out)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(port, role);
    chan_cfg.dma_desc_num = DMA_DESC_NUM;
    chan_cfg.dma_frame_num = DMA_FRAME_NUM;
    chan_cfg.auto_clear_after_cb = false; /* keep repeating the preloaded pattern */
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, out, NULL));

    i2s_tdm_config_t cfg = {
        .clk_cfg = I2S_TDM_CLK_DEFAULT_CONFIG(SPIKE_SAMPLE_RATE_HZ),
        .slot_cfg = I2S_TDM_PCM_SHORT_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO, mask),
        .gpio_cfg = {
            .mclk = mclk,
            .bclk = (gpio_num_t)SPIKE_PIN_I2S_BCLK,
            .ws = (gpio_num_t)SPIKE_PIN_I2S_WS,
            .dout = dout,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    /* UNVERIFIED: I2S_CLK_SRC_PLL_160M is accepted for both roles on the S3. */
    cfg.clk_cfg.clk_src = I2S_CLK_SRC_PLL_160M;
    cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_384;
    if (role == I2S_ROLE_SLAVE) {
        cfg.clk_cfg.bclk_div = SPIKE_I2S1_BCLK_DIV;
    }
    cfg.slot_cfg.total_slot = (uint32_t)SPIKE_SLOTS;
    ESP_ERROR_CHECK(i2s_channel_init_tdm_mode(*out, &cfg));

    i2s_chan_info_t info;
    ESP_ERROR_CHECK(i2s_channel_get_info(*out, &info));
    ESP_LOGI(TAG, "DRIVER INFO (not a clock measurement): port=%d role=%d mode=%d",
             (int)info.id, (int)info.role, (int)info.mode);
}

static void preload(i2s_chan_handle_t handle, const char *name)
{
    size_t loaded = 0;
    /* UNVERIFIED: preload before enable, then the DMA ring repeats it. */
    ESP_ERROR_CHECK(i2s_channel_preload_data(handle, s_pattern, sizeof(s_pattern), &loaded));
    if (loaded != sizeof(s_pattern)) {
        ESP_LOGW(TAG, "%s preload loaded %u of %u bytes", name, (unsigned)loaded,
                 (unsigned)sizeof(s_pattern));
    }
}

void app_main(void)
{
    if (!pins_distinct()) {
        ESP_LOGE(TAG, "SPIKE_PIN_* values must be five distinct GPIOs");
        abort();
    }
    for (size_t f = 0; f < PRELOAD_FRAMES; f++) {
        for (size_t s = 0; s < (size_t)SPIKE_SLOTS; s++) {
            s_pattern[f * (size_t)SPIKE_SLOTS + s] = k_slot_pattern[s];
        }
    }

    ESP_LOGI(TAG, "CONFIGURED (not measured): fs=%d Hz, %d slots x %d bit = %d BCK/frame",
             SPIKE_SAMPLE_RATE_HZ, SPIKE_SLOTS, SPIKE_SLOT_BITS, SPIKE_FRAME_BCK);
    ESP_LOGI(TAG, "EXPECTED at the pins: BCLK %d Hz, MCLK %d Hz, WS %d Hz",
             SPIKE_BCLK_HZ, SPIKE_MCLK_HZ, SPIKE_SAMPLE_RATE_HZ);
    ESP_LOGI(TAG, "EXPECTED I2S1 internal clock >= %d Hz (8 x BCLK)", SPIKE_I2S1_MIN_CLK_HZ);

    i2s_chan_handle_t i2s0 = NULL;
    i2s_chan_handle_t i2s1 = NULL;
    lane_init(I2S_NUM_0, I2S_ROLE_MASTER, (i2s_tdm_slot_mask_t)SPIKE_I2S0_SLOT_MASK,
              (gpio_num_t)SPIKE_PIN_I2S_MCLK, (gpio_num_t)SPIKE_PIN_I2S0_DOUT, &i2s0);
    lane_init(I2S_NUM_1, I2S_ROLE_SLAVE, (i2s_tdm_slot_mask_t)SPIKE_I2S1_SLOT_MASK,
              I2S_GPIO_UNUSED, (gpio_num_t)SPIKE_PIN_I2S1_DOUT, &i2s1);
    dump_pads("after init, before enable");

    preload(i2s0, "i2s0");
    preload(i2s1, "i2s1");

    /* ADR 0015: I2S1 first, then I2S0, so both begin on the same frame. */
    ESP_ERROR_CHECK(i2s_channel_enable(i2s1));
    ESP_ERROR_CHECK(i2s_channel_enable(i2s0));

#if SPIKE_PAD_FIXUP
    /* ADR 0015: BCLK and WS need IE set after both channels are enabled. */
    /* UNVERIFIED: gpio_input_enable() exists in v5.5.x and leaves OE alone. */
    ESP_ERROR_CHECK(gpio_input_enable((gpio_num_t)SPIKE_PIN_I2S_BCLK));
    ESP_ERROR_CHECK(gpio_input_enable((gpio_num_t)SPIKE_PIN_I2S_WS));
    ESP_LOGI(TAG, "SPIKE_PAD_FIXUP applied to BCLK and WS");
#endif

    vTaskDelay(pdMS_TO_TICKS(SETTLE_MS));
    dump_pads("after both enabled");

    ESP_LOGI(TAG, "RUNNING: capture BCLK, WS, DOUT0, DOUT1 now (docs/gate-spike.md)");
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_MS));
        ESP_LOGI(TAG, "still running");
    }
}
