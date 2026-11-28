# ADR 0018: QEMU smoke test in CI

- Status: Proposed (needs CODEOWNERS review: it adds a file under .github/)
- Date: 2026-11-26
- Task: none yet (requested by the maintainer; a task card id is needed before merge)
- Relates to: AGENTS X4 (new dependency), V1, V2, ADR 0015

## Context
CI builds the firmware (T28) but never executes it. A boot-time crash such as a
bad early GPIO call would only show on hardware.

## Decision
1. Add a CI job that boots the mode `none` build in Espressif QEMU for esp32s3
   and checks the serial log (docs/simulation.md).
2. QEMU comes from ESP-IDF's own tool manager (`qemu-xtensa`), so its version is
   pinned by the pinned `espressif/idf:v5.5.6` image. No QEMU code enters this repo.
3. The job simulates the flash size in `sdkconfig.defaults`. It does not enable
   PSRAM and does not change flash or PSRAM settings (human-only, AGENTS section 2).
4. A pass is reported as HOST-VERIFIED boot only. It is never evidence for
   S2, S3, TDM or DAC behaviour; those remain HW-VERIFIED items.

## Alternatives
- Wokwi CI: models more peripherals but is a hosted service that needs an API
  token and a vendor account (X2, X5 review); it has no PCM5122 either. Not chosen.

## Consequences
- One more CI job (about a few minutes; not measured).
- Runtime apt packages are installed in the job and not pinned.
- If QEMU for esp32s3 cannot boot the image, the job is red and must be fixed or removed;
  it must not be loosened to pass (V3).
