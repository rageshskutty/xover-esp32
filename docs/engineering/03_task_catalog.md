# 7B execution catalog (planning iteration 2)

**Authority for this architecture iteration:** ADR 0012 (chain-per-output), ADR 0013 (protected block and limiter authority), ADR 0008 (protection curves), ADR 0010 (protection comparator seam), plus frozen `dspblocks` headers and `docs/api.md`. The graph-oriented T07a1–T07a6 cards below are historical planning material and are not executable under chain v1; they must be replaced by chain-response cards before implementation. If a card disagrees with an ADR/header, stop and report; never edit the interface.
**Baseline:** this catalog originated from `architecture/7b-executable-engineering-plan` at `d8ce744e06edf0c05a50734d7e2ec13c764cebf2`. The current product architecture is chain-per-output; graph-runtime tasks are retired. No chain executor or protection evaluator is implemented.
**Card sizing:** one card changes one behavior in one module. Target 30–80 handwritten production LOC; tests/golden data and generated files excluded. If estimation exceeds 80 production LOC, split before coding. All cards require test-first execution inside the same reviewable change: add the exact assertions, run and record the expected failing baseline, implement, rerun. Do not submit a red branch as complete.

## Required completion evidence (every card)
List exact files changed, production LOC count, test names, command lines and exit codes, first-fail evidence before implementation, final test output, diff summary, and any deviations. Run `python tools/check.py`, host build and CTest. Run sanitizer host suite when the card touches parsing/graph math. Hardware results are UNVERIFIED unless an owner records them in bringup-log.md. No golden regeneration to make a failure pass.

## T07a1 — Protection curve comparator
**Goal:** implement the exact pure comparator from ADR 0010; no graph traversal.
**Dependencies:** ADR 0010 accepted; host baseline.
**Files:** `firmware/components/dspgraph/dspgraph_protect.c`, `firmware/components/dspgraph/internal/dspgraph_protect.h`, `host/tests/test_protect.c`, `host/CMakeLists.txt`, `firmware/components/dspgraph/CMakeLists.txt`.
**Fixed API:** use the exact `dspg_protect_check(...)` declaration in ADR 0010. Do not rename fields or add options.
**Behavior:** validate pointer/length pairs, record ID correspondence, finite fields/curves and strictly increasing frequencies; compare ceiling and enabled band thresholds. Threshold equality passes. Deterministic failure order is output array order then increasing sample index. Success zeroes failure output.
**Tests first:** exact vectors in ADR 0008 plus max ceiling equality and just-over (0.00001 dB), NaN, infinity, duplicate/missing/unknown ID, disabled attenuation mismatch, duplicate or descending frequency, null pointers, output ordering.
**Size:** comparator only; if >80 production LOC split validation from comparison under maintainer direction.

## T07a2 — Protection record validator
**Goal:** isolate record validity so malformed protection never reaches DSP math.
**Dependencies:** T07a1.
**Files:** same protection source/header and test file only.
**Behavior:** expose internal `dspg_protect_record_valid(const dspg_protect_t*, const char *const *output_ids, size_t n_outputs)`; returns 1 iff exact output/record set, IDs NUL-terminated within 32 bytes, all fields finite, max_gain [-120,+24], band edge 0 or [10,20000], enabled attenuation [0,120], disabled attenuation exactly 0.0f. This internal function is not public API.
**Tests:** each lower/upper bound, one below/above, empty ID, 31-byte ID plus NUL, unterminated ID, duplicate output, missing and unknown IDs, disabled-band inconsistency.

## T07a3 — Protection frequency sample list
**Goal:** add deterministic sample list from `dsp_log_grid` and exact enabled band edges.
**Dependencies:** T07a2.
**Files:** protection source/internal header/tests only.
**Behavior:** exact internal signature: `int dspg_protect_grid(const dspg_protect_t *records, size_t n_records, double *out_freqs, size_t capacity, size_t *out_count)`; start with 1054 values from 10 to 20000 Hz, append 10/20000 and enabled edges, exact-float deduplicate, ascending sort. On insufficient capacity return -1, set *out_count=0, and leave out_freqs unchanged. Maximum count is 1054 plus unique enabled edges, at most 1070 for eight outputs.
**Tests:** no bands => 1054 values; both unique edges => count 1056; edges at 10/20000 deduplicate; sorted strict; capacity 1057 for 1058 unique fails. Verify endpoints exactly 10.0 and 20000.0.

## T07a4 — Single SOS response curve golden vectors
**Goal:** prove existing SOS complex evaluation can produce dB samples without graph API.
**Dependencies:** T07a3.
**Files:** `host/tests/test_protect.c` and test build registration only.
**Behavior:** test harness calls current `dsp_sos_eval` for a one-section unity biquad (b=[1,0,0], a=[0,0]); expected 0 dB at 10, 1000, 20000 Hz. Add constant gain using b0=0.5,b1=b2=a1=a2=0; expected -6.020599913 dB within 1e-5 dB. This card adds no production implementation; if dsp_sos_eval cannot represent a fixture, stop and propose a contract change.
**Size:** test-only, no golden generator or SciPy dependency.

## T07a5 — Compiled-node response evaluator seam
**Goal:** add host-only internal function for one compiled node's complex response.
**Dependencies:** T07a4 green; design approval by maintainer because a new internal seam is required.
**Files:** internal dspgraph response source/header, host test, CMake.
**Fixed behavior:** use node's already-compiled SOS cascade and existing `dsp_sos_eval`; identity for input/output/gain(0)/polarity(non-inverted); multiply section responses in cascade order. Any unsupported node kind or non-finite result returns error. No graph walk, mix, or mutation.
**Golden:** identity 0 dB; -6.020599913 dB gain vector; use host/tests/golden/rbj_eq.json entry type=peq, f0_hz=1000, q=0.7, gain_db=-6.0, and its 1000-Hz sample expected -6.000003627305285 dB within 0.001 dB. Never regenerate it.
**Size:** only one compiled node.

## T07a6 — Protection comparator integration
**Goal:** call the pure comparator from graph build only after graph curves exist.
**Dependencies:** T07a1–a5, graph curve propagation design ADR approved, all golden vectors committed.
**Files:** graph build/protection source, host tests, CMake, docs/api.md.
**Stop rule:** current repository has no final response-curve-by-output seam. Do not invent fanout/mix semantics. A maintainer must add and approve that ADR and exact signature before this card is considered queued.

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

## Step 3 — Chain JSON v1 schema and semantic contract

**State:** schema, 2.1 example, chain examples, and negative fixtures drafted on
`architecture/chain-per-output-protection-v1`. Tool execution is still
required before this task can be marked verified.

**Files:** `configs/schema.json`, `configs/example_2way_sub.json`,
`configs/example_lr_difference.json`, `configs/invalid/*.json`,
`tools/check.py`, ADRs 0012/0013, `docs/design.md`, `docs/architecture.md`,
`docs/requirements.md`, and `docs/api.md`.

**Frozen schema contract:**
- `v=1`, `fs=48000`; top-level `inputs` is a unique non-empty subset of
  `L,R,C,LFE,SL,SR`; optional `pre`; 1–8 outputs.
- `pre[].channel`, each output's `in[].channel`, output `id`, and output
  `slot` are unique in their respective scopes. All referenced channels
  must be in `inputs`. A missing pre-chain is identity.
- Each output has 1–8 input mix entries. Weights are finite, non-zero and
  inclusive [-4,+4]. Negative weights are valid; protection uses absolute
  weights.
- Slot 0–31 is only a schema sanity range. The protected TDM slot count is
  authoritative. Output `id` joins one-to-one with protected `out_id`.
- Chain block forms are closed in the JSON Schema: designed biquad, gain,
  delay, polarity, and fs-tagged SOS. PEQ alone may carry optional `role`.
- `protect` permits optional `attack_ms`, `release_ms`, and
  `lookahead_ms=0). Missing attack/release use protected `lim_default`;
  out-of-bounds values return `E_LIMIT`; non-zero lookahead and any
  threshold/ceiling/margin authority key return `E_SCHEMA`.
- Empty `chain` passes JSON Schema but fails semantic protection with
  `E_PROTECT`. Static response validation must prove attenuation below the
  protected `min_freq_hz`; a non-empty chain is not automatically safe.
- Cross-field uniqueness, channel references, protected-record joins,
  limiter bounds/defaults, TDM slot authority, and static response checks are
  semantic rules, not JSON Schema features.

**Acceptance:** validate the schema itself; every positive config passes;
every fixture under `configs/invalid/` fails with its exact `_expect` code;
test inclusive weights -4/+4 and a negative mix; ensure fixtures use
synthetic protected metadata only and never imply hardware-approved values.
Run `python tools/check.py`, host build, and CTest. Record command output;
until then all changes remain UNVERIFIED.
