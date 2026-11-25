# GATE-DOUT2 candidate analysis (DAC2 data pin)

Status: PROPOSED. This note does not close GATE-DOUT2 and does not change
`firmware/main/board_config.h` or `docs/hw-pinmap.md`. The maintainer picks the
pin; nothing here is HW-VERIFIED.

## Constraint (from the gate)

The DAC2 data GPIO must not be a strapping, flash, PSRAM, USB or console pin,
and must differ from every pin already in `board_config.h`.

## Pins already used (board_config.h on main)

| GPIO | Signal |
|---:|---|
| 0 | admin button (BOOT) |
| 4 | I2S MCLK |
| 5 | I2S BCLK |
| 6 | I2S WS |
| 7 | I2S DOUT (DOUT1) |
| 8 | I2C SDA |
| 9 | I2C SCL |
| 10 | DAC mute |
| 21 | admin LED |

## Excluded

| GPIO | Reason | Source |
|---|---|---|
| 0, 3, 45, 46 | strapping | WROOM-1 datasheet, as in `board_config.h` |
| 19, 20 | native USB (D-/D+) | `board_config.h`; Espressif DevKitC-1 v1.1 schematic, "ESP USB" port |
| 26-37 | flash and octal PSRAM on the N16R8 | `board_config.h` |
| 43, 44 | UART0 console | `board_config.h`; DevKitC-1 v1.1 schematic (U0TXD/U0RXD to CP2102N) |
| 38, 48 | possible RGB LED data | Espressif DevKitC-1 v1.1 schematic: GPIO38 = `RGB_CTRL` (v1.0 used GPIO48). The actual bring-up board is a Hubtronics-style board with an unlabelled LED pin, so keep both out. |
| 39-42 | default JTAG | avoid unless JTAG is given up |

## Remaining candidates

GPIO 1, 2, 11, 12, 13, 14, 15, 16, 17, 18, 47.

Preferred: **GPIO15**. It sits on the left header directly after GPIO7 (the
header order is 4, 5, 6, 7, 15, 16, 17, 18, 8, ...), so the DOUT2 wire runs
beside the other I2S lines. GPIO16 is the next choice.

Caveats:
- The Espressif schematic is for the official DevKitC-1. The bring-up board
  differs (see `docs/hw-pinmap.md` section 1), so confirm on the real board that
  nothing is wired to the chosen pin.
- Routing I2S to any GPIO through the pin matrix is still UNVERIFIED (TRM check,
  `docs/hw-pinmap.md` section 2).
- GATE-SPIKE and ADR 0015 decide whether a second data lane is used at all.

## To close the gate

Maintainer states the GPIO number; then `board_config.h` (CODEOWNERS path) and
`docs/hw-pinmap.md` are updated in a task PR with the compile-time pin checks
extended to cover it.
