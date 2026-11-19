# 7B execution catalog (planning iteration 2)

**Authority:** ADRs 0007–0011 and docs/api.md. Catalog owns task dependencies and status; each linked prompt is the complete implementation work order. If a card disagrees with an ADR/header, stop and report; never edit the interface.
**Baseline:** branch parent `architecture/7b-executable-engineering-plan` at `d8ce744e06edf0c05a50734d7e2ec13c764cebf2`; T06b1 code is present. No runtime graph execution or protection evaluator exists.
**Card sizing:** one card changes one behavior in one module. Target 30–80 handwritten production LOC; tests/golden data and generated files excluded. If estimation exceeds 80 production LOC, split before coding. The planning model commits every test before implementation; the 7B only registers that test and writes production code to make it pass. All task tests stay unregistered until their card begins. The sole exception is test-only T07a4, whose baseline is expected to pass.

## Required completion evidence (every card)
List exact files changed, production LOC count, test names, command lines and exit codes, expected baseline evidence, final test output, diff summary, and deviations. Run `python tools/check.py`, host build and CTest. Run sanitizer host suite when the card touches parsing/graph math. Hardware results are UNVERIFIED unless an owner records them in bringup-log.md. No golden regeneration to make a failure pass.

## T07a1 — Protection curve comparator
**Bundle/test:** [T07a1 prompt](prompts/T07a1.md); committed `host/tests/test_protect.c`.
**Goal/dependency:** implement ADR 0010 pure comparator; depends on ADR 0010, ADR 0011, and CODEOWNERS contract approval.
**Fixed behavior:** zero outputs or frequencies reject; non-finite response returns -1 with zeroed failure; malformed input leaves empty `out_id`; validate every record/input before any compare; require a true one-to-one ID mapping (outputs `{o,o}`, records `{o,p}` rejects). Comparator production <=80 LOC. Public `dspg_protect_t` belongs in `dspgraph.h`; no internal duplicate.

## T07a2 — Protection record validator
**Bundle/test:** [T07a2 prompt](prompts/T07a2.md); committed `host/tests/test_protect_record.c`.
**Dependency:** T07a1. Internal API includes explicit `n_records`; validate complete one-to-one record/output set, bounded IDs, finite values, limits, and disabled-band encoding. Production 30–80 LOC.

## T07a3 — Protection frequency sample list
**Bundle/test:** [T07a3 prompt](prompts/T07a3.md); committed `host/tests/test_protect_grid.c`.
**Dependency:** T07a2. Use 1054-point base grid plus exact endpoints/edges; max 1070. Capacity fixture uses two records with four distinct interior edges (100, 500, 2000, 8000), yielding 1058 unique values; capacity 1057 rejects without modifying output. Production 30–80 LOC.

## T07a4 — SOS response vectors (test-only)
**Bundle/test:** [T07a4 prompt](prompts/T07a4.md); committed `host/tests/test_sos_response.c`.
**Dependency:** T07a3. Adds only tests for existing `dsp_sos_eval`; expected baseline is pass after registration. No production LOC.

## T07a5 — Compiled-node response evaluator
**Bundle/test:** [T07a5 prompt](prompts/T07a5.md); committed `host/tests/test_node_response.c`.
**Dependencies:** T07a4, ADR 0011, and maintainer approval of the internal seam. Single-node response only; no graph walk or mix. Reuses the verified `rbj_eq.json` golden (-6.000003627305285 dB); never regenerate it. Production 30–80 LOC.

## T07a6 — Protection comparator integration
**State:** blocked; no executable bundle yet. Depends on T07a1–a5 and ADR 0011 graph response semantics and signature. Do not start until graph propagation tests and CODEOWNERS review are complete.

## T16 — SPSC audio frame ring
**Bundle/test:** [T16 prompt](prompts/T16.md); committed `host/tests/test_spsc_ring.c`. Independent host card, 30–80 production LOC. Caller-owned storage, C11 acquire/release, immediate partial read/write; caller owns blocking/backpressure.

## T17 — Pure TDM slot packer
**Bundle/test:** [T17 prompt](prompts/T17.md); committed `host/tests/test_tdm_pack.c`. Independent host card, 30–80 production LOC. Six planar channels map to slots 0–5 in design.md order; slots 6–7 are zero. Retain the invalid-slot rejection regression in `test_graph_validate` (`duplicate_slot.json`, E_SLOT).

## T18 — S7 ramp duration helper
**Bundle/test:** [T18 prompt](prompts/T18.md); committed `host/tests/test_s7_ramp.c`. Independent host card, 30–80 production LOC. At 48 kHz, directional minima are 240/960 samples; linear gain delta is at most 0.001 per sample.

## T19 — Fail-closed application state machine
**Bundle/test:** [T19 prompt](prompts/T19.md); committed `host/tests/test_app_state_machine.c`. Independent host card, 30–80 production LOC. No transition can enter RUNNING without valid protection and config; ramps and owner acknowledgments remain caller events.

## T20 — Config CRC and A/B slot selection
**Bundle/test:** [T20 prompt](prompts/T20.md); committed `host/tests/test_config_slot.c`. Independent host card, 30–80 production LOC. CRC-32/ISO-HDLC plus pure header validation and active/fallback selection; no flash or NVS access.

## T21 — Request bounds
**Bundle/test:** [T21 prompt](prompts/T21.md); committed `host/tests/test_request_bounds.c`. Independent host card, 30–80 production LOC. Enforces body 1..32768 bytes, depth 1..8, nodes <=64, strings <=32; retains E_NODE_PARAM rejection regression.

## T22 — Token authentication comparison
**Bundle/test:** [T22 prompt](prompts/T22.md); committed `host/tests/test_token_auth.c`. Independent host card, 30–80 production LOC. Fixed 32-byte content loop, no early exit, bounded caller buffers; retains E_NODE_PARAM rejection regression.

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
