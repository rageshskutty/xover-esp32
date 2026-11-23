#include "tdmout.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_idf_version.h"

#if ESP_IDF_VERSION_MAJOR != 5 || ESP_IDF_VERSION_MINOR != 5
#error "tdmout requires ESP-IDF v5.5.x"
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_tdm.h"
#include "hal/i2s_ll.h"

#include "board_config.h"

_Static_assert(XOVER_SAMPLE_RATE_HZ == 48000, "sample rate must be 48 kHz");
_Static_assert(XOVER_TDM_SLOT_BITS == 32, "slot bits must be 32");
_Static_assert(XOVER_TDM_SLOTS_PER_FRAME == 8, "slots per frame must be 8");
_Static_assert(XOVER_TDM_SLOT_MASK == 0x3F, "slot mask must be 0x3F");

#define TDMOUT_BYTES_PER_SLOT (sizeof(int32_t))
#define TDMOUT_FRAME_SIZE_BYTES ((size_t)(XOVER_TDM_SLOTS_PER_FRAME * TDMOUT_BYTES_PER_SLOT))

/* 160 MHz PLL / (13 + 1/48) = 12.288 MHz exact MCLK */
#define TDMOUT_MCLK_DIV_INT 13U
#define TDMOUT_MCLK_DIV_NUM 1U
#define TDMOUT_MCLK_DIV_DEN 48U
#define TDMOUT_BCK_DIV 1U

static i2s_chan_handle_t s_tx_chan = NULL;
static bool s_running = false;

static void tdmout_apply_clock_override(void)
{
    const hal_utils_clk_div_t mclk_div = {
        .integer = TDMOUT_MCLK_DIV_INT,
        .numerator = TDMOUT_MCLK_DIV_NUM,
        .denominator = TDMOUT_MCLK_DIV_DEN,
    };
    i2s_dev_t *hw = I2S_LL_GET_HW(0);
    i2s_ll_tx_set_mclk(hw, &mclk_div);
    i2s_ll_tx_set_bck_div_num(hw, TDMOUT_BCK_DIV);
}

esp_err_t tdmout_init(void)
{
    if (s_tx_chan != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 6,
        .dma_frame_num = 240,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .allow_pd = false,
        .intr_priority = 0,
    };

    esp_err_t ret = i2s_new_channel(&chan_cfg, &s_tx_chan, NULL);
    if (ret != ESP_OK) {
        return ret;
    }

    i2s_tdm_config_t tdm_cfg = {
        .clk_cfg = I2S_TDM_CLK_DEFAULT_CONFIG(XOVER_SAMPLE_RATE_HZ),
        .slot_cfg = I2S_TDM_PCM_SHORT_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO, (i2s_tdm_slot_mask_t)XOVER_TDM_SLOT_MASK),
        .gpio_cfg = {
            .mclk = (gpio_num_t)XOVER_PIN_I2S_MCLK,
            .bclk = (gpio_num_t)XOVER_PIN_I2S_BCLK,
            .ws = (gpio_num_t)XOVER_PIN_I2S_WS,
            .dout = (gpio_num_t)XOVER_PIN_I2S_DOUT,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    tdm_cfg.slot_cfg.total_slot = XOVER_TDM_SLOTS_PER_FRAME;

    ret = i2s_channel_init_tdm_mode(s_tx_chan, &tdm_cfg);
    if (ret != ESP_OK) {
        (void)i2s_del_channel(s_tx_chan);
        s_tx_chan = NULL;
        return ret;
    }

    tdmout_apply_clock_override();
    s_running = false;
    return ESP_OK;
}

esp_err_t tdmout_start(void)
{
    if (s_tx_chan == NULL || s_running) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = i2s_channel_enable(s_tx_chan);
    if (ret == ESP_OK) {
        s_running = true;
    }
    return ret;
}

esp_err_t tdmout_stop(void)
{
    if (s_tx_chan == NULL || !s_running) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = i2s_channel_disable(s_tx_chan);
    if (ret == ESP_OK) {
        s_running = false;
    }
    return ret;
}

esp_err_t tdmout_deinit(void)
{
    if (s_tx_chan == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_running) {
        (void)tdmout_stop();
    }
    esp_err_t ret = i2s_del_channel(s_tx_chan);
    s_tx_chan = NULL;
    s_running = false;
    return ret;
}

esp_err_t tdmout_write(const void *src, size_t size_bytes, size_t *bytes_written, uint32_t timeout_ms)
{
    if (s_tx_chan == NULL || !s_running) {
        return ESP_ERR_INVALID_STATE;
    }
    if (src == NULL || size_bytes == 0U || (size_bytes % TDMOUT_FRAME_SIZE_BYTES) != 0U) {
        return ESP_ERR_INVALID_ARG;
    }
    const TickType_t ticks = (timeout_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return i2s_channel_write(s_tx_chan, src, size_bytes, bytes_written, ticks);
}
