# Hardware pin map and wiring

Status: PROPOSED from datasheets and board photos by an agent. Nothing here is
HW-VERIFIED. The maintainer approves it in review, and the first bring-up entry
in `docs/bringup-log.md` is the real check. The same values are in
`firmware/main/board_config.h`.

**Board reference:** See [Adafruit PCM5122 breakout hardware and integration reference](hardware/adafruit-pcm5122-breakout.md) for the breakout pad-to-IC signal crosswalk, control-mode straps, address selection, clock-direction jumper, datasheet register map, and supporting Adafruit/TI documents. This companion does not approve or change the proposed pin assignments below.

## 1. Parts

| Part | Fact | Source | Status |
|---|---|---|---|
| ESP32-S3-WROOM-1-N16R8 | 16 MB quad SPI flash, 8 MB octal SPI PSRAM, ambient -40 to 65 C | Espressif ESP32-S3-WROOM-1 datasheet v1.8, Table 1-1 | datasheet |
| GPIO35, 36, 37 | wired to the octal PSRAM, not available | same datasheet, Table 3-1 note b | datasheet |
| Strapping pins | GPIO0, 3, 45, 46 | same datasheet, section 4 | datasheet |
| Dev board | Hubtronics-style board, two USB-C (CH343P UART bridge and native USB), BOOT and RST keys, AMS1117 3.3 V regulator, WS2812 RGB LED | seller photos | photo |
| DAC | 3 x Adafruit PCM5122 I2S DAC, line out, 3.3 V logic | Adafruit guide, TI PCM5122 datasheet SLAS763C | datasheet |

Header labels on the dev board match the module pin table: left header
3V3, 3V3, RST, 4, 5, 6, 7, 15, 16, 17, 18, 8, 3, 46, 9, 10, 11, 12, 13, 14, 5Vin, GND;
right header GND, TX, RX, 1, 2, 42, 41, 40, 39, 38, 37, 36, 35, 0, 45, 48, 47, 21, 20, 19, GND, GND.
That is 36 GPIOs, as the datasheet says. The silkscreen also prints 35, 36 and 37,
but they cannot be used on this module. The RGB LED data pin is not labelled
(vendor documents give GPIO38 or GPIO48); the map below uses neither.

## 2. ESP32-S3 pins

| Signal | GPIO | Module pin | Why this pin |
|---|---:|---:|---|
| I2S MCLK | 4 | 4 | plain GPIO, next to the other I2S pins |
| I2S BCLK | 5 | 5 | same |
| I2S WS (frame sync) | 6 | 6 | same |
| I2S DOUT | 7 | 7 | same |
| I2C SDA | 8 | 12 | Arduino default for the S3 |
| I2C SCL | 9 | 17 | Arduino default for the S3 |
| DAC mute (low = muted) | 10 | 18 | plain GPIO, one line to all three DACs |
| Admin button | 0 (BOOT key) | 27 | strapping pin, read only after boot |
| Admin LED | 21 | 23 | plain GPIO; add a 470 ohm series resistor |

Avoided: 0 (except the button), 3, 45, 46 strapping; 19, 20 native USB;
35 to 37 PSRAM; 43, 44 console; 38 and 48 possible RGB LED; 39 to 42 JTAG.
GPIO3 and GPIO46 sit between 8 and 9 on the left header, so check the labels
when wiring I2C.

UNVERIFIED: the ESP32-S3 can route I2S signals to any GPIO through its pin
matrix, but this datasheet defers that to the Technical Reference Manual, which
has not been checked. Confirm in the TRM chapters "IO MUX and GPIO Matrix" and
"I2S Controller" before relying on it.

## 3. Wiring to each DAC board

All three DAC boards are wired in parallel to the same bus signals.

| ESP32 | DAC pad | Notes |
|---|---|---|
| GPIO4 | MCK | 33 ohm series resistor at the ESP32 end |
| GPIO5 | BCK | 33 ohm series resistor at the ESP32 end |
| GPIO6 | WSEL | 33 ohm series resistor at the ESP32 end |
| GPIO7 | DIN | 33 ohm series resistor at the ESP32 end |
| GPIO8 | SDA | no external pull-up (section 6) |
| GPIO9 | SCL | no external pull-up (section 6) |
| GPIO10 | MUTE | 470 ohm pull-down to GND (section 6) |
| 3V3 | VIN | feed all boards from the 3V3 pin |
| GND | GND | star ground; use both GND pads for the audio side |

Straps on each board (the guide says an unconnected strap pad is low):

| Board | MOD1 | MOD2 | A1 | A2 | 7-bit I2C address |
|---|---|---|---|---|---|
| DAC 0 | open | 3V3 | open | open | 0x4C |
| DAC 1 | open | 3V3 | 3V3 | open | 0x4D |
| DAC 2 | open | 3V3 | open | 3V3 | 0x4E |

MOD1 low and MOD2 high selects I2C control; TDM is available only in software
control. Tie A1, A2 and MOD2 to the ESP32 3V3 pin, not to VIN, so that the
levels stay at 3.3 V even if VIN is ever moved to 5 V. Leave FMT, AGN, ATT0,
FILT and DEEM unconnected. The "CLK Dir Sel" jumper on the back stays open, so
each board takes its clock from the ESP32. The line outputs need a load of
1 kohm or more.

## 4. TDM frame and slot plan

48 kHz, 8 slots of 32 bit = 256 BCK per frame, so BCK = MCLK = 12.288 MHz. Eight
slots are needed, not six: the PCM5122 allows TDM at 48 kHz only with
128 or 256 BCK per sample (TDM/DSP rows of the datasheet's audio-interface table), and 6 x 32 = 192 is not allowed.

| DAC | Address | Slots | Offset (Reg 0x29) | Output assignment (from the earlier placeholder, to be confirmed) |
|---|---|---|---:|---|
| DAC 0 | 0x4C | 0, 1 | 0 | left low / left high |
| DAC 1 | 0x4D | 2, 3 | 64 | right low / right high |
| DAC 2 | 0x4E | 4, 5 | 128 | sub channels |
| none | | 6, 7 | | silent |

PCM5122 registers, Page 0 (datasheet tables 83 and 84):
Reg 0x28 AFMT = 01 (TDM/DSP) and ALEN = 11 (32 bit), which is 0x13 if the
reserved bits are written as 0; Reg 0x29 AOFS = offset in BCK from the frame start.

ESP32 side (intent for T08, not built): I2S TDM master, PCM short frame sync
(the datasheet needs the sync to be at least 1 BCK wide, rising edge = frame
start), 32 bit slots, `total_slot` = 8, slot mask 0x3F, MCLK = 256 x fS.

UNVERIFIED, to be settled at bring-up:
- whether each DAC's right sample follows its left sample immediately (the
  datasheet figures 16 to 18 were not readable in the text version);
- whether the offset counts from the sync pulse or one BCK later;
- the PLL and clock registers when MCLK is supplied (T08).

## 5. Power

The dev board's AMS1117 feeds the ESP32 and, through the 3V3 pin, the DAC boards.
Each DAC board has its own 3.3 V regulator. Check the supply with the real
load before the first long run.

## 6. Resistors and termination

Read from the Adafruit Eagle schematic/netlist in `adafruit/Adafruit-PCM5122-I2S-DAC-PCB` (not only the low-resolution schematic image). Electrical behaviour is source-resolved below; measure the assembled system during bring-up.

| Item | Finding | What to do |
|---|---|---|
| I2C pull-ups | each DAC board pulls SDA and SCL up to its 3.3 V (about 10 kohm; three boards in parallel are about 3.3 kohm) | add none; do not enable the ESP32 internal pull-ups; start at 100 kHz, try 400 kHz after a scope check |
| I2S lines | 12.288 MHz clocks over jumper wires to three loads | 33 ohm series resistor at the ESP32 end of MCLK, BCLK, WS and DOUT; keep wires short, with a ground wire beside each clock; tune on a scope |
| MUTE | Each board has a 10 kΩ pull-up from 3.3 V to MUTE (Adafruit schematic R1, gate G$4); MUTE connects directly to XSMT pin 25 with no inverter. XSMT is active-low: low = mute, high = unmute. | Keep the proposed 470 Ω pull-down from GPIO10 to GND for muted-at-boot behaviour; with three 10 kΩ pull-ups in parallel (about 3.33 kΩ), the shared line is about 0.41 V. GPIO10 high must override this pull-down to unmute; verify the actual high level/current on the assembled system before releasing the safety gate. |
| MUTE note | The schematic's text annotation “LOW = Mute Off / HIGH = Mute On” is incorrect. The TI datasheet, Adafruit guide and schematic netlist agree: MUTE is directly connected to active-low XSMT, with an onboard 10 kΩ pull-up. | Do not reverse the project's polarity or swap the 470 Ω pull-down. Verify the assembled idle voltage and that GPIO10 low mutes / high unmutes; if the measurement disagrees, stop and inspect the board revision/wiring rather than assuming an inverter. |
| Strap pins | MOD1, MOD2, A1 and A2 are pulled to ground on the board | tie to 3V3 only where section 3 says |
| Admin LED | plain LED on GPIO21 | 470 ohm series resistor to the LED, then GND |

The DAC mute is not the amplifier mute. The hardware requirements of ADR 0014
section 5 (amplifier mute, DC blocking, fuse) still apply.

### 6.1 End-to-end ESP32 GPIO10 to PCM5122 mute interface

This is the proposed project-level wiring, combining the Adafruit breakout
circuit with the external shared-line component shown in this pin map. It is
not a measured hardware record.

**Connection topology**

```text
ESP32-S3 GPIO10 (XOVER_PIN_DAC_MUTE_N)
                 |
                 +-------------------- shared MUTE net ----------------------+
                 |                         |                 |                |
              470 ohm                   DAC 0 MUTE         DAC 1 MUTE      DAC 2 MUTE
                 |                         |                 |                |
                GND                  XSMT pin 25        XSMT pin 25       XSMT pin 25
                                           |                 |                |
                                        10 kΩ             10 kΩ            10 kΩ
                                           |                 |                |
                                          3V3               3V3              3V3
```

- The Adafruit PCB schematic/netlist proves, per board, that JP3 MUTE pad 7
  connects directly to PCM5122 U1 XSMT pin 25 (no inverter or series resistor
  shown), and R1 resistor-pack gate G$4 is a 10 kΩ pull-up from 3.3 V to MUTE.
- The project pin map proposes one shared MUTE line from ESP32 GPIO10 to all
  three MUTE pads, with one external 470 Ω resistor from that shared line to
  GND. The 470 Ω component is an external project component, not populated on
  the Adafruit breakout. No GPIO-side buffer, transistor or level shifter is
  specified in this proposed topology.
- The three 10 kΩ pull-ups are in parallel: effective pull-up is about
  3.33 kΩ. With GPIO10 high-impedance and the 470 Ω pull-down fitted, the
  divider predicts approximately 0.41 V on the shared line:
  3.3 V × 470 / (3333 + 470) ≈ 0.408 V.
- With GPIO10 actively driven high, the GPIO must source approximately
  3.3 V / 470 Ω ≈ 7.0 mA through the external pull-down (plus any other
  current). This is a first-order estimate, not a measured current or a
  GPIO drive-strength approval. Verify the actual GPIO10 high voltage/current
  against the ESP32-S3 and development-board limits before relying on it.

**End-to-end logic (expected; bench confirmation still required)**

| GPIO10 state | Shared MUTE / XSMT voltage | PCM5122 state |
|---|---|---|
| LOW | LOW | Muted |
| HIGH | HIGH (must be verified under the 470 Ω load) | Unmuted |
| High impedance during reset/boot | Approximately 0.41 V from the proposed resistor network | Muted, provided the pull-down is fitted and the network is wired as shown |

Firmware pin name `XOVER_PIN_DAC_MUTE_N` refers to the active-low mute
signal: asserting it LOW mutes; deasserting it HIGH unmutes. The boot-time
high-impedance state is not a substitute for firmware's fail-closed mute
initialization. Keep amplifier/speaker outputs disconnected during first-tone
and mute tests.

**Verification required before marking HW-VERIFIED:** confirm continuity from
GPIO10 to all three MUTE pads, confirm exactly one 470 Ω pull-down to GND,
measure the shared-line voltage with GPIO10 high-impedance, then test GPIO10
LOW and HIGH while observing both the MUTE pad voltage and DAC output mute
behaviour. Record instrument, setup, values and result in
`docs/bringup-log.md`. If the measured circuit differs from this topology,
stop and update the human-owned pin map before changing firmware.

## 7. Checks for the first bring-up (human)

1. Multimeter: assembled MUTE idle voltage and GPIO10 low/high levels, SDA/SCL pull-up voltage, strap pad levels, 3V3 at each VIN.
2. I2C scan finds 0x4C, 0x4D and 0x4E and nothing else.
3. Scope: BCK = 12.288 MHz, MCLK = 12.288 MHz, sync pulse one BCK wide.
4. Six distinct tones, one per slot, at no more than -30 dBFS with amplifiers and speakers disconnected (gate G1).
5. Record results in `docs/bringup-log.md`.

## 8. Not decided here

ESP-IDF version (the maintainer is considering v5.5.x), the real speaker and amplifier models,
`sdkconfig.defaults` flash and PSRAM settings (they follow the module purchase),
and the admin LED and button hardware details beyond the pins above.
