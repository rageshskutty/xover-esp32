# Pre-bring-up checklist: from purchase to the first tone test

Status: PROPOSED. Companion to `docs/hw-pinmap.md` and `docs/bom.md`.
Scope: everything the maintainer does after the parts arrive and before the first
six-tone test (gate G1). Amplifiers and speakers stay disconnected throughout.
Record every result in `docs/bringup-log.md` (template at the end).

Rules for the whole sequence:
- One phase at a time. Do not start a phase until every step of the previous one passes.
- A failed step is a stop. Log it, do not work around it.
- Nothing is HW-VERIFIED until it has a log entry with a measured value.

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
| P3.3 | Measure the MUTE pad with it floating | Near 3.3 V (default unmuted) |
| P3.4 | Pull the MUTE pad to GND with a wire and listen or scope the line output with a tone source later | Record which level mutes. If it is the opposite of the guide, change the pull-down to a pull-up (section 6 of the pin map) and log it |
| P3.5 | Tie MOD2 to 3V3 on every board; tie A1 to 3V3 on board 1 and A2 to 3V3 on board 2 | Meter confirms levels from the table in section 3 of the pin map; board 0 has A1 and A2 open |
| P3.6 | Label the boards DAC 0, DAC 1, DAC 2 on the board itself | Labels on all three |

## Phase 4: Assembly and passive checks (power off)

| ID | Step | Pass criterion |
|---|---|---|
| P4.1 | Fit 33 ohm resistors at the ESP32 end of GPIO4, 5, 6, 7 | Four resistors, on the signal side before the bus |
| P4.2 | Fit 470 ohm from GPIO10 to GND | One resistor |
| P4.3 | Fit the LED with 470 ohm in series on GPIO21 | LED polarity correct, other end to GND |
| P4.4 | Wire the bus: GPIO4 to MCK, 5 to BCK, 6 to WSEL, 7 to DIN, 8 to SDA, 9 to SCL, 10 to MUTE, on all three boards in parallel. Check pins 3 and 46 are not used for I2C | Continuity from each ESP32 pin to the same pad on each DAC board |
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
| P5.4 | Measure MUTE with GPIO10 as input | About 0.4 V (three board pull-ups against 470 ohm), so the DACs are muted at boot |
| P5.5 | Supply current with the meter | Logged; compare with the P2.7 baseline |
| P5.6 | I2C scan at 100 kHz | Addresses 0x4C, 0x4D, 0x4E and nothing else |
| P5.7 | Scope SDA and SCL (optional) | Clean edges at 100 kHz. Try 400 kHz only after this |

## Phase 6: Hand-off to the bring-up test

Entry criteria for the six-tone test (G1): P0 to P5 all pass and are logged.
Then follow `hw-pinmap.md` section 7:

1. Scope BCK = 12.288 MHz, MCLK = 12.288 MHz, sync pulse one BCK wide.
2. Resolve the three UNVERIFIED TDM items in section 4 of the pin map.
3. Six distinct tones, one per slot, at no more than -30 dBFS, captured on the audio interface, amplifiers and speakers disconnected.

## Open items that block this checklist

- Technical Reference Manual check that I2S signals can be routed to GPIO 4, 5, 6, 7 (pin map section 2).
- The three UNVERIFIED TDM items (pin map section 4).
- `docs/bringup-log.md` does not exist yet; create it from the template below.
- Firmware needed: pin-walk app, I2C scan, then the T08 TDM tone app. None is built yet.

## `docs/bringup-log.md` template

```
## <date> <phase-id> <step-id>
- Setup: <board, wiring state, firmware commit>
- Measured: <value and unit, instrument>
- Expected: <from this document>
- Result: PASS / FAIL
- Notes / photos: <links>
```
