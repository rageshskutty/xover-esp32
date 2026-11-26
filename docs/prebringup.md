# Pre-bring-up checklist: from purchase to the first tone test

Status: PROPOSED procedure (AGENTS.md: agents add procedures, never results). Companion to
`docs/hw-pinmap.md`, `docs/hw-verification-notes.md` and `docs/bom.md`. The board-specific pad/strap/datasheet crosswalk is in [docs/hardware/adafruit-pcm5122-breakout.md](hardware/adafruit-pcm5122-breakout.md).
Scope: everything the maintainer does after the parts arrive and before the first
six-tone test (gate G1). Amplifiers and speakers stay disconnected throughout (AGENTS S3).
Results go in `docs/bringup-log.md`, entered by the human who ran the measurement, using
that file's own template. This document does not define a second template.

Rules for the whole sequence:
- One phase at a time. Do not start a phase until every step of the previous one passes.
- A failed step is a stop. Log it, do not work around it.
- Nothing is HW-VERIFIED until it has a `bringup-log.md` entry with a measured value.

## Gate 0: maintainer approval of the pin map (before any bring-up firmware)

The pin map is HUMAN-ONLY (AGENTS section 2) and `docs/hw-pinmap.md` is still PROPOSED.
Bring-up firmware (pin-walk, I2C scan, T08 tone app) cannot be written under AGENTS S1
and the T08 entry in `docs/engineering/03_task_catalog.md` until the maintainer records
approval of sections 2 to 4 of the pin map. Approval for bring-up use is not hardware
verification; the bring-up steps below are what verify it.

## Phase 0: Receiving inspection (no power)

| ID | Step | Pass criterion |
|---|---|---|
| P0.1 | Count parts against `docs/bom.md` | All items present |
| P0.2 | Read the ESP32 module marking | Reads N16R8 |
| P0.3 | Compare header silkscreen with `hw-pinmap.md` section 1 | Left and right header labels match the list there |
| P0.4 | Photograph the back of all three DAC boards | "CLK Dir Sel" jumper open on all three; layouts identical |
| P0.5 | Check DAC boards for headers | Headers present or solder them (P3.1) |
| P0.6 | Resistance GND to 3V3 on the ESP32 board, unpowered | Not a short (more than 1 kohm, rising as capacitors charge) |

## Phase 1: Toolchain on the PC (no hardware needed)

| ID | Step | Pass criterion |
|---|---|---|
| P1.1 | Install ESP-IDF v5.5.6 offline installer (Windows). Skip 6.x | `idf.py --version` prints v5.5.6 |
| P1.2 | Install the CH343 USB-serial driver if no COM port appears | Board shows as a COM port when the UART USB-C is plugged in |
| P1.3 | Clone the repo, run `python tools/check.py` | Passes |
| P1.4 | `cd firmware`, `idf.py set-target esp32s3`, `idf.py build` | Build succeeds |

## Phase 2: ESP32 board alone (nothing else attached)

Needs Gate 0 for steps P2.4 and P2.5 (they use pin numbers).

| ID | Step | Pass criterion |
|---|---|---|
| P2.1 | Plug the UART USB-C. Run `python -m esptool --chip esp32s3 flash_id` | Reports 16 MB flash |
| P2.2 | Run `python -m esptool --chip esp32s3 chip_id` | Features list shows 8 MB embedded PSRAM |
| P2.3 | Fix the provisional config: set `firmware/partitions.csv` and `sdkconfig.defaults` for 16 MB flash and octal PSRAM (`CONFIG_SPIRAM_MODE_OCT`) | Boot log shows 8 MB PSRAM initialised, no errors |
| P2.4 | Flash a hello-world and a pin-walk app that drives GPIO 4, 5, 6, 7, 8, 9, 10, 21 one at a time (3 s each) | Multimeter or scope sees each pin toggle on the matching header pin and no other; pins 3 and 46 stay untouched |
| P2.5 | Read GPIO0 (BOOT key) after boot, press and release | Reads low when pressed, high when released |
| P2.6 | Measure 3V3 on the 3V3 header pin on USB power | 3.2 to 3.4 V |
| P2.7 | Measure supply current with the inline meter (if available) | Logged as the baseline |

## Phase 3: DAC boards on the bench (each board alone, powered from the ESP32 3V3 pin)

| ID | Step | Pass criterion |
|---|---|---|
| P3.1 | Solder headers where missing | Clean joints, no bridges |
| P3.2 | Measure strap pad levels with the board unpowered and powered, no ties yet | MOD1, MOD2, A1, A2 read low (pulled to ground), as `hw-pinmap.md` section 6 states |
| P3.3 | Measure the MUTE pad with it floating | Near 3.3 V (Adafruit schematic R1 G$4 provides a 10 kΩ pull-up; default unmuted) |
| P3.4 | Pull the MUTE pad to GND with a wire; later confirm silence with a low-level tone source | The source-resolved circuit is active-low: the pad connects directly to XSMT, so low must mute and high must unmute. Record the functional result. If the measurement disagrees, stop and inspect board revision/wiring; do not reverse the pull-down based on the erroneous schematic text annotation |
| P3.5 | Tie MOD2 to 3V3 on every board; tie A1 to 3V3 on board 1 and A2 to 3V3 on board 2 | Meter confirms levels from the table in section 3 of the pin map; board 0 has A1 and A2 open |
| P3.6 | Label the boards DAC 0, DAC 1, DAC 2 on the board itself | Labels on all three |

## Phase 4: Assembly and passive checks (power off)

| ID | Step | Pass criterion |
|---|---|---|
| P4.1 | Fit 33 ohm resistors at the ESP32 end of GPIO4, 5, 6, 7 | Four resistors, on the signal side before the bus |
| P4.2 | Fit one 470 ohm resistor from the shared GPIO10/MUTE net to GND (see `hw-pinmap.md` section 6.1) | One resistor; do not fit one per DAC |
| P4.3 | Fit the LED with 470 ohm in series on GPIO21 | LED polarity correct, other end to GND |
| P4.4 | Wire the bus: GPIO4 to MCK, 5 to BCK, 6 to WSEL, 7 to DIN, 8 to SDA, 9 to SCL, GPIO10 to the shared MUTE net on all three boards. Fit the single 470 ohm pull-down per `hw-pinmap.md` section 6.1. Check pins 3 and 46 are not used for I2C | Continuity from each ESP32 pin to the same pad on each DAC board; no series device or inverter in the MUTE path unless separately reviewed and documented |
| P4.5 | Wiring rules: clock and data wires 10 cm or shorter, a ground wire beside each clock line, star ground, both GND pads on the audio side | Done and photographed |
| P4.6 | Do not fit external I2C pull-ups. Do not enable ESP32 internal pull-ups | None fitted |
| P4.7 | Continuity: 3V3 to GND, and each signal to GND and to 3V3 | No shorts; each signal reads only the 470 ohm path where expected |
| P4.8 | Photograph the finished build, both sides | In the log |

## Phase 5: First power-up (UART USB-C only, DAC outputs unconnected)

| ID | Step | Pass criterion |
|---|---|---|
| P5.1 | Power up | Nothing hot, no smell; LED behaves as the pin-walk app dictates |
| P5.2 | Measure 3V3 at each DAC board's VIN | 3.2 to 3.4 V on all three |
| P5.3 | Measure SDA and SCL idle voltage | About 3.3 V (pull-ups from the DAC boards, about 3.3 kohm combined) |
| P5.4 | Measure shared MUTE with GPIO10 configured high-impedance | About 0.41 V predicted from three onboard 10 kΩ pull-ups in parallel against one 470 Ω pull-down; measure, do not assume. The active-low DACs should remain muted |
| P5.5 | Supply current with the meter | Logged; compare with the P2.7 baseline |
| P5.6 | I2C scan at 100 kHz | Addresses 0x4C, 0x4D, 0x4E and nothing else |
| P5.7 | Scope SDA and SCL (optional) | Clean edges at 100 kHz. Try 400 kHz only after this |
| P5.8 | Drive GPIO10 LOW, then HIGH while measuring the shared MUTE net and observing a low-level DAC tone (amps/speakers disconnected) | LOW mutes; HIGH unmutes. Record both pad voltage and functional result; GPIO10 HIGH under the 470 Ω load must satisfy ESP32-S3 output-level/current limits |

## Phase 6: Hand-off to the bring-up test

Entry criteria for the six-tone test (G1): Gate 0 and P0 to P5 all pass and are logged.
Then follow `hw-pinmap.md` section 7:

1. Scope BCK = 12.288 MHz, MCLK = 12.288 MHz, sync pulse one BCK wide.
2. Settle the two remaining UNVERIFIED TDM items (`hw-verification-notes.md` section 2): right sample position, and the one-BCK offset ambiguity.
3. With clocks running, read back the detected-clock registers (`hw-verification-notes.md` section 4): 48 kHz detected, SCK 256 x fS, BCK 256 per frame, no clock error.
4. Six distinct tones, one per slot, at no more than -30 dBFS (Kconfig `XOVER_BRINGUP_MAX_DBFS`, AGENTS S3), captured on the audio interface, amplifiers and speakers disconnected.

## Open items that block this checklist

- Gate 0: maintainer approval of the pin map for bring-up use.
- T08 stays human-blocked in `docs/engineering/03_task_catalog.md` until Gate 0 evidence is recorded; no coding card exists yet.
- Firmware needed after Gate 0: pin-walk app, I2C scan, then the T08 TDM tone app. None is built.
- ESP-IDF I2S TDM driver behaviour (short sync, slot masks) has not been read yet.
