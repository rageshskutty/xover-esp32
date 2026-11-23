/*
 * board_config.h - board facts for the xover-esp32 bring-up hardware.
 *
 * Board: ESP32-S3-WROOM-1-N16R8 dev board with 3 x Adafruit PCM5122 I2S DAC
 * breakouts on one TDM bus. The wiring table and the sources are in
 * docs/hw-pinmap.md.
 *
 * Status: PROPOSED from datasheets, UNVERIFIED until a human records the
 * wiring check in docs/bringup-log.md. This file is a CODEOWNERS path.
 */
#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/* I2S / TDM bus, shared by all three DACs (GPIO numbers). */
#define XOVER_PIN_I2S_MCLK 4
#define XOVER_PIN_I2S_BCLK 5
#define XOVER_PIN_I2S_WS 6
#define XOVER_PIN_I2S_DOUT 7

/* I2C control bus for the DAC registers. The DAC boards carry the pull-ups. */
#define XOVER_PIN_I2C_SDA 8
#define XOVER_PIN_I2C_SCL 9
#define XOVER_I2C_FREQ_HZ 100000 /* bring-up value; try 400000 after a scope check */

/* DAC mute, active low (low = muted). Needs the external pull-down. */
#define XOVER_PIN_DAC_MUTE_N 10

/* Admin window (ADR 0014 P3). The BOOT key is read only after boot has finished. */
#define XOVER_PIN_ADMIN_BUTTON 0
#define XOVER_PIN_ADMIN_LED 21

/* TDM frame: 48 kHz, 8 slots x 32 bit = 256 BCK = 12.288 MHz. Slots 0-5 carry audio. */
#define XOVER_SAMPLE_RATE_HZ 48000
#define XOVER_TDM_SLOT_BITS 32
#define XOVER_TDM_SLOTS_PER_FRAME 8
#define XOVER_TDM_SLOT_MASK 0x3F

/* DAC boards: 7-bit I2C address and TDM data offset in BCK from the frame start. */
#define XOVER_DAC_COUNT 3
#define XOVER_DAC0_I2C_ADDR 0x4C /* A1 low,  A2 low  */
#define XOVER_DAC1_I2C_ADDR 0x4D /* A1 high, A2 low  */
#define XOVER_DAC2_I2C_ADDR 0x4E /* A1 low,  A2 high */
#define XOVER_DAC0_TDM_OFFSET_BCK 0
#define XOVER_DAC1_TDM_OFFSET_BCK 64
#define XOVER_DAC2_TDM_OFFSET_BCK 128

/* ---- Compile-time checks. A missing value or a bad pin fails the build. ---- */

#ifndef XOVER_PIN_I2S_MCLK
#error "XOVER_PIN_I2S_MCLK is not set; see docs/hw-pinmap.md"
#endif
#ifndef XOVER_PIN_I2S_BCLK
#error "XOVER_PIN_I2S_BCLK is not set; see docs/hw-pinmap.md"
#endif
#ifndef XOVER_PIN_I2S_WS
#error "XOVER_PIN_I2S_WS is not set; see docs/hw-pinmap.md"
#endif
#ifndef XOVER_PIN_I2S_DOUT
#error "XOVER_PIN_I2S_DOUT is not set; see docs/hw-pinmap.md"
#endif
#ifndef XOVER_PIN_I2C_SDA
#error "XOVER_PIN_I2C_SDA is not set; see docs/hw-pinmap.md"
#endif
#ifndef XOVER_PIN_I2C_SCL
#error "XOVER_PIN_I2C_SCL is not set; see docs/hw-pinmap.md"
#endif
#ifndef XOVER_PIN_DAC_MUTE_N
#error "XOVER_PIN_DAC_MUTE_N is not set; see docs/hw-pinmap.md"
#endif
#ifndef XOVER_PIN_ADMIN_BUTTON
#error "XOVER_PIN_ADMIN_BUTTON is not set; see docs/hw-pinmap.md"
#endif
#ifndef XOVER_PIN_ADMIN_LED
#error "XOVER_PIN_ADMIN_LED is not set; see docs/hw-pinmap.md"
#endif

/* Pins the N16R8 module cannot give to the application: flash and octal PSRAM
 * (GPIO26-37), native USB (19, 20) and the UART0 console (43, 44). */
#define XOVER_PIN_UNUSABLE(p) \
    (((p) >= 26 && (p) <= 37) || (p) == 19 || (p) == 20 || (p) == 43 || (p) == 44)
/* Strapping pins (0, 3, 45, 46) are sampled at reset. Only the admin button may sit on one. */
#define XOVER_PIN_STRAPPING(p) ((p) == 0 || (p) == 3 || (p) == 45 || (p) == 46)
#define XOVER_PIN_BAD_FOR_SIGNAL(p) (XOVER_PIN_UNUSABLE(p) || XOVER_PIN_STRAPPING(p))

#if XOVER_PIN_BAD_FOR_SIGNAL(XOVER_PIN_I2S_MCLK) || XOVER_PIN_BAD_FOR_SIGNAL(XOVER_PIN_I2S_BCLK) || \
    XOVER_PIN_BAD_FOR_SIGNAL(XOVER_PIN_I2S_WS) || XOVER_PIN_BAD_FOR_SIGNAL(XOVER_PIN_I2S_DOUT) || \
    XOVER_PIN_BAD_FOR_SIGNAL(XOVER_PIN_I2C_SDA) || XOVER_PIN_BAD_FOR_SIGNAL(XOVER_PIN_I2C_SCL) || \
    XOVER_PIN_BAD_FOR_SIGNAL(XOVER_PIN_DAC_MUTE_N) || XOVER_PIN_BAD_FOR_SIGNAL(XOVER_PIN_ADMIN_LED)
#error "a signal pin is a strapping, flash, PSRAM, USB or console pin; see docs/hw-pinmap.md"
#endif
#if XOVER_PIN_UNUSABLE(XOVER_PIN_ADMIN_BUTTON)
#error "the admin button pin is not usable on this module; see docs/hw-pinmap.md"
#endif

/* Every pin must be different: with distinct bits, the sum equals the OR. */
#define XOVER_PIN_BIT(p) (1ULL << (p))
#if (XOVER_PIN_BIT(XOVER_PIN_I2S_MCLK) + XOVER_PIN_BIT(XOVER_PIN_I2S_BCLK) + XOVER_PIN_BIT(XOVER_PIN_I2S_WS) + \
     XOVER_PIN_BIT(XOVER_PIN_I2S_DOUT) + XOVER_PIN_BIT(XOVER_PIN_I2C_SDA) + XOVER_PIN_BIT(XOVER_PIN_I2C_SCL) + \
     XOVER_PIN_BIT(XOVER_PIN_DAC_MUTE_N) + XOVER_PIN_BIT(XOVER_PIN_ADMIN_BUTTON) + XOVER_PIN_BIT(XOVER_PIN_ADMIN_LED)) != \
    (XOVER_PIN_BIT(XOVER_PIN_I2S_MCLK) | XOVER_PIN_BIT(XOVER_PIN_I2S_BCLK) | XOVER_PIN_BIT(XOVER_PIN_I2S_WS) | \
     XOVER_PIN_BIT(XOVER_PIN_I2S_DOUT) | XOVER_PIN_BIT(XOVER_PIN_I2C_SDA) | XOVER_PIN_BIT(XOVER_PIN_I2C_SCL) | \
     XOVER_PIN_BIT(XOVER_PIN_DAC_MUTE_N) | XOVER_PIN_BIT(XOVER_PIN_ADMIN_BUTTON) | XOVER_PIN_BIT(XOVER_PIN_ADMIN_LED))
#error "two signals share one GPIO; see docs/hw-pinmap.md"
#endif

/* PCM5122 TDM runs up to 48 kHz only with BCK = 128 or 256 x fS (TI SLAS763C, audio-interface table). */
#if (XOVER_TDM_SLOT_BITS * XOVER_TDM_SLOTS_PER_FRAME) != 256 && (XOVER_TDM_SLOT_BITS * XOVER_TDM_SLOTS_PER_FRAME) != 128
#error "TDM frame must be 128 or 256 BCK per sample for the PCM5122 at 48 kHz"
#endif
#if XOVER_DAC1_TDM_OFFSET_BCK != (2 * XOVER_TDM_SLOT_BITS) || XOVER_DAC2_TDM_OFFSET_BCK != (4 * XOVER_TDM_SLOT_BITS)
#error "DAC TDM offsets must follow two slots per DAC"
#endif

#endif /* BOARD_CONFIG_H */
