# Bill of materials (bring-up stage)

Status: PROPOSED. Derived from `docs/hw-pinmap.md` (branch `docs/hw-pinmap-board-config`).
Covers only what is needed to reach gate G1 (six tones on six outputs, amplifiers and
speakers disconnected). Amplifiers, speakers and protection hardware are in section C and
must not be wired until their own gate.

"Have?" is for the maintainer to fill in. Quantities include the spares worth buying.

## A. Electronics

| # | Item | Spec / what to check on arrival | Qty needed | Buy | Have? |
|---|---|---|---:|---:|---|
| A1 | ESP32-S3 dev board | Hubtronics-style, two USB-C (CH343P UART + native USB), module **ESP32-S3-WROOM-1-N16R8** (16 MB flash, 8 MB octal PSRAM). Read the module's metal-can marking; N8R2 or N16R2 variants break the PSRAM and flash assumptions | 1 | 2 | |
| A2 | Adafruit PCM5122 I2S DAC breakout (ADA6421) | Check whether header pins are supplied or loose; check the "CLK Dir Sel" back jumper is open | 3 | 3 | |
| A3 | Resistor 33 ohm, 1 %, 0.25 W | Series damping at ESP32 end of MCLK, BCLK, WS, DOUT | 4 | 10 | |
| A4 | Resistor 470 ohm, 1 %, 0.25 W | GPIO10 mute pull-down; admin LED series resistor | 2 | 10 | |
| A5 | LED, 3 or 5 mm, any colour | Admin LED on GPIO21 | 1 | 3 | |
| A6 | Perfboard (about 7 x 9 cm) | For a soldered bus. Breadboard is acceptable only for the I2C scan, not for 12.288 MHz clocks | 1 | 1 | |
| A7 | Female header strips, 2.54 mm | Socket the ESP32 board so it can be removed; also useful for the three DAC boards | 1 set | 1 set | |
| A8 | Male header strips, 2.54 mm | For DAC boards if not supplied | 3 boards | 1 strip | |
| A9 | Solid-core hook-up wire, 24 to 26 AWG | Signal wires 10 cm or shorter; one extra ground wire per clock line | 1 m | 2 m | |
| A10 | USB-C cable, data capable (not charge-only) | Flash and console on the UART port | 1 | 2 | |

Strap options for the DAC boards (MOD2, A1, A2 to 3V3): solder bridges or short wire links to the ESP32 3V3 pin, no extra parts.

## B. Test equipment

| # | Item | Why | Qty | Have? |
|---|---|---|---:|---|
| B1 | Multimeter | Strap levels, MUTE polarity, 3V3, pull-up voltage, continuity | 1 | |
| B2 | Oscilloscope, 100 MHz or better, 10x probe | BCK and MCLK are 12.288 MHz; a 20 or 24 MHz scope or a 24 MS/s logic analyser cannot show their edges | 1 | |
| B3 | Logic analyser, 8 channel (optional, I2C only) | Decode the PCM5122 register writes | 1 | |
| B4 | Audio capture input: USB audio interface or PC sound card with line-in | Records each DAC's two outputs (three captures for six channels). Tones are at or below -30 dBFS so level is safe | 1 | |
| B5 | Cables from DAC line out to capture input | Depends on the DAC board's output connector and the capture device. Confirm both ends on arrival | 3 | |
| B6 | Soldering iron, solder, flux, desoldering braid | Headers, straps, bus | 1 | |
| B7 | Inline USB power meter (optional) | Check supply current of ESP32 plus three DAC boards against the AMS1117 | 1 | |

## C. Deferred, not part of bring-up

- Power amplifiers for six outputs (models undecided, `hw-pinmap.md` section 8).
- Amplifier mute, DC blocking and fuse (ADR 0014 section 5). Required before any amplifier is connected.
- Speakers and sub drivers.
- Calibrated measurement microphone and stand (room measurement, later phase).
- Enclosure, strain relief, final power supply.
- A dedicated admin push button and status LED hardware, if the BOOT key is not wanted for the admin window.

## D. Order-time decisions that affect the BOM

1. Confirm the module marking before buying a second board; sellers mix N16R8 and N8R2 under one listing.
2. If the three DAC boards come from different batches, check that the strap and jumper layout is identical (photos of the back of each).
3. If the capture device has only two inputs, plan three capture passes (DAC 0, DAC 1, DAC 2).
