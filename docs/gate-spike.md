# GATE-SPIKE bench kit

Status: **UNVERIFIED**. The firmware in `tools/gate-spike/` was written without an
ESP-IDF toolchain and has **not been compiled or run** (AGENTS V2). Nothing here is
HW-VERIFIED. Only a human records results, in `docs/bringup-log.md` (V1).

Authority: ADR 0015 (Proposed) and the GATE-SPIKE section of
`docs/engineering/03_task_catalog.md`. If this page disagrees with either, they win.

## Safety
No DAC and no amplifier. The kit only toggles five digital pins. Do not connect
speakers or amps (AGENTS S3).

## Equipment
- Any ESP32-S3 board, ESP-IDF v5.5.6, USB cable.
- A scope or logic analyzer that can show a 6.144 MHz clock with edge timing
  better than a few ns. BCLK period is 1 / 6.144 MHz = 162.76 ns. ADR 0015
  estimates the input-synchronizer lag at about 12.5 ns (an estimate, not a TRM
  figure), so a 100 MS/s analyzer (10 ns steps) is too coarse for item 3.
  Use a scope, or an analyzer at 500 MS/s or more (recommendation, UNVERIFIED).
- Five jumper leads and a ground lead.

## Pins (human-only, AGENTS S1)
Create `tools/gate-spike/main/spike_pins.h` locally and do not commit it:

```c
#define SPIKE_PIN_I2S_MCLK   /* GPIO number */
#define SPIKE_PIN_I2S_BCLK
#define SPIKE_PIN_I2S_WS
#define SPIKE_PIN_I2S0_DOUT  /* I2S0 master data, DAC0 and DAC1 in the product */
#define SPIKE_PIN_I2S1_DOUT  /* I2S1 slave data, DAC2 in the product (XOVER_PIN_I2S_DOUT2) */
```

Build fails with `#error` while any is missing. Pick pins that are not strapping,
flash, PSRAM, USB or console pins on your board (GATE-DOUT2 rules).

## Build and flash (ESP-IDF PowerShell)
```
cd tools\gate-spike
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

Any compile error is an author mistake, not a hardware result. Fix it or report it.
API points marked `UNVERIFIED` in `main.c`: `gpio_dump_io_configuration`,
`I2S_CLK_SRC_PLL_160M` on a slave, `i2s_channel_preload_data` repeating, and
`gpio_input_enable` leaving OE alone.

## Expected values (derived from the configuration, not measured)
| Signal | Value | Arithmetic |
|---|---|---|
| WS | 48 kHz | sample rate |
| BCLK | 6.144 MHz | 48000 x 32 x 4 |
| Frame | 128 BCK, 20.833 us | 4 x 32; 1 / 48000 |
| MCLK | 18.432 MHz | 48000 x 384 |
| I2S1 internal clock | >= 49.152 MHz | 8 x BCLK (ADR 0015 TRM item 4) |

Pattern per frame (slot 0 to 3): `0x80000001`, `0x00000000`, `0xAAAAAAAA`, `0x0000FFFF`.
Both lanes send the same pattern, so DOUT0 and DOUT1 edges should coincide up to the
lag being measured. Slot format is `PCM_SHORT`, as in `tdmout.c` today.

## Procedure
**Item 1, shared BCLK/WS pads.** Flash with `SPIKE_PAD_FIXUP 0`. From the serial log,
record the pad dump "after both enabled": IE and OE of BCLK and WS. On the scope,
BCLK and WS must keep toggling at the expected rates with I2S1 initialized.
Pass: both pads still driven by I2S0 and valid. If the pads are input-only or BCLK/WS
stop, rebuild with `-DSPIKE_PAD_FIXUP=1`, repeat, and record both runs.
If it still fails, ADR 0015 falls back (see below).

**Item 2, frame and start order.** Init returns OK without a `bclk_div` warning.
Measure MCLK, BCLK, WS. Count BCLK periods per WS period: expect 128. DOUT0 and
DOUT1 must begin on the same frame.

**Item 3, DAC2 data timing.** Probe BCLK with DOUT1 (I2S1) and with DOUT0 (I2S0) at
the ESP32 pins. Record the lag of DOUT1 behind BCLK in ns and as a fraction of
162.76 ns, and the lag of DOUT1 behind DOUT0. Compare with the PCM5122 setup and
hold times from SLAS763C. Take those figures from the datasheet and write the table
number next to them. Do not use generic I2S figures (S1).

**Item 4, what worked.** Record `SPIKE_I2S1_SLOT_MASK` and `SPIKE_I2S1_BCLK_DIV` used.
T29b needs the I2S1 slot mode and mask.

## Results (human fills in; copy the outcome to `docs/bringup-log.md`)
| Item | Expected | Measured | Pass / Fail | Notes |
|---|---|---|---|---|
| 1 BCLK/WS pad state (IE, OE) | driven by I2S0, IE set | | | |
| 1 BCLK toggling with I2S1 init | 6.144 MHz | | | |
| 2 WS | 48 kHz | | | |
| 2 BCLK per frame | 128 | | | |
| 2 MCLK | 18.432 MHz | | | |
| 2 DOUT0/DOUT1 same frame | yes | | | |
| 3 DOUT1 lag behind BCLK | datasheet table, ns | | | |
| 3 DOUT1 lag behind DOUT0 | fraction of 162.76 ns | | | |

If item 1 cannot be fixed in software, or item 3 shows DOUT1 late by one BCLK or
more or outside the PCM5122 setup and hold times: Option C (8-channel TDM DAC) or,
as a new maintainer decision, jumpered I2S1 input pads (ADR 0015). T29a to T29c and
T33 are then re-issued.

Not in this kit: the optional 8 x 32 rejection check, the DAC-side probing at the
DAC2 board, and any write to `docs/bringup-log.md`.
