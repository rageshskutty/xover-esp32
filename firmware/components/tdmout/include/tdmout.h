#ifndef TDMOUT_H
#define TDMOUT_H

#include <stddef.h>
#include <stdint.h>

#if defined(ESP_PLATFORM)
#include "esp_err.h"
#else
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_TIMEOUT 0x107
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initialize I2S0 peripheral in TDM master TX mode.
 * Configured for 48 kHz, 8 slots of 32 bits (slots 0-5 active, slot mask 0x3F),
 * with short 1-BCK sync pulse and MCLK = BCLK = 12.288 MHz (256x fS, bck_div = 1).
 * Returns ESP_OK on success, or ESP_ERR_INVALID_STATE if already initialized.
 */
esp_err_t tdmout_init(void);

/*
 * Start I2S TX channel DMA transmission.
 * Returns ESP_OK on success, or ESP_ERR_INVALID_STATE if not initialized or already running.
 */
esp_err_t tdmout_start(void);

/*
 * Stop I2S TX channel DMA transmission.
 * Returns ESP_OK on success, or ESP_ERR_INVALID_STATE if not initialized or not running.
 */
esp_err_t tdmout_stop(void);

/*
 * Deinitialize and release I2S channel resources.
 * Returns ESP_OK on success, or ESP_ERR_INVALID_STATE if not initialized.
 */
esp_err_t tdmout_deinit(void);

/*
 * Write interleaved 8-slot 32-bit frames to the I2S TX DMA buffer.
 * Blocking call. timeout_ms: milliseconds to wait (or UINT32_MAX for portMAX_DELAY).
 * bytes_written: optional out-pointer for actual bytes transferred.
 * Validates non-NULL src, non-zero size_bytes, and alignment to 32-byte frame boundaries.
 * Returns ESP_OK on success, ESP_ERR_INVALID_ARG on bad arguments,
 * ESP_ERR_INVALID_STATE if not running, or ESP_ERR_TIMEOUT.
 */
esp_err_t tdmout_write(const void *src, size_t size_bytes, size_t *bytes_written, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* TDMOUT_H */
