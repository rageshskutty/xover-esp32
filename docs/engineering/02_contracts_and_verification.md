# Contracts and verification

## Authority and architecture

The product configuration is the parametric filter-stack schema in
configs/schema.json. ADR 0012 defines chain-per-output execution; ADR 0014
defines the current protection boundary and limiter contract. ADRs 0008,
0010, and 0013 are superseded historical records. Existing public DSP
headers remain authoritative. Public header changes require an ADR,
docs/api.md update, tests, and CODEOWNERS review.

## Implemented interfaces and planned runtime

- dspblocks: filter design, SOS processing, simple blocks, numeric safety,
  and response evaluation are implemented primitives.
- dspgraph: retained legacy code and tests only; not the product architecture.
- JSON v1: schema and guardrail validation exist. JSON contains optional
  per-input pre[].filters[], per-output ordered filters[], and optional
  top-level protection records. Runtime expansion and dspchain execution
  tables remain planned, not implemented.

## Protection contract

Output filters and detector-only weighting filters use the same parametric
filter object. Output filters are expanded to biquad sections once during
configuration load. Weighting applies only to peak detection; RMS is
unweighted because it observes the crossover-band-limited output. Missing
or incomplete protection means unprovisioned and muted, not enabled.

The limiter is fixed at 48 kHz: 48 samples lookahead, attenuation-only gain,
configurable release, and a final digital sample clamp. The clamp does not
guarantee loudspeaker excursion or thermal safety. Excursion margin and
infrasonic design remain the speaker designer's responsibility. See ADR 0014.

## Verification

CI runs tools/check.py, host CMake/CTest, tools/test_scipy_compare.py,
ASan/UBSan host tests, and the dspblocks coverage gate. The SciPy test loads
positive JSON examples against configs/schema.json, expands each declared
filter stack using existing dspblocks design primitives, and compares
compiled C response evaluation with SciPy on a 1054-point 10 Hz–20 kHz grid.
It verifies only stacks present in checked-in examples. It is not a
protection policy evaluator, driver model, or evidence of physical safety.

Host sequence:
python tools/check.py
cmake -S host -B host/build -DCMAKE_BUILD_TYPE=Release
cmake --build host/build
ctest --test-dir host/build --output-on-failure
python tools/test_scipy_compare.py --library host/build/libdspblocks_eval.so
cmake -S host -B host/build-san -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON
cmake --build host/build-san
ctest --test-dir host/build-san --output-on-failure

CI evidence applies only to the exact commit and jobs shown in Actions.
Remote source inspection is not a local test run. Hardware results require
owner-authored bring-up evidence.

## Evidence labels

HOST-VERIFIED only for commands actually run successfully. CI-VERIFIED only
for named jobs that passed on the exact commit. HW-VERIFIED only with
owner-authored evidence. Otherwise UNVERIFIED.
