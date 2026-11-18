# Contracts and verification

This document restates current public contracts and adds **proposed** seams only where no API exists. Current signatures in source/header remain authoritative. Any public header change needs an ADR and same-commit update to `docs/api.md` (AGENTS C5).

## Existing APIs to preserve
### dspblocks — `firmware/components/dspblocks/include/dspblocks.h`
Current public types: `dsp_err_t` = DSP_OK, DSP_E_PARAM, DSP_E_RANGE, DSP_E_UNSTABLE, DSP_E_UNSUPPORTED, DSP_E_NOMEM; family/mode/shelf enums; `dsp_sos_t {float b0,b1,b2,a1,a2,da1,da2;}`; state `{float z1,z2;}`; cascade of up to 4 SOS; gain state; delay state; `DSP_MAX_DELAY_SAMPLES=960); response cap 96 sections, grid default 1054/max 100000.
Current APIs: `dsp_design_xo(family,mode,unsigned order,double fc_hz,double fs_hz,dsp_cascade_t*)`; `dsp_design_peq(f,q,gain_db,fs,out)`; `dsp_design_shelf(shape,f,gain_db,fs,out)`; `dsp_design_allpass(f,q,fs,out)`; `dsp_design_notch(f,q,fs,out)`; `dsp_check_stable(sos)`; RT `dsp_sos_process(sos,states,n_sections,in,out,n_frames)`; gain init/set/process; polarity process; delay init/set/process; mix process; check NaN/Inf, flush denormal, sanitize block; dB conversions; delay-us-to-samples; response `dsp_sos_eval`, `dsp_log_grid`.
Contract: design/evaluation core 0 only; RT functions no heap/blocking, float32 only, in-place allowed for SOS/simple path functions where documented. Buffer/state lifetime is caller-owned. Invalid pointer behavior is documented by implementation/tests; new callers must validate before RT invocation.

### dspgraph — `firmware/components/dspgraph/include/dspgraph.h`
Current API: `int dspg_validate(const char *json,size_t len,dspg_report_t *report)`; report stores up to 8 issues `{code[20],path[48],msg[96]}`; core 0 only; returns 0 valid / -1 rejected. It does not return a graph. Internal APIs are private.

### Config/API contracts
- JSON version 1, sample rate exactly 48000, max 64 nodes; 32 KiB and depth 8 bounded parser; ID max 31 chars; only `frequency_hz` accepts `@var`; node types and property schemas in `configs/schema.json`.
- DSP config errors in `docs/api.md` remain stable: E_JSON, E_SCHEMA, E_NODE_PARAM, E_GRAPH_REF, E_GRAPH_CYCLE, E_GRAPH_FANIN, E_GRAPH_DEAD, E_SLOT, E_VAR_UNDEFINED, E_LIMIT, E_PROTECT, E_RESOURCE, E_UNSUPPORTED, E_AUTH, E_BUSY.
- Current node compile seam: `dspg_compile_node(const cJSON *vars,const cJSON *node,bnode_t *out)` (internal header); B_MAX_IN=8, B_ID_LEN=32, B_FS_HZ=48000. Compiled node stores type, cascade, gain/mix values, delay and polarity. Not yet a runtime graph API.
- Build contract: host `cmake -S host -B host/build -DCMAKE_BUILD_TYPE=Release`; `cmake --build host/build`; `ctest --test-dir host/build --output-on-failure`. CI additionally compares compiled response with SciPy, runs ASan/UBSan, and enforces >=90% dspblocks line coverage. Guardrail command: `python tools/check.py`.

## Proposed additions (must be frozen in ADR before implementation)
Use opaque graph and explicit ownership to prevent models inventing layout. Candidate contract, subject to owner review and API ADR:
```c
typedef struct dspg dspg_t;
typedef struct { uint16_t input_frames; uint16_t output_slots; uint32_t generation; } dspg_info_t;
int dspg_build(const char *json, size_t len, const dspg_build_opts_t *opts,
               dspg_t **out, dspg_report_t *report); /* core 0; *out NULL on failure */
int dspg_process(dspg_t *g, const float *const input[2],
                 float *const output[8], size_t n_frames); /* DSP core; fixed bounded work */
void dspg_destroy(dspg_t *g); /* core 0 only, only after audio task quiescence */
```
This is a design placeholder, not current API: exact options, allocator injection/internal SRAM enforcement, error return, frame bounds, output slot mapping, state ownership, and retirement acknowledgment must be settled in ADR. Do not copy these declarations into source until accepted. Prefer caller-supplied preallocated arena to make exact memory bounds observable and keep allocation out of RT path. Published graph is immutable except its private DSP state, which is mutated only by DSP owner.

## Interface obligations for empty modules
Interfaces are absent/stubs today. Before implementation, ADR/API must define names/signatures, error enum, frame layout, ownership, thread rules, blocking and lifecycle:
- `audiosrc`: init/deinit, bounded SPSC write/read, capacity/availability, overflow/underflow counters; producer may block only where library callback contract safely permits, otherwise explicit backpressure policy.
- `tdmout`: init/start/write_block/stop/mute; 8 slots × 32-bit frames; write may block on DMA and is RT task's sole allowed blocking output call.
- `pcm5122`: bus init/device setup/hardware mute/status; all pins, addresses, register constants and protection facts cited before coding.
- `xoweb`: start/stop, auth, bounded config upload/status/response; control-plane only, no protection write API.
- top-level runtime: explicit BOOT_MUTED → PROTECTION_READY → CONFIG_READY → RUNNING; any fault transitions to MUTED_FAULT, recover only after validation and ramp.

## Verification-first plan
1. **Host DSP invariants:** preserve all CTest suites, golden JSON/header vectors, analytic Butterworth -3.01 dB and LR response, LR4/LR8 summing, SciPy response comparison, float32 behavior; no golden regeneration as a way to mask differences. Existing PLAN tolerance gates (design ±0.001 dB/0.01°, float32 ±0.01 dB above 20 Hz) are targets from repository, not newly measured.
2. **Graph/config:** valid example accepted; every `configs/invalid/*.json` rejected with its expected code; malformed/deep/oversized JSON, duplicate ids/slots, dangling refs, cycles, fan-in, dead ends, variable bounds, Bessel unsupported, unstable float32 biquad. Fuzz/mutation gate: 100,000 configs, no crash/hang, <=50 ms each per PLAN G3 (host measurement).
3. **Safety:** no protection record and corrupt record remain muted; config limits cannot widen protection; input/output clipping and NaN/Inf produce deterministic mute/fault; all start/stop/swap paths obey >=5 ms down / >=20 ms up and <=-60 dBFS step. Test on host where possible, hardware only by human.
4. **Target integration:** IDF build after exact version decision; verify pin/DAC/TDM mapping with amps disconnected and -30 dBFS cap. No HW-VERIFIED claim without owner log in docs/bringup-log.md.
5. **Real-time:** instrument cycles/block, p50/p99/p99.9/max, underruns and cycle count per SOS for worst permitted path with Wi-Fi active over 1 h; report internal SRAM minima, task stack high-water, DMA/ring bytes and clipping/fault counts. G4's 64-frame / 1 ms contradiction must be resolved first.
6. **Streaming/control:** one-hour approved source run, 0 underruns; 100 swaps no step >-60 dBFS; config upload round trip <=1s; internal heap stable within 1KB; response endpoint agrees host within 0.01dB (PLAN G5 targets). No claim until measured.

## Test evidence labels
Every result is HOST-VERIFIED only if command was actually run in a host environment, HW-VERIFIED only with human log reference, otherwise UNVERIFIED. This documentation task had no checkout/toolchain/hardware; therefore build/tests and all hardware/performance facts remain UNVERIFIED.
