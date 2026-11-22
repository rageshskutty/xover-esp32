# Contracts and verification

## Authority and architecture

The product planning architecture is chain-per-output v1, not a graph/DAG runtime. ADR 0012 defines the ordered-chain architecture; ADR 0013 defines the protected block and mandatory output-protection pipeline; ADR 0008 defines the conservative per-output response bound and limit curves; ADR 0010 defines the pure comparator behavior; ADR 0011 defines configuration mutation and response ownership.

These ADRs are proposals, not approved implementation authority. ADR 0008, ADR 0010, ADR 0012, and ADR 0013 require human safety review and CODEOWNERS approval before implementation. Existing public DSP source headers remain authoritative for implemented modules. Public header changes require an ADR, docs/api.md update, and CODEOWNERS review.

## Implemented interfaces

- dspblocks: see `firmware/components/dspblocks/include/dspblocks.h`. Response APIs include `dsp_sos_eval(...)` and `dsp_log_grid(...)`; both are core-0-only.
- dspgraph: deleted (branch work/delete-dspgraph); it was legacy graph code, not the product execution architecture. Git history keeps it.
- chain-v1 JSON: schema in `configs/schema.json`; 48 kHz, at most 64 blocks per chain, 1–8 outputs, declared input channels and ordered output chains. This is a schema/guardrail contract only; no chain compiler or runtime exists.

## Protection contract status

The normative planning direction is the conservative per-output sum `Σ |w_i| · |H_i(f)|`, with no phase-cancellation credit. The pure comparator receives already validated response magnitudes, weights, frequencies, and per-output curve values. It does not parse JSON, design filters, access hardware, derive driver limits, or run runtime limiters.

The current comparator ADR is incomplete for implementation: exact C layouts/signatures, maximum dimensions and memory budget, zero-transfer dB floor, curve interpolation, and complete grid rules must be frozen. Driver/amplifier/supply-to-limit-curve derivation and real protected values require human safety evidence; agents must not invent them. `auto_trim` is not part of the current JSON schema and is disabled/deferred for chain v1; no user gain may be silently changed.

The current subsonic check in `tools/check.py` is only a temporary presence heuristic, not a response calculation. It must not be treated as evidence that attenuation at `min_freq_hz` is satisfied. Static protection acceptance remains blocked until a real response evaluator is implemented and verified.

## Verification

CI workflow `.github/workflows/ci.yml` runs:
- `python tools/check.py`;
- CMake configure/build and CTest;
- `tools/test_scipy_compare.py`, which compares existing compiled `dsp_sos_eval` filter responses against SciPy on the 1054-point 10 Hz–20 kHz grid;
- ASan/UBSan host tests;
- the dspblocks coverage gate.

Important limitation: `tools/test_scipy_compare.py` validates existing DSP SOS response primitives. It is not a chain-v1 static protection oracle and does not generate the protection comparator's golden vectors. The independent NumPy/SciPy reference oracle is implemented in `tools/protection_oracle.py`, with deterministic vectors in `tools/protection_oracle_vectors.json` and tests in `tools/test_protection_oracle.py`. It evaluates supported chain-v1 static blocks against supplied (synthetic or provisioned) curves; it does not derive real protected curves. The exact numeric conventions in that oracle remain proposals pending human/CODEOWNERS approval.

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
