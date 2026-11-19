# Contracts and verification

## Authority
Existing public source headers are authoritative for implemented DSP modules. ADR 0007 is the sole planned dspgraph build/process/lifetime contract; ADR 0008 and 0010 govern protection semantics/API; ADR 0009 defines timing. The former proposed graph signatures in this document are retired. Public header changes require an ADR, docs/api.md update, and CODEOWNERS review.

## Implemented interfaces
- dspblocks: see `firmware/components/dspblocks/include/dspblocks.h`. In particular response APIs are `dsp_sos_eval(const dsp_sos_t*,size_t,double,double,double*,double*)` and `dsp_log_grid(double,double,double*,size_t)`; both are core-0-only.
- dspgraph: current public API is `dspg_validate(const char*,size_t,dspg_report_t*)`, see `firmware/components/dspgraph/include/dspgraph.h`. The internal `dspg_compile_node` contract is in `firmware/components/dspgraph/internal/dspgraph_build.h`; it compiles one validated node and is not a runtime graph.
- JSON v1: schema in `configs/schema.json`; 48 kHz, at most 64 nodes, body <=32 KiB, depth <=8, IDs <=31 characters. Existing config error taxonomy is in `docs/api.md`.

## Planned graph contract (not implemented)
ADR 0007 defines the one build signature, caller-owned aligned arena, 1..64 frame planar processing, alias prohibition and acknowledged lifetime. Do not use any signatures from the retired `docs/engineering/05_proposed_module_interfaces.md`.

## Planned protection contract (not implemented)
ADR 0008 defines one record per output, zero-edge disabled bands and sample points. ADR 0010 defines the exact `dspg_protect_check` comparator signature. Graph-path complex response construction is not yet specified; T07a6 is blocked until a maintainer-approved response propagation ADR exists. Never publish a graph without all outputs passing protection.

## Verification
Host commands: `python tools/check.py`; `cmake -S host -B host/build -DCMAKE_BUILD_TYPE=Release`; `cmake --build host/build`; `ctest --test-dir host/build --output-on-failure`. CI also compares SciPy response, runs sanitizers and coverage. These commands were not run in this planning session: UNVERIFIED. Target/hardware results require human-authored bring-up evidence.

## Evidence labels
HOST-VERIFIED only for commands actually run; HW-VERIFIED only with owner-authored bringup-log entry; otherwise UNVERIFIED. Remote source inspection is not a test run.
