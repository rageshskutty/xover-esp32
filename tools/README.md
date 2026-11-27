# VituixCAD export tooling

Planned: create a tested template/converter for exporting biquad coefficients into the graph format. Verify sample-rate metadata and coefficient sign convention against known test vectors before using exported filters in a speaker system.

- `coverage_gate.py`: G2 gate, fails if line coverage of `firmware/components/dspblocks`
  is below 90 %. Run after a `--coverage` host build and `ctest` (see the
  `host-coverage` job in `.github/workflows/ci.yml`). Stdlib only, needs `gcov`.
- `lookahead_screen.py`: HOST-SCREENING of limiter lookahead and attack against
  excursion overshoot (ADR 0014 section 3). Prints a table; not a gate. Needs
  numpy and scipy (`tools/requirements.txt`).
- `protection_oracle.py`: independent NumPy/SciPy reference for the parametric `filters[]` schema
  (ADR 0014 section 4). Reports the conservative per-output magnitude bound and the detector
  weighting response; no limits, no pass/fail. Vectors: `protection_oracle_vectors.json`; tests:
  `python tools/test_protection_oracle.py` (runs in CI). Needs numpy and scipy.
- `gen_mp3_vectors.py`: regenerates the MP3 golden vectors in `host/tests/golden/` (libmp3lame
  encode, ffmpeg decode). Needs ffmpeg with libmp3lame and numpy. See `host/tests/golden/README.md`.
- `biquad-bench/`: ESP-IDF project (results: `docs/bench/biquad-throughput.md`) that benchmarks biquad throughput on the ESP32-S3 (6- and
  2-channel). Host-side check: `gcc -std=gnu11 -Wall -Wextra tools/biquad-bench/host_test/test_host.c -lm`.
  Flashable binaries come from the `Firmware binaries` workflow (see below).
- Flashable binaries: the `Firmware binaries` workflow (`.github/workflows/firmware-binaries.yml`)
  builds `tools/biquad-bench` and `firmware/` for ESP32-S3 and uploads them as run artifacts.
  Flash the merged image: `esptool.py --chip esp32s3 write_flash 0x0 <name>-merged.bin`.
