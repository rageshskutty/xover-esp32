# 7B execution catalog (planning iteration 2)

**Authority:** ADRs 0007–0011 and docs/api.md. Catalog owns task dependencies and status; each linked prompt is the complete implementation work order. If a card disagrees with an ADR/header, stop and report; never edit the interface. `awaiting_approval` means the prompt and test are ready, but every listed human decision in task_state.json must be recorded as approved before work starts. `queued` means dependencies are complete and no human decision is open. `blocked` means a dependency or executable bundle is missing.
**Baseline:** branch `architecture/7b-plan-overhaul-v2` at `f13984ed6577f28804298ab74203755d3ecca255`; T06b1 code is present. No runtime graph execution or protection evaluator exists.
**Card sizing:** one card changes one behavior in one module. Target 30–80 handwritten production LOC; tests/golden data and generated files excluded. If estimation exceeds 80 production LOC, split before coding. The planning model commits every test before implementation; the 7B only registers that test and writes production code to make it pass. All task tests stay unregistered until their card begins. The sole exception is test-only T07a4, whose baseline is expected to pass.

## Required completion evidence (every card)
List exact files changed, production LOC count, test names, command lines and exit codes, expected baseline evidence, final test output, diff summary, and deviations. Run `python tools/check.py`, host build and CTest. Run sanitizer host suite when the card touches parsing/graph math. Hardware results are UNVERIFIED unless an owner records them in bringup-log.md. No golden regeneration to make a failure pass.

## T07a0 — Public protection record type
**Bundle/test:** [T07a0 prompt](prompts/T07a0.md); committed `host/tests/test_protect_layout.c`.
**Dependency:** ADR 0011. Adds the public type to dspgraph.h and matches docs/api.md §2.2; no logic. CODEOWNERS approval required.

## T07a1 — Protection record validator
**Bundle/test:** [T07a1 prompt](prompts/T07a1.md); committed `host/tests/test_protect_record.c`.
**Dependency:** T07a0. Implements shared field validation and record/output set validation. Comparator and grid cards reuse dspg_protect_fields_valid.

## T07a2 — Protection curve comparator
**Bundle/test:** [T07a2 prompt](prompts/T07a2.md); committed `host/tests/test_protect.c`.
**Dependency:** T07a1. Validates complete inputs before comparison and delegates record checks to dspg_protect_record_valid. Production comparator <=80 LOC.

## T07a3 — Protection frequency sample list
**Bundle/test:** [T07a3 prompt](prompts/T07a3.md); committed `host/tests/test_protect_grid.c`.
**Dependency:** T07a1. Uses 1054-point base grid plus exact endpoints/edges; maximum 1070. Capacity fixture uses four distinct interior edges (100, 500, 2000, 8000), yielding 1058 unique values; capacity 1057 rejects unchanged. Production 30–80 LOC.

## T07a4 — SOS response vectors (test-only)
**Bundle/test:** [T07a4 prompt](prompts/T07a4.md); committed `host/tests/test_sos_response.c`.
**Dependency:** T07a3. Tests existing dsp_sos_eval; baseline should pass after registration. No production LOC.

## T07a5 — Compiled-node response evaluator
**Bundle/test:** [T07a5 prompt](prompts/T07a5.md); committed `host/tests/test_node_response.c`.
**Dependencies:** T07a4, ADR 0011, and CODEOWNERS/maintainer approval of the internal seam before implementation. Single-node response only. Reuses the rbj_eq.json golden (-6.000003627305285 dB); denominator convention is `1 + a1*z^-1 + a2*z^-2` with stored signs unchanged. Test invalid pointers, frequencies, sample rates, empty cascades, non-finite results, and unchanged output sentinels; never regenerate the golden.

## T07a6 — Protection comparator integration
**State:** blocked; no executable bundle yet. Depends on T07a1–a5 and ADR 0011 graph response semantics and signature. Do not start until graph propagation tests and CODEOWNERS review are complete.

## T16 — SPSC audio frame ring
**Bundle/test:** [T16 prompt](prompts/T16.md); committed `host/tests/test_spsc_ring.c`. Independent host card, 30–80 production LOC. Caller-owned storage, C11 acquire/release, immediate partial read/write; overflow-safe modulo-2*capacity count; threaded producer/consumer stress under TSan with `Threads::Threads` linkage and a reproducible sanitizer command. Caller owns blocking/backpressure.

## T17 — Pure TDM slot packer
**Bundle/test:** [T17 prompt](prompts/T17.md); committed `host/tests/test_tdm_pack.c`. Independent host card, 30–80 production LOC. Six planar channels map to slots 0–5 in design.md order; slots 6–7 are zero. Retain the invalid-slot rejection regression in `test_graph_validate` (`duplicate_slot.json`, E_SLOT).

## T18 — S7 ramp duration helper
**Bundle/test:** [T18 prompt](prompts/T18.md); committed `host/tests/test_s7_ramp.c`. Independent host card, 30–80 production LOC. At 48 kHz, directional minima are 240/960 samples; float32 per-sample gain delta is <=`0.001f`, with adjacent-float boundary vectors around 250 frames.

## T19 — Fail-closed application state machine
**Bundle/test:** [T19 prompt](prompts/T19.md); committed `host/tests/test_app_state_machine.c`. Independent host card, 30–80 production LOC. No transition can enter RUNNING without valid protection and config; tests cover FAULT from SWAP_WAIT/RAMP_UP and pending FAULT surviving invalidation during RAMP_DOWN. `FAULT_MUTED` is latched until reset/reinitialization; no runtime event clears it. Await CODEOWNERS and human safety review before execution; terminal fault behavior is safety-owned.

## T20 — Config CRC and A/B slot selection
**Bundle/test:** [T20 prompt](prompts/T20.md); committed `host/tests/test_config_slot.c`. Independent host card, 30–80 production LOC. CRC-32/ISO-HDLC plus pure header validation and active/fallback selection; no flash or NVS access. Application transaction stages and verifies inactive data before owner swap acknowledgment, then flips the active selector; storage-fault behavior needs human safety review before integration.

## T21 — Request bounds
**Bundle/test:** [T21 prompt](prompts/T21.md); committed `host/tests/test_request_bounds.c`. Independent host card, 30–80 production LOC. Enforces body 1..32768 bytes, depth 1..8, nodes <=64, strings <=32. Do not modify test_graph_validate; the full host suite must stay green. Await CODEOWNERS review before execution.

## T22 — Token authentication comparison
**Bundle/test:** [T22 prompt](prompts/T22.md); committed `host/tests/test_token_auth.c`. Independent host card, 30–80 production LOC. Fixed 32-byte content loop, no early exit, token padding bytes ignored, lengths 0/1/32/>32 covered. Do not modify test_graph_validate; the full host suite must stay green. Await CODEOWNERS review before execution.

## T07b — Protection storage and provisioning
**State:** human-blocked. Requires owner-approved record values/ranges, NVS layout, serial provisioning transport and CODEOWNERS approval. No task card until these facts are provided. Runtime must remain muted with missing/corrupt records.

## T08 — Board and DAC integration
**State:** human-blocked. Board model, module memory, IDF version, pin map, PCM5122 addressing/straps/registers, slot mapping and safety limits are not in the repository as verified facts. No 7B coding card until human evidence is recorded.

## T09 — Graph runtime
**State:** API ADR exists but is not CODEOWNERS-approved and exact response path semantics/arena golden sizes are still pending. First authorized work is a big-model-owned host contract-test/golden-vector commit. Do not start runtime implementation cards until that commit is reviewed.

## T10–T15 — Source, control plane, bring-up and release
**State:** dependent human/architecture gates remain. Existing broad cards are retired; they are not executable work orders. Reissue them as micro-cards only after T07b/T08/T09 decisions and explicit per-card signatures, test inputs and expected outputs are committed. No 7B may decide external dependencies, hardware behavior, protection policy, licenses or release status.

## Explicit 7B execution criteria
Before beginning, the task bundle must give: one objective; exact paths; immutable signatures and relevant header text; exact accepted/rejected inputs; expected output values/tolerance; commands; expected baseline failure; 5 or fewer applicable rules; stop conditions. Card passes review only if:
1. No design/API/architecture choice is deferred to implementer.
2. Production scope is 30–80 LOC or has been split; tests excluded from this count.
3. Every numerical assertion has a value and tolerance.
4. Tests precede implementation and protect existing golden vectors.
5. Host logic has no ESP-IDF/hardware dependency.
6. Human/hardware gates are clearly blocked, never guessed.
7. Retry protocol is bounded and output is diff-only.

