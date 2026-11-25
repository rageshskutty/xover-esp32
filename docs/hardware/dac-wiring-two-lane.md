# PCM5122 x3 wiring for the two-lane TDM feed (ADR 0015)

Status: PROPOSED. Nothing here is HW-VERIFIED. This note does not change
`docs/hw-pinmap.md` or `firmware/main/board_config.h` (both human-owned) and it
does not accept ADR 0015. It shows what the wiring becomes if ADR 0015 is
accepted, and lists what in `hw-pinmap.md` is then out of date.

Sources: `docs/hardware/adafruit-pcm5122-breakout.md` (pad names, straps,
CLK Dir jumper), `docs/adr/0015-tdm-feed-two-lanes.md`,
`docs/hardware/dout2-candidates.md` (DOUT2 pin options), TI SLAS763C as cited
in those files.

## 1. What ADR 0015 changes on the DAC side

| Item | Now in hw-pinmap.md | Under ADR 0015 |
|---|---|---|
| Frame | 8 slots x 32 bit = 256 BCK | 4 slots x 32 bit = 128 BCK per lane |
| BCK | 12.288 MHz | 6.144 MHz (128 x 48 kHz) |
| MCLK (MCK pad) | 12.288 MHz (256 x fS) | 18.432 MHz (384 x fS) |
| DIN of DAC0, DAC1 | one data line (GPIO7) | one data line (GPIO7, lane 0) |
| DIN of DAC2 | same data line | its own data line, DOUT2 (lane 1) |
| DAC offsets (Reg 0x29) | 0 / 64 / 128 | DAC0 = 0, DAC1 = 64, DAC2 = 0 |
| Extra jumper | none | none (I2S1 slave reads the shared BCLK/WS pads; jumper is only the fallback, which needs a new maintainer decision) |

## 2. Wiring table

DOUT2 is the GPIO chosen at GATE-DOUT2. GPIO15 is the proposed value in
`dout2-candidates.md`; the gate is still open, so treat it as a placeholder.

| ESP32 signal | GPIO | DAC0 pad | DAC1 pad | DAC2 pad | Notes |
|---|---:|---|---|---|---|
| I2S MCLK | 4 | MCK | MCK | MCK | 33 ohm series at the ESP32 end |
| I2S BCLK | 5 | BCK | BCK | BCK | 33 ohm series; also read back by I2S1 inside the chip, no wire |
| I2S WS | 6 | WSEL | WSEL | WSEL | 33 ohm series; same |
| I2S DOUT0 (lane 0) | 7 | DIN | DIN | not connected | 33 ohm series; DAC0 and DAC1 share this line |
| I2S DOUT2 (lane 1) | GATE-DOUT2 (proposed 15) | not connected | not connected | DIN | 33 ohm series; keep the wire about as long as the GPIO7 wire |
| I2C SDA | 8 | SDA | SDA | SDA | no external pull-up |
| I2C SCL | 9 | SCL | SCL | SCL | no external pull-up |
| DAC mute | 10 | MUTE | MUTE | MUTE | 470 ohm pull-down, as in hw-pinmap.md section 6 |
| 3V3 | | VIN | VIN | VIN | |
| GND | | GND | GND | GND | star ground; both GND pads |

Straps, CLK Dir jumper and addresses are unchanged from `hw-pinmap.md`
section 3 (MOD1 open, MOD2 to 3V3, A1/A2 as 0x4C / 0x4D / 0x4E, "CLK Dir Sel"
open on all three, so every board takes its clock from the ESP32 as secondary).

Note that DIN on DAC2 must not be tied to the GPIO7 line any more. Wiring DIN
of all three boards to GPIO7 and then enabling lane 1 would put two drivers on
one DAC2 input.

## 3. Slot plan

| Lane | Data pin | DAC | Slots | Reg 0x29 AOFS | Reg 0x28 |
|---|---|---|---|---:|---|
| 0 | GPIO7 | DAC0 | 0, 1 | 0 | AFMT 01 (TDM), ALEN 11 (32 bit) |
| 0 | GPIO7 | DAC1 | 2, 3 | 64 | same |
| 1 | DOUT2 | DAC2 | 0, 1 | 0 | same |
| 1 | DOUT2 | none | 2, 3 | | silent |

Output assignment (low/high/sub) is still the earlier placeholder and is not
decided here.

## 4. What to re-check on the real boards

1. Each board's DIN pad, MCK, BCK and WSEL: continuity to the intended ESP32
   header pin before power (the breakout photos show labels, not continuity).
2. DAC2 DIN has no connection to GPIO7.
3. CLK Dir jumper open on all three (photograph each).
4. Scope, two channels: BCK = 6.144 MHz, MCK = 18.432 MHz; DOUT2 against
   DOUT0 shows a lag that is a fraction of one BCK (period about 163 ns). Take
   setup/hold limits from the datasheet timing table, not from memory.
5. Clock status (Reg 0x5F, mask 0x05) reads valid on all three boards.
6. Record everything in `docs/bringup-log.md`; that file is the only source of
   HW-VERIFIED.

## 5. Docs that become stale if ADR 0015 is accepted

For the maintainer to update in a task PR; this note does not edit them:

- `hw-pinmap.md` section 3 (DIN row for DAC2, BCK/MCK values), section 4
  (frame, slot table, offsets), section 6 (MCLK/BCK rates are no longer
  12.288 MHz), section 7 step 3 (scope check) and the section 2 UNVERIFIED note
  about the pin matrix (the breakout reference section 10 and ADR 0015 TRM
  check now cover it).
- `firmware/main/board_config.h`: 4 slots, `XOVER_PIN_I2S_DOUT2`, per-lane
  offsets (the file already has the compile-time pin checks to extend).
- `adafruit-pcm5122-breakout.md`: section 9 and 10.1 list one data line only;
  section 7 and 10.3 talk about 256 BCK / 12.288 MHz. Its tables also contain
  literal `\n` characters at the Adafruit guide row (section 1) and in the
  "Pad functions" row (section 11), which break the table rendering.

## 6. Not decided here

DOUT2 GPIO number (GATE-DOUT2), acceptance of ADR 0015 (GATE-ADR), whether the
shared BCLK/WS pads work with I2S1 as slave (GATE-SPIKE), and the output
assignment per channel.
