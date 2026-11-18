# Micro-task catalog

## Execution rule
This catalog begins at the verified main state T06b1, commit `cd2ae0d598fe7c414f4d128655083b0ffc5ec57d`. It supplements `docs/PLAN.md`; it does not reopen completed DSP math/config tasks. Execute only the earliest unblocked task in `task_state.json`. One task = one branch/PR, <=400 changed lines excluding generated/golden data. No unrelated files. If acceptance exposes a contract conflict, stop and report; do not implement an interpretation.

## Common required task fields
Every card provides objective, context, dependencies, exact files, implementation specification, API contract, constraints, acceptance criteria, tests, build verification, regression requirements, and completion report. Unless overridden, build commands are `python tools/check.py`; `cmake -S host -B host/build -DCMAKE_BUILD_TYPE=Release`; `cmake --build host/build`; `ctest --test-dir host/build --output-on-failure`. Report actual output or UNVERIFIED. Update progress state and related docs in the same PR.

## Cards

### T07a — Pure protection response evaluator
**Objective:** implement deterministic config-level protection evaluation independent of the future runtime graph.  
**Context:** design.md §6 defines output response and device records; T06b1 has JSON validation/node compile, no runtime graph.  
**Dependencies:** T06b1; protection schema review is required before final record fields are frozen.  
**Files:** `firmware/components/dspgraph/{dspgraph_protect.c,internal/dspgraph_protect.h,CMakeLists.txt}`, `host/tests/test_protect.c`, `host/CMakeLists.txt`, `docs/api.md`, one ADR.  
**Implementation:** parse validated config on core 0; walk bounded DAG/topological order and calculate complex response for each input→output including cascades/mix, on bounded 1054-point log grid. Compare to immutable supplied records. Missing/corrupt records fail closed; no actual driver thresholds are introduced.  
**API:** ADR-freeze pure interface e.g. `dspg_check_protection(const char *json,size_t len,const dspg_protect_record_t *records,size_t n,dspg_report_t *report)`; no `dspg_t` dependency.  
**Constraints:** core 0 only; no NVS writes, no RT code, preserve v1 config and current errors.  
**Acceptance:** every protected output is evaluated; invalid record and violated limits rejected as E_PROTECT (or agreed record error); limits cannot be widened by upload; calculation is bounded and deterministic.  
**Tests:** analytical safe/fail curves, band edges, peak gain, fanout/mix, missing/corrupt record and all invalid fixtures.  
**Build:** common host checks plus sanitizer build/CTest.  
**Regression:** G2/G3 and golden vectors unchanged.  
**Completion report:** changed paths, math/reference, test output, unresolved human-supplied values.

### T07b — Protection persistence and provisioning boundary
**Objective:** runtime can read protection state; only serial provisioning can write.  
**Context:** protection is safety authority, separate from user config.  
**Dependencies:** T07a, owner-reviewed record schema, D1/D2.  
**Files:** new `firmware/components/protection/*`, serial tool under `tools/`, `docs/{api.md,questions.md,bringup-log.md}`, firmware CMake.  
**Implementation:** version/CRC/range validation; absent or corrupt record → muted; read-only runtime API; no web/config mutation path.  
**API:** ADR freeze `protection_load` and `protection_validate`; provisioning transport offline serial-only.  
**Constraints:** S1-S7; no guessed hardware/limit values.  
**Acceptance:** absent/corrupt state cannot transition RUNNING; valid test record is owner-provided.  
**Tests:** host storage adapter cases missing/corrupt/valid/rejected; target read-only boot check.  
**Build:** common + pinned IDF when D2 is resolved.  
**Regression:** G3, all host tests.  
**Completion report:** record format, access boundary, evidence and human inputs.

### T08a — Board/build decision closure
**Objective:** make the target build reproducible without invented facts.  
**Context:** current tree has no board_config.h; defaults assume 8MB and leave PSRAM off.  
**Dependencies:** D1, D2, D4, D5, D6 from human.  
**Files:** `firmware/main/board_config.h`, `firmware/Kconfig.projbuild`, `.github/workflows/ci.yml`, `docs/{hw-pinmap.md,questions.md,bringup-log.md}`.  
**Implementation:** source every pin/address/board value from owner/cited datasheet; unset macros fail compile; pin IDF; add target CI build.  
**API:** named compile-time board config only.  
**Constraints:** no driver behavior; no main changes; preserve host CI.  
**Acceptance:** removing any required value fails clearly; target skeleton builds for declared module.  
**Tests:** negative compile test and target CI.  
**Build:** pinned `idf.py build`.  
**Regression:** host workflow unchanged and passing.  
**Completion report:** exact IDF/module citations and build output.

### T08b — TDM output and DAC-safe bring-up
**Objective:** implement DAC/I2S output after all board decisions.  
**Context:** tdmout/pcm5122 are stubs; 8×32/48k is design intent only.  
**Dependencies:** T08a,T07b.  
**Files:** `firmware/components/{pcm5122,tdmout}/*`, hardware tests/docs.  
**Implementation:** configure I2S TDM, six mapped outputs, hardware mute and safe startup based only on verified board record.  
**API:** ADR freezes `tdmout_init/start/write_block/stop/mute`, `pcm5122_init/set_hardware_mute`; see `05_proposed_module_interfaces.md`.  
**Constraints:** no guessed offsets/addresses/registers; <=-30dBFS; amps/speakers disconnected.  
**Acceptance:** three DACs detected, clocks within existing G1 tolerance, six distinct tones, crosstalk and ramp gates pass.  
**Tests:** host packing test and human hardware log.  
**Build:** target build then bench protocol.  
**Regression:** G1 and host.  
**Completion report:** board, slot mapping, captures/meters, safety cap.

### T09a — Runtime graph execution and bounded memory
**Objective:** execute validated DAG on host/target using bounded preallocated memory.  
**Context:** current public graph API validates only; validated-node compile exists.  
**Dependencies:** T06b1 and accepted opaque graph/arena API ADR.  
**Files:** dspgraph runtime source/internal headers/CMake, public header, host tests, docs/api and ADR.  
**Implementation:** topological execution plan, slots 0..7, two input channels, eight outputs, correct per-node/per-channel state; measure exact arena need before build; reject undersized arena before publication.  
**API:** follow `05_proposed_module_interfaces.md` only after ADR; `dspg_process` fixed max frames, no allocations.  
**Constraints:** no JSON/design/heap/logging/locks in RT; no public ABI drift without ADR.  
**Acceptance:** deterministic output routing and defined alias policy; no shared channel state; teardown only after quiescence.  
**Tests:** chain, fanout, mix, each node, memory exhaustion, channel independence and golden graph.  
**Build:** common host commands, sanitizers, coverage.  
**Regression:** G2/G3.  
**Completion report:** worst path sections, arena bytes and tests.

### T09b — Core-1 runtime, ring and swap lifecycle
**Objective:** integrate DSP worker, SPSC ring, mute state and safe graph generation swaps.  
**Context:** source and executor absent; 64-frame period versus execution-time measurement definition is unresolved.  
**Dependencies:** T09a,T07b,T08b and accepted timing ADR.  
**Files:** new runtime/task, audiosrc, CMake, tests and design/API docs.  
**Implementation:** fixed block pacing; SPSC only; generation handoff; retire graph only after task acknowledgment; underrun/overrun counters; non-finite output muted and fault-counted.  
**API:** freeze runtime lifecycle/ring APIs in ADR.  
**Constraints:** AGENTS R1-R6/S2/S7, no core-crossing lock, resolve 64-frame conflict first.  
**Acceptance:** p99.9 <=0.60ms/max<=1.00ms 1h Wi-Fi, zero underruns (current PLAN targets, after timing ADR).  
**Tests:** host ring/state stress; target timing instrumentation.  
**Build:** common + pinned target build.  
**Regression:** G4.  
**Completion report:** block length, timing distribution, section count, SRAM/stack and evidence.

### T10a — Audio-source compatibility spike
**Objective:** decide decoder/source integration before adding framework.  
**Context:** docs/audio-source proposes ESP32-audioI2S hook but revision, license, pairing, pacing and memory are unresolved.  
**Dependencies:** D2,D3,D8 and T09b.  
**Files:** docs/audio-source, small build spike, manifest and ADR.  
**Implementation:** pin revision; build exact callback; evaluate license, IDF/Arduino compatibility, task/core, pacing/backpressure, RAM/PSRAM, supported format and 48k output; reject with evidence or approve.  
**API:** retain bounded audiosrc PCM interface.  
**Constraints:** decoder never enters DSP task; no dependency without pin/license/ADR.  
**Acceptance:** reproducible compile and measured buffer/pacing; dependency decision recorded.  
**Tests:** hook adaptation and short stream.  
**Build:** clean pinned target build.  
**Regression:** host framework-free.  
**Completion report:** revision/license evidence, measured behavior and decision.

### T10b — Approved source and endurance
**Objective:** deliver approved source playback on core 0.  
**Context:** decoder should feed bounded ring; DSP remains paced by output DMA.  
**Dependencies:** T10a,T09b,T08b.  
**Files:** audiosrc component, main integration, tests/docs.  
**Implementation:** exact PCM conversion, ring backpressure/underrun policy, Wi-Fi/decode separation.  
**API:** approved source lifecycle plus fixed PCM frame contract.  
**Constraints:** pinned dependency only; no PSRAM assumption; preserve output mapping.  
**Acceptance:** one-hour 48k approved FLAC/HTTP, zero underruns, stable internal heap (PLAN G5 target).  
**Tests:** host conversion/ring boundaries, target endurance.  
**Build:** common + target.  
**Regression:** G5.  
**Completion report:** source, duration, counters, heap and formats.

### T11 — Authenticated config upload and atomic persistence
**Objective:** safely upload, validate, store and activate configuration.  
**Context:** xoweb stub, A/B config partitions exist but flash assumption unverified.  
**Dependencies:** T07b,T09a,T09b,T08a.  
**Files:** xoweb, config-store component, web UI, tests/docs.  
**Implementation:** NVS token constant-time compare; body<=32KiB/depth<=8; validate/build/protect inactive graph; inactive A/B slot write, CRC/readback, marker swap; E_BUSY; stable error codes.  
**API:** ADR freezes route/method/body/status; no protection writes.  
**Constraints:** no secrets logged; no audio-thread allocation; failed upload preserves previous state or stays safely muted.  
**Acceptance:** upload <=1s target; invalid fixture code exact; power-loss selects last verified config.  
**Tests:** host transaction adapter, auth, oversized/malformed and power-loss simulation.  
**Build:** common + target.  
**Regression:** G3/G5/S2/S4.  
**Completion report:** endpoint examples, timing, flash recovery evidence.

### T12 — Status and response endpoint
**Objective:** expose read-only device state and frequency response.  
**Context:** response evaluation already exists at DSP SOS level, not graph HTTP level.  
**Dependencies:** T11,T09a.  
**Files:** xoweb, docs/api, host endpoint tests.  
**Implementation:** expose state/generation/counters/fault/config digest; calculate response on core 0 using active config/graph.  
**API:** bounded query/response schema in ADR.  
**Constraints:** no DSP-thread work; no token/protection leak.  
**Acceptance:** agrees with host graph evaluation within 0.01dB; invalid/unauthorized requests rejected.  
**Tests:** endpoint schema, bound and integration response.  
**Build:** common + target.  
**Regression:** G5/security.  
**Completion report:** comparison output and endpoint fields.

### T13 — VituixCAD convention proof/export
**Objective:** resolve external coefficient/export conventions by golden evidence.  
**Context:** design flags denominator sign and Bessel normalization VERIFY.  
**Dependencies:** existing T03/T04; owner-provided app/version/reference vector.  
**Files:** converter tool/fixtures/docs/design/ADR.  
**Implementation:** prove transfer convention independently; Bessel stays E_UNSUPPORTED until owner decision.  
**API:** versioned offline file format; no runtime ABI changes.  
**Constraints:** do not change DSP math to match unverified convention.  
**Acceptance:** converter fixtures match declared tolerance and reject unsupported syntax.  
**Tests:** positive/negative round-trip vectors.  
**Build:** tool tests + common.  
**Regression:** preserve all existing golden behavior.  
**Completion report:** source/version, vectors, convention decision.

### T14 — User build, install and bring-up documentation
**Objective:** document safe reproducible operation after implementation evidence exists.  
**Context:** pinmap, bring-up and licensing docs currently contain unresolved entries.  
**Dependencies:** T08b,T10b,T11,T12,T13 and human bring-up log.  
**Files:** README and relevant docs.  
**Implementation:** wiring only from verified pinmap; build/upload/auth/recovery/rollback/safety/known limits.  
**API:** none.  
**Constraints:** every claim evidence-labeled; no guessed device instructions.  
**Acceptance:** reviewer follows safe muted boot and approved test procedure.  
**Tests:** links/path check, tools/check.py and human dry run.  
**Build:** common + target.  
**Regression:** AGENTS safety language consistent.  
**Completion report:** tested procedure and open questions.

### T15 — Release readiness/license gate
**Objective:** establish evidence-based release readiness; no architecture rewrite.  
**Context:** human owns license and release.  
**Dependencies:** T14 and all G0-G6 gates.  
**Files:** docs/licenses and release docs/workflow only if approved.  
**Implementation:** dependency/license inventory, exact version pins, gate evidence and known limitations.  
**API:** no API changes.  
**Constraints:** human makes license/merge/tag decision.  
**Acceptance:** every gate evidenced, accepted license, no blocker.  
**Tests:** full host CI and target build; hardware report by human.  
**Build:** clean reproducibility procedure.  
**Regression:** all API/safety/backward-compatibility checks.  
**Completion report:** evidence links, revisions, unresolved items; no automatic merge/tag.

## Dependency order
Parallel after baseline: T07a, T09a (after API ADR), T13 (after owner vector), T08a only after board decisions. T07b follows T07a; T08b follows T08a+T07b; T09b follows T09a+T07b+T08b+timing ADR; T10a→T10b; T11 after safety/runtime/board; T12 after T11/T09a; T14 after all operational modules; T15 after T14 and owner gates.
