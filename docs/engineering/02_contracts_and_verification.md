# Contracts and verification

## Authority and architecture

The product planning architecture is chain-per-output v1, not a graph/DAG runtime. ADR 0012 defines the ordered-chain architecture; ADR 0013 defines the protected block and mandatory output-protection pipeline; ADR 0014 defines the firmware protection contract and supersedes the protection decisions of ADRs 0008 (limit curves), 0010 (comparator) and 0013; ADR 0011 defines configuration mutation and response ownership.

These ADRs are proposals, not approved implementation authority. ADR 0011, ADR 0012, and ADR 0014 require human safety review and CODEOWNERS approval before implementation. Existing public DSP source headers remain authoritative for implemented modules. Public header changes require an ADR, docs/api.md update, and CODEOWNERS review.

## Implemented interfaces

- dspblocks: see `firmware/components/dspblocks/include/dspblocks.h`. Response APIs include `dsp_sos_eval(...)` and `dsp_log_grid(...)`; both are core-0-only.
- dspgraph: deleted (branch work/delete-dspgraph); it was legacy graph code, not the product execution architecture. Git history keeps it.
- chain-v1 JSON: schema in `configs/schema.json`; 48 kHz, at most 64 blocks per chain, 1–8 outputs, declared input channels and ordered output chains. This is a schema/guardrail contract only; no chain compiler or runtime exists.

## Protection contract status

ADR 0014 retires the production C comparator, the per-output limit curves, curve interpolation and the zero-transfer floor as firmware contract. The protection record is the parametric block in `configs/schema.json` (detector weighting filters, thresholds, hold, thermal stages, release, ceiling). Driver, amplifier and supply modelling stays in the designer tool; real protected values require human safety evidence and agents must not invent them. The conservative per-output sum `Σ |w_i| · |H_i(f)|`, with no phase-cancellation credit, survives only as a designer-side report produced by `tools/protection_oracle.py`. `auto_trim` is not part of the current JSON schema and is disabled/deferred for chain v1; no user gain may be silently changed.

No firmware-side static response acceptance check exists. The weighting group-delay check named in ADR 0014 section 3 is a planned validator task.

## Verification

CI workflow `.github/workflows/ci.yml` runs:
- `python tools/check.py`;
- CMake configure/build and CTest;
- `tools/test_scipy_compare.py`, which compares existing compiled `dsp_sos_eval` filter responses against SciPy on the 1054-point 10 Hz–20 kHz grid;
- ASan/UBSan host tests;
- the dspblocks coverage gate.

Important limitation: `tools/test_scipy_compare.py` validates existing DSP SOS response primitives. It is not a chain-v1 response oracle. The independent NumPy/SciPy oracle is `tools/protection_oracle.py`, with deterministic analytic vectors in `tools/protection_oracle_vectors.json` and tests in `tools/test_protection_oracle.py`. It designs the ten schema filter types (`lp1 hp1 lp2 hp2 peq lshelf hshelf ap1 ap2 notch`) with the float32 rounding of `dspblocks.c` and reports, per output, the conservative static magnitude bound of the signal path and the magnitude of the peak-detector weighting on a caller-supplied grid. It makes no pass/fail decision, reads no driver, amplifier or supply data, and does not prove loudspeaker safety. HOST-VERIFIED: its designs matched the compiled `dsp_design_*` coefficients to rounding for every type that has a C designer (all except `ap1`). That check was run by hand and is not in CI. Open schema/C mismatches are logged as Q2 and Q3 in `docs/questions.md`.

The following commands are the host verification sequence:
```sh
python tools/check.py
cmake -S host -B host/build -DCMAKE_BUILD_TYPE=Release
cmake --build host/build
ctest --test-dir host/build --output-on-failure
python tools/test_scipy_compare.py --library host/build/libdspblocks_eval.so
cmake -S host -B host/build-san -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON
cmake --build host/build-san
ctest --test-dir host/build-san --output-on-failure
```

CI pass is evidence only for the exact commit and jobs shown in GitHub Actions. Remote source inspection is not a local test run. Target/hardware results require human-authored bring-up evidence.

## Evidence labels

HOST-VERIFIED only for commands actually run successfully. CI-VERIFIED only for named jobs that passed on the exact commit. HW-VERIFIED only with owner-authored bring-up evidence. Otherwise UNVERIFIED.
