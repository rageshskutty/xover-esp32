# ADR 0015: TDM feed for three PCM5122 (two 4 x 32-bit lanes)

- Status: Proposed (maintainer chose Option A on 2026-11-24; becomes Accepted after the Step 1 bench check in docs/review/2026-11-24-correction-plan.md)
- Date: 2026-11-24
- Task: review correction plan Steps 1-5 (Q6)

## Context
The repo assumed one 8-slot x 32-bit TDM frame (256 BCK) from the ESP32-S3 to
three PCM5122 (6 channels). ESP-IDF v5.5.1 (`i2s_ll.h`, `i2s_tdm.c`, I2S
programming guide; SOURCE-VERIFIED, v5.5.6 and the S3 TRM not checked) limits an S3
TDM frame to 128 bit (`I2S_LL_SLOT_FRAME_BIT_MAX`), so 8 x 32 bit fails with
`ESP_ERR_INVALID_ARG` in `tdmout_init()`. The PCM5122 needs 128 or 256 BCK per
frame, so 6 x 32 = 192 is not legal either.

## Decision
Option A. I2S0 is the master with a 128-BCK frame (4 slots x 32 bit) and
carries DAC0 and DAC1 on DOUT0 (offsets 0 and 64). I2S1 runs as a slave on the
same BCLK/WS and carries DAC2 on its own DOUT1 (offset 0). Start I2S1 first,
then I2S0, so both begin on the same frame. This costs one GPIO and one jumper
(BCLK/WS looped to the I2S1 inputs).

MCLK is 384 x fS = 18.432 MHz (`mclk_multiple = 384`, master `bclk_div` = 3),
using the stock IDF clock path with no private `i2s_ll_*` calls. The
`pcm5122_status_is_running_48k_tdm` check derives `bck_per_frame` and
`sck_ratio_code` from the configured values instead of hard-coding 256 BCK and
256 x fS. Fallback if the PCM5122 datasheet does not allow 384 x fS: 512 x fS
(24.576 MHz, divider 4). The fallback is not needed, see Datasheet check below.

## Datasheet check (done 2026-11-24, SLAS763C, PCM5121/PCM5122)
384 x fS at 48 kHz is allowed. Table 32: 48 kHz row, 384 fS column = 18.432 MHz,
no PLL footnote (supported without the PLL), below the 50 MHz SCK limit.
Table 104 (Reg 0x5B, DTSR): code 0111 = SCK 384 FS. Table 133 (SCK as master
clock, 48 kHz, RSCK 384): NMAC 1, NDAC 3, DOSR 8, NCP 4. Checked against the
datasheet page images by the maintainer.

## Alternatives considered
- B, 8 x 16 bit: fits 128 BCK but gives a 96 dB floor. Acceptable for the first tone test only.
- C, an 8-channel TDM DAC: hardware change, kept as the fallback if the alignment check fails.
- Keep the divider-2 override (`i2s_ll_tx_set_bck_div_num(..., 1)`): relies on a divider the S3 TRM has not been checked for, and fights IDF's own `bclk_div <= 2` clamp.
- 256 x fS through the stock path: not possible, IDF bumps the divider to 3 and gives 384 x fS plus a warning.

## Consequences (include measured data when changing a gate)
UNVERIFIED until the bench check passes: (1) DAC2 data is not one or more BCLK
late (the IDF guide says a TDM slave needs `bclk_div` >= 8 and its data can lag
by more than one BCLK), (2) the driver does not reconfigure the shared BCLK/WS
pins as input-only. Item (3), the PCM5122 SCK table allowing the chosen ratio,
is verified (see Datasheet check). If (1) or (2) fails, switch to Option C.
Code and doc changes: `board_config.h`
(4 slots, DOUT2, per-lane offset assertion), `tdm_pack_4slot`, `tdmout`
(two lanes, start order), `pcm5122_ctl`, `hw-pinmap.md`,
`hw-verification-notes.md`, `bom.md`.
