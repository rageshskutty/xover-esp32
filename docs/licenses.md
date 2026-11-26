# Third-Party Licenses

## cJSON
- URL: https://github.com/DaveGamble/cJSON
- Version: 1.7.18 (pinned commit acc76239bee01d8e9c858ae2cab296704e52d916)
- License: MIT License
- Copyright (c) 2009-2017 Dave Gamble and cJSON contributors
- Purpose: Configuration JSON parsing on host and target (T06a2)
- Notes: Parsed on core 0; no cJSON calls are used by real-time code.

## Espressif QEMU (CI tool only, not vendored)
- URL: https://github.com/espressif/qemu
- Version: the `qemu-xtensa` entry in ESP-IDF v5.5.6 `tools/tools.json` (installed in CI, not in this repo)
- License: GPL-2.0-or-later (QEMU). UNVERIFIED for the exact release; confirm before ADR 0018 is accepted.
- Purpose: boot smoke test of the firmware image in CI (ADR 0018)
- Notes: runs as a separate process in CI. No QEMU source or binary is committed.
