#!/usr/bin/env bash
# Boot a built firmware image in Espressif QEMU (esp32s3) and check the serial log.
# Usage: tools/qemu_smoke.sh BUILD_DIR [RUN_SECONDS] [EXPECT_TEXT]
# Needs qemu-system-xtensa and esptool.py on PATH (ESP-IDF export.sh, qemu-xtensa tool).
set -euo pipefail

build_dir=${1:?usage: qemu_smoke.sh BUILD_DIR [RUN_SECONDS] [EXPECT_TEXT]}
run_seconds=${2:-20}
expect_text=${3:-"bringup mode: NONE (idle)"}
fail_regex='Guru Meditation|abort\(\)|assert failed|Rebooting\.\.\.|Brownout'

cd "$build_dir"
# Use the flash size the build was configured for; never a value typed here.
flash_size=$(grep -o -- '--flash_size [0-9]*MB' flash_args | cut -d' ' -f2)
esptool.py --chip esp32s3 merge_bin --fill-flash-size "$flash_size" \
    -o qemu_flash_image.bin @flash_args

log=qemu_serial.log
# -no-reboot turns a boot loop into a clean exit. timeout (124) is the normal end.
set +e
timeout "$run_seconds" qemu-system-xtensa -M esp32s3 -nographic -no-reboot \
    -drive file=qemu_flash_image.bin,if=mtd,format=raw \
    -serial mon:stdio > "$log" 2>&1
rc=$?
set -e
cat "$log"

if [ "$rc" -ne 0 ] && [ "$rc" -ne 124 ]; then
    echo "qemu_smoke: QEMU exited with status $rc" >&2
    exit 1
fi
if grep -Eq "$fail_regex" "$log"; then
    echo "qemu_smoke: crash or reboot marker found in serial log" >&2
    exit 1
fi
if ! grep -qF -- "$expect_text" "$log"; then
    echo "qemu_smoke: expected text not found: $expect_text" >&2
    exit 1
fi
echo "qemu_smoke: PASS ($expect_text)"
