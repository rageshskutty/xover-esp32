# ADR 0015: TDM feed for three PCM5122 (two 4 x 32-bit lanes)

- Status: Proposed (maintainer chose Option A on 2026-11-24; becomes Accepted after the Step 1 bench check in docs/review/2026-11-24-correction-plan.md, as narrowed by GATE-SPIKE in docs/engineering/03_task_catalog.md)
- Date: 2026-11-24
- Amended: 2026-11-25 (S3 TRM v1.8 check added, docs only; the decision is unchanged)
- Task: review correction plan Steps 1-5 (Q6)

## Context
The repo assumed one 8-slot x 32-bit TDM frame (256 BCK) from the ESP32-S3 to
three PCM5122 (6 channels). ESP-IDF v5.5.1 (`i2s_ll.h`, `i2s_tdm.c`, I2S
programming guide; SOURCE-VERIFIED, v5.5.6 not checked; the S3 TRM check is in
the TRM check section below) limits an S3 TDM frame to 128 bit
(`I2S_LL_SLOT_FRAME_BIT_MAX`), so 8 x 32 bit fails with `ESP_ERR_INVALID_ARG` in
`tdmout_init()`. The PCM5122 needs 128 or 256 BCK per frame, so 6 x 32 = 192 is
not legal either.

## Decision
Option A. I2S0 is the master with a 128-BCK frame (4 slots x 32 bit) and
carries DAC0 and DAC1 on DOUT0 (offsets 0 and 64). I2S1 runs as a slave on the
same BCLK/WS and carries DAC2 on its own DOUT1 (offset 0). Start I2S1 first,
then I2S0, so both begin on the same frame. This costs one GPIO (the DAC2 data
line). I2S1 must run from the 160 MHz PLL clock source, not the 40 MHz crystal:
as a slave its internal clock must be at least 8 x BCK = 49.152 MHz (see TRM
check).

The first version of this ADR said the design also costs "one jumper (BCLK/WS
looped to the I2S1 inputs)". The TRM check below shows the S3 hardware does not
need a jumper, because I2S1's slave inputs can read the same pads that I2S0
drives. The jumper stays as the fallback only if the bench check of the shared
pads fails; it would use extra GPIOs, so it needs a new maintainer decision
(AGENTS S1) and is not part of Option A as written here.

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

## TRM check (done 2026-11-25, ESP32-S3 TRM v1.8, SOURCE-VERIFIED)
Labels follow AGENTS V1: SOURCE-VERIFIED means the cited TRM text was read;
nothing here is HW-VERIFIED. Chapter 28 (I2S controller) was read from the PDF
by OCR, so a reviewer should spot-check the section numbers below against the
PDF. Chapter 6 (IO MUX and GPIO matrix) was read from the PDF text.

1. **128-bit frame limit is a hardware limit, not only an IDF constant.**
   `I2S_TX_HALF_SAMPLE_BITS` is a 6-bit field (bits 23:18 of `I2S_TX_CONF1_REG`,
   Register 28.13, printed p. 1069), and the TRM says this value x 2 is the
   number of BCK cycles in one WS period. 8 x 32 = 256 cannot be programmed.
   4 x 32 = 128 fits with zero margin. Caveat: read literally, a 6-bit field
   with "value x 2" gives at most 126. The reset value 0xF only makes sense as
   a 32-BCK frame (16-bit stereo) if the frame is (value + 1) x 2, which
   matches IDF's 128. The bench check (GATE-SPIKE item 2) confirms 128 works.
2. **Slots.** TDM supports up to 16 channels and `TX_TDM_CHAN_BITS` is 5 bits
   (up to 32 bit per slot). The total bits per frame is the constraint, so
   Option B (8 x 16 bit) would also fit.
3. **BCK divider (section 28.6).** BCK = TX_CLK / (`TX_BCK_DIV_NUM` + 1), and
   `TX_BCK_DIV_NUM` must not be configured as 1. The divider-2 override of the
   old `tdmout.c` (`i2s_ll_tx_set_bck_div_num(..., 1)`) is therefore invalid on
   hardware as well as fighting IDF's clamp. Divider 3 is legal and is what the
   stock path uses.
4. **I2S1 as slave needs a fast clock.** In slave mode TX_CLK must be at least
   8 x f_BCK. At 384 x 48 kHz / 3, BCK is 6.144 MHz, so I2S1's clock must be at
   least 49.152 MHz. The 40 MHz crystal violates this; the 160 MHz PLL source
   gives a ratio of about 26. This matches the IDF guide's `bclk_div` >= 8 rule
   for TDM slaves.
5. **Pad sharing is allowed by the hardware (chapter 6).** All I2S signals go
   through the GPIO matrix only (Table 6.11-1, "Direct ... via IO MUX" = no):
   I2S0 outputs `I2S0O_BCK_out` (signal 22) and `I2S0O_WS_out` (24); I2S1
   slave inputs are `I2S1O_BCK_in` (28) and `I2S1O_WS_in` (29); I2S1 data out
   is `I2S1O_SD_out` (30). A pad has separate input-enable (IE) and
   output-enable (OE) (Figure 6.3-2). One pin input can feed several
   peripheral inputs (section 6.4.3, note 1) and the input needs
   `IO_MUX_FUN_IE` = 1 (section 6.4.3, step 4). The TRM never says in words
   that the input works while the same pad's output is enabled; that is read
   from Figure 6.3-2, where the input path taps the pad node.
   Open and software-side: whether the IDF driver, when it initializes I2S1 as
   a slave, sets the shared BCLK/WS pads to input-only and so switches off
   I2S0's outputs. Not checked in IDF source.
6. **Input synchronizer.** Section 6.4.2 says matrix inputs are synchronized to
   the APB clock, with two optional stages (`GPIO_PINn_SYNC1_BYPASS`,
   `GPIO_PINn_SYNC2_BYPASS`, Register 6.18). The register text says 0 = no
   synchronization and both fields reset to 0, so by default no synchronization
   delay is added; the section 6.4.2 text can be read the other way. If a
   stage is enabled it costs up to about one APB clock (12.5 ns at 80 MHz;
   an estimate, not a TRM figure). BCK is 163 ns per period, so the lag of
   I2S1's data is a fraction of one BCLK, not a whole one. It is still a
   bench measurement, not an assumption.

## Alternatives considered
- B, 8 x 16 bit: fits 128 BCK but gives a 96 dB floor. Acceptable for the first tone test only.
- C, an 8-channel TDM DAC: hardware change, kept as the fallback if the alignment check fails.
- Keep the divider-2 override (`i2s_ll_tx_set_bck_div_num(..., 1)`): the TRM says `TX_BCK_DIV_NUM` must not be 1 (TRM check item 3), and it fights IDF's own `bclk_div <= 2` clamp.
- 256 x fS through the stock path: not possible, IDF bumps the divider to 3 and gives 384 x fS plus a warning.
- Separate I2S1 input pads jumpered to BCLK/WS: not needed by the hardware (TRM check item 5); fallback only if the shared-pad bench check fails.

## Consequences (include measured data when changing a gate)
Verified from the TRM text (SOURCE-VERIFIED, still not HW-VERIFIED): the
128-bit limit, the invalid divider value 1, the slave clock rule, and that
the pads can be shared. The PCM5122 SCK table allowing the chosen ratio is
verified (see Datasheet check).

UNVERIFIED until the bench check passes: (1) DAC2's data timing against BCLK
at the DAC, measured as setup and hold against the PCM5122 timing table in
SLAS763C (take the figures from the datasheet, not from memory); the TRM
suggests a fraction of one BCLK of lag, the IDF guide warns of more than one;
(2) the pad state after both I2S channels are initialized: BCLK and WS must
still be driven by I2S0 while I2S1 reads them, which needs IE and OE both set
on those pads, and the start order of I2S1 then I2S0. If (1) or (2) fails and
a software fix does not help, switch to Option C or take the jumper fallback
as a new maintainer decision.
Code and doc changes: `board_config.h`
(4 slots, DOUT2, per-lane offset assertion), `tdm_pack_4slot`, `tdmout`
(two lanes, start order, I2S1 on the 160 MHz clock source, pad IE/OE after both
channels are enabled), `pcm5122_ctl`, `hw-pinmap.md`,
`hw-verification-notes.md`, `bom.md`.
