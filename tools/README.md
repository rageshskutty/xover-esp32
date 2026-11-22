# VituixCAD export tooling

Planned: create a tested template/converter for exporting biquad coefficients into the graph format. Verify sample-rate metadata and coefficient sign convention against known test vectors before using exported filters in a speaker system.

- `coverage_gate.py`: G2 gate, fails if line coverage of `firmware/components/dspblocks`
  is below 90 %. Run after a `--coverage` host build and `ctest` (see the
  `host-coverage` job in `.github/workflows/ci.yml`). Stdlib only, needs `gcov`.
- `lookahead_screen.py`: HOST-SCREENING of limiter lookahead and attack against
  excursion overshoot (ADR 0014 section 3). Prints a table; not a gate. Needs
  numpy and scipy (`tools/requirements.txt`).
