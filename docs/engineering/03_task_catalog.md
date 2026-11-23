# 7B execution catalog (planning iteration 2)

**Authority for this architecture iteration:** ADR 0012 (chain-per-output) and ADR 0014 (firmware protection contract; supersedes the protection decisions of ADRs 0008, 0010 and 0013), plus frozen `dspblocks` headers and `docs/api.md`. The graph-oriented T07a1–T07a6 cards below are historical planning material and are not executable under chain v1; they must be replaced by chain-response cards before implementation. If a card disagrees with an ADR/header, stop and report; never edit the interface.
**Baseline:** this catalog originated from `architecture/7b-executable-engineering-plan` at `d8ce744e06edf0c05a50734d7e2ec13c764cebf2`. The current product architecture is chain-per-output; graph-runtime tasks are retired. No chain executor or protection evaluator is implemented.
**Card sizing:** one card changes one behavior in one module. Target 30–80 handwritten production LOC; tests/golden data and generated files excluded. If estimation exceeds 80 production LOC, split before coding. All cards require test-first execution inside the same reviewable change: add the exact assertions, run and record the expected failing baseline, implement, rerun. Do not submit a red branch as complete.

## Required completion evidence (every card)
List exact files changed, production LOC count, test names, command lines and exit codes, first-fail evidence before implementation, final test output, diff summary, and any deviations. Run `python tools/check.py`, host build and CTest. Run sanitizer host suite when the card touches parsing/graph math. Hardware results are UNVERIFIED unless an owner records them in bringup-log.md. No golden regeneration to make a failure pass.

## Retired graph-era protection cards (T07a1–T07a6)

These cards are retained in Git history only. They specify graph-era APIs and dependencies that do not exist in chain-per-output v1. They are not queued, blocked implementation work, and must not be used as 7B instructions. In particular, do not implement `dspg_protect_check`, `dspg_protect_record_valid`, `dspg_protect_grid`, or `dspg_node_response` from these cards. A replacement chain-v1 protection plan requires a reviewed numerical contract and oracle before cards are authored.

## T07b — Protection storage and provisioning
**State:** human-blocked. Requires owner-approved record values/ranges, NVS layout, serial provisioning transport and CODEOWNERS approval. No task card until these facts are provided. Runtime must remain muted with missing/corrupt records.

## T08 — Board and DAC integration
**State:** split into micro-cards T23–T27. The pin map is merged (`docs/hw-pinmap.md`) and PCM5122 register values are taken from cited datasheet sections (`docs/hw-verification-notes.md`); neither is hardware-verified. T23, T24 and T26 are ready; T25 follows T23. T27 (the ESP-IDF TDM driver) stays human-blocked until the IDF TDM details and the two open TDM timing items are settled. Procedure for the human bring-up is `docs/prebringup.md`; results go only in `docs/bringup-log.md`.

## T09 — Graph runtime (retired)

**State:** retired with the graph/DAG architecture by ADR 0012. Do not create graph runtime tasks from ADR 0007 or the old graph task bundle.

## Chain runtime contract (future work; not Step 4)

The chain-per-output executor, bounded execution-table layout, memory ownership, DSP-task processing contract, and chain response API require their own reviewed component contract. They are not defined by the protection comparator. No implementation card may infer these interfaces from graph-era headers.

## T10–T15 — Source, control plane, bring-up and release

**State:** dependent human/architecture gates remain. Existing broad cards are retired; they are not executable work orders. Reissue them as micro-cards only after the chain runtime contract, T07b protection provisioning, and T08 board facts are reviewed and explicit per-card signatures, test inputs and expected outputs are committed. No 7B may decide external dependencies, hardware behavior, protection policy, licenses or release status.

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
`configs/example_lr_difference.json`, `configs/example_weight_boundaries.json`, `configs/invalid/*.json`,
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
every chain-v1 fixture under `configs/invalid/` fails with its exact `_expect`
code. Fixtures cover declared-input membership, all uniqueness scopes,
weight bounds/zero/negative acceptance, array bounds, version/sample-rate,
block shape, PEQ-only role, look-ahead, designer threshold/ceiling/margin
keys, protected record missing/unknown keys, output/protected-ID join,
limiter bounds, unstable `x_model`, `vpk_at_0dbfs > clip_v`, protected TDM
slot count, static sum-bound violation, missing subsonic HP, empty chain,
and SOS sample-rate mismatch. Legacy graph fixtures remain explicitly
excluded from the chain-v1 schema gate and continue to feed the old C
validator regression test. Synthetic protected metadata is test-only and
must never imply hardware-approved values. Run `python tools/check.py`, host
build, CTest, sanitizers, and coverage; record command output.
