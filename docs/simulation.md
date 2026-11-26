# Virtual hardware (QEMU smoke test)

Status: UNVERIFIED. The workflow and script were written without a toolchain,
Docker daemon or network, so neither has been run. The first CI run is the first test.

## What it does
`.github/workflows/qemu-smoke.yml` builds the firmware (bring-up mode `none`,
ESP-IDF v5.5.6, `-Werror`), merges the images into one flash image with the flash
size the build was configured for, boots it in Espressif QEMU (`-M esp32s3`) and
checks the serial log (`tools/qemu_smoke.sh`):

- passes when `bringup mode: NONE (idle)` appears and the run ends by timeout;
- fails on `Guru Meditation`, `abort()`, `assert failed`, `Brownout` or a reboot.

## What it does not cover
QEMU models the CPU, flash and basic peripherals. It has no PCM5122, no I2C
devices, no I2S/TDM output and no real pin levels. It cannot show that the DAC
mute pin, the TDM lanes or the admin button work, so it proves nothing for
S2, S3 or ADR 0015. Those stay HW-VERIFIED items in docs/bringup-log.md.
The `pinwalk` and `i2cscan` modes are not run: `i2cscan` would report FAIL
without a DAC model.

## Flash and PSRAM
The simulated flash is whatever `firmware/sdkconfig.defaults` selects (8 MB today).
PSRAM is off in that file and therefore off in QEMU. board_config.h names an
N16R8 module (16 MB flash, 8 MB octal PSRAM); see Q12 in docs/questions.md.
Changing flash or PSRAM size is human-only (AGENTS section 2), so this PR leaves it.

## Run locally
    docker run --rm -it -v "${PWD}:/w" -w /w espressif/idf:v5.5.6 bash
    python $IDF_PATH/tools/idf_tools.py install qemu-xtensa
    . $IDF_PATH/export.sh
    cd firmware && idf.py -B build/none -DIDF_TARGET=esp32s3 build && cd ..
    tools/qemu_smoke.sh firmware/build/none 20
