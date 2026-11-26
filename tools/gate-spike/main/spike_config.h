#pragma once
/*
 * GATE-SPIKE configuration (ADR 0015, docs/gate-spike.md).
 * Bench kit only. Not part of the product firmware.
 *
 * Pins are human-only facts (AGENTS S1). Create tools/gate-spike/main/spike_pins.h
 * locally with the five SPIKE_PIN_* defines below. Do not commit it.
 */
#include "esp_idf_version.h"

#if ESP_IDF_VERSION_MAJOR != 5 || ESP_IDF_VERSION_MINOR != 5
#error "gate-spike requires ESP-IDF v5.5.x"
#endif

#if __has_include("spike_pins.h")
#include "spike_pins.h"
#endif

#ifndef SPIKE_PIN_I2S_MCLK
#error "SPIKE_PIN_I2S_MCLK not set: define it in spike_pins.h (human-only pin fact)"
#endif
#ifndef SPIKE_PIN_I2S_BCLK
#error "SPIKE_PIN_I2S_BCLK not set: define it in spike_pins.h (human-only pin fact)"
#endif
#ifndef SPIKE_PIN_I2S_WS
#error "SPIKE_PIN_I2S_WS not set: define it in spike_pins.h (human-only pin fact)"
#endif
#ifndef SPIKE_PIN_I2S0_DOUT
#error "SPIKE_PIN_I2S0_DOUT not set: I2S0 master data pin, carries DAC0 and DAC1 in the product"
#endif
#ifndef SPIKE_PIN_I2S1_DOUT
#error "SPIKE_PIN_I2S1_DOUT not set: I2S1 slave data pin, carries DAC2 in the product (XOVER_PIN_I2S_DOUT2)"
#endif

/* Configuration from ADR 0015. */
#define SPIKE_SAMPLE_RATE_HZ 48000
#define SPIKE_SLOT_BITS 32
#define SPIKE_SLOTS 4
#define SPIKE_FRAME_BCK (SPIKE_SLOT_BITS * SPIKE_SLOTS)
#define SPIKE_MCLK_MULTIPLE 384
#define SPIKE_BCLK_HZ (SPIKE_SAMPLE_RATE_HZ * SPIKE_FRAME_BCK)
#define SPIKE_MCLK_HZ (SPIKE_SAMPLE_RATE_HZ * SPIKE_MCLK_MULTIPLE)
/* ADR 0015 TRM check item 4: slave internal clock >= 8 x BCK. */
#define SPIKE_I2S1_MIN_CLK_HZ (8 * SPIKE_BCLK_HZ)

_Static_assert(SPIKE_FRAME_BCK == 128, "frame must be 128 BCK (ADR 0015)");
_Static_assert(SPIKE_MCLK_MULTIPLE == 384, "mclk_multiple must be 384 (ADR 0015)");

/* Overridable for the bench. Record whatever worked in docs/bringup-log.md. */
#ifndef SPIKE_I2S0_SLOT_MASK
#define SPIKE_I2S0_SLOT_MASK 0x0F
#endif
#ifndef SPIKE_I2S1_SLOT_MASK
#define SPIKE_I2S1_SLOT_MASK 0x0F
#endif
#ifndef SPIKE_I2S1_BCLK_DIV
#define SPIKE_I2S1_BCLK_DIV 8 /* IDF guide: slave bclk_div >= 8 */
#endif
/* 0: leave the pads as the driver left them (item 1, first run).
 * 1: after both channels are enabled, call gpio_input_enable() on BCLK and WS. */
#ifndef SPIKE_PAD_FIXUP
#define SPIKE_PAD_FIXUP 0
#endif
