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
**State:** split into micro-cards T23–T27. The pin map is merged (`docs/hw-pinmap.md`) and PCM5122 register values are taken from cited datasheet sections (`docs/hw-verification-notes.md`); neither is hardware-verified. T23, T24, T25 and T26 are merged; T27 implements the tdmout ESP-IDF TDM driver with the 12.288 MHz MCLK/BCLK register override. T27 is now `needs_rework`: the single 8 x 32-bit frame is not possible on the ESP32-S3 (ADR 0015, Proposed), and the rework is carried by T28, T29a–T29c, T30, T30b and T33 below. Procedure for the human bring-up is `docs/prebringup.md`; results go only in `docs/bringup-log.md`.

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

---

# Target-layer rework cards (T28–T33), authored 2026-11-24

**Authority:** ADR 0015 (TDM feed, two 4 x 32-bit lanes; Proposed), `docs/review/2026-11-24-correction-plan.md` (findings B1, H2, H4, M2, M3; Steps 3, 4, 5, 7), AGENTS C1, C3, S1, V1–V4. Sources for every number below are named in the card that uses it.

**Order and gates**

| Card | State | Waits for |
|---|---|---|
| T28 | open | nothing |
| T30 | open | nothing (parallel with T28) |
| T30b | open, partly human | datasheet citation for the `0x05` mask |
| GATE-SPIKE | human | a bench with any ESP32-S3 board, no DAC |
| GATE-ADR | human | maintainer decision on ADR 0015, 0016, 0017 |
| GATE-DOUT2 | human | GPIO number for the DAC2 data line |
| T29a | blocked | GATE-SPIKE, GATE-ADR |
| T29b | blocked | T28 merged, T29a merged, GATE-SPIKE, GATE-DOUT2 |
| T29c | blocked | T29b merged |
| T33 | blocked | T29b merged, GATE-DOUT2 |

The PCM5122 SCK-ratio question from the correction plan (step 5.0) is closed: ADR 0015 "Datasheet check" records that 384 x fS at 48 kHz is allowed (SLAS763C, Table 32, Table 104, Table 133). Do not re-open it inside a card; if a card finds a conflict, stop and write it to `docs/questions.md`.

T29 from the plan is split into T29a–T29c because it exceeds the 80-line card size. `board_config.h` constants move from the plan's Step 3 into T29b: `tdmout.c` asserts 8 slots today, so the 4-slot constants and the tdmout rework have to compile together. This is a deviation from the plan order and is stated in the PR that introduces these cards (V4).

Test ownership: `tools/check.py` fails if two tasks own the same test. `test_tdm_pack` and `test_pcm5122_ctl` stay with T27 and T25 in `task_state.json`; T29a and T29c add assertions to those files and do not claim them.

## T28 — Target CI job and TDM frame-size assertion

**State:** open. No dependency. Must be reviewed before T29b so the current 8 x 32 code fails red.

**Objective:** CI builds the firmware for ESP32-S3 in four bring-up configurations with `-Werror`, and `tdmout.c` cannot compile a TDM frame larger than the S3 limit.

**Files (exact, nothing else):**
- `.github/workflows/ci.yml` — new job `target-build` (CODEOWNERS path)
- `firmware/CMakeLists.txt` — option `XOVER_WERROR`
- `firmware/components/tdmout/tdmout.c` — one `_Static_assert` (CODEOWNERS path)

**Production size:** about 45 lines of workflow YAML, 8 of CMake, 2 of C. No public header changes, no ADR needed (C5 not triggered).

**Job contract:**
- Container `espressif/idf:v5.5.6`; `actions/checkout@v4` with `submodules: recursive`.
- Matrix `mode`: `none`, `pinwalk`, `pinwalk-mute`, `i2cscan`; `fail-fast: false`.
- Kconfig per mode, appended after `sdkconfig.defaults` through `SDKCONFIG_DEFAULTS`:
  - `none`: no override (Kconfig default `XOVER_BRINGUP_MODE_NONE`)
  - `pinwalk`: `CONFIG_XOVER_BRINGUP_MODE_PINWALK=y`
  - `pinwalk-mute`: `CONFIG_XOVER_BRINGUP_MODE_PINWALK=y` and `CONFIG_XOVER_BRINGUP_PINWALK_MUTE=y`
  - `i2cscan`: `CONFIG_XOVER_BRINGUP_MODE_I2CSCAN=y`
- One build directory and one `sdkconfig` per mode; target `esp32s3` given by `-DIDF_TARGET=esp32s3`.
- `-DXOVER_WERROR=ON`. In `firmware/CMakeLists.txt` this adds `-Werror` to the repository's own components only (`main`, `audiosrc`, `dspblocks`, `pcm5122`, `tdmout`, `xoweb`). ESP-IDF components and managed components (`espressif/esp-dsp` is unpinned `*` in `firmware/main/idf_component.yml`) keep their upstream warning policy.

**Assertion (add after the four existing asserts in `tdmout.c`; `hal/i2s_ll.h` is already included there):**
```c
_Static_assert((XOVER_TDM_SLOT_BITS * XOVER_TDM_SLOTS_PER_FRAME) <= I2S_LL_SLOT_FRAME_BIT_MAX,
               "TDM frame exceeds the ESP32-S3 I2S frame limit (I2S_LL_SLOT_FRAME_BIT_MAX); see ADR 0015");
```
No literal 128 may replace the macro.

**Accepted / rejected:** with `XOVER_TDM_SLOT_BITS` 32 and `XOVER_TDM_SLOTS_PER_FRAME` 4 (128 <= 128) the assertion passes. With 8 slots (256 > 128) the build fails with the message above.

**Expected baseline (first-fail evidence):** on this card's branch, all four matrix jobs fail while compiling `tdmout.c` with the assertion message. A run that fails for any other reason (image pull, macro not found, another warning) is not the expected baseline; stop.
**Expected final state:** after T29b merges, all four jobs are green.

**Commands:**
```
python tools/check.py
cmake -S host -B host/build -DCMAKE_BUILD_TYPE=Release && cmake --build host/build && ctest --test-dir host/build --output-on-failure
```
The target build evidence comes only from the GitHub Actions run URL. There is no ESP-IDF toolchain in the agent sandbox, so any target result not taken from Actions is UNVERIFIED (V2).

**Acceptance:**
1. Four `target-build` jobs appear in the Actions run, one per mode.
2. Each fails at the new assertion on this branch; paste the log excerpt (C6).
3. The four existing host jobs are unchanged and green.
4. `python tools/check.py` PASS.
5. The workflow diff contains no `continue-on-error`, no `-Wno-error`, and no unpinned image tag.

**Rules (5):** C1, C3, C6, V2, V3.

**Stop conditions:**
- `espressif/idf:v5.5.6` cannot be pulled. Do not substitute a floating tag; record the question in `docs/questions.md`.
- `I2S_LL_SLOT_FRAME_BIT_MAX` is not defined in v5.5.6 `hal/i2s_ll.h`. Record it in `docs/questions.md`; do not use a literal.
- A warning in a project component needs more than a one-line fix. Do not suppress it; report it and split a card.
- `-Werror` cannot be limited to project components.

**Merge order:** this card is red by design until T29b lands. The maintainer decides whether to merge it with T29b in one window, accept a red `main` meanwhile, or keep the job non-required until then (V4).

## T29a — `tdm_pack_4slot` (host-only)

**State:** blocked on GATE-SPIKE and GATE-ADR.

**Objective:** replace the 8-slot packer with a 4-slot packer used once per lane.

**Files:** `firmware/components/tdmout/internal/tdm_pack.h`, `firmware/components/tdmout/tdm_pack.c`, `host/tests/test_tdm_pack.c`. The header is an internal header (PRIV include dir), so C5 is not triggered.

**Signature (replaces `tdm_pack_8slot`):**
```c
#define TDM_PACK_SLOTS_PER_FRAME 4U  /* 4 x 32-bit words, one lane frame */
int tdm_pack_4slot(const int32_t *const *channels, size_t n_channels,
                   size_t n_frames, int32_t *interleaved, size_t out_words);
```
Slot `i < n_channels` takes `channels[i]`; the remaining slots are zero. Lane A passes the four channels of DAC0 and DAC1 (slots 0 and 1 = DAC0 at offset 0 BCK, slots 2 and 3 = DAC1 at offset 64 BCK). Lane B passes the two channels of DAC2 and gets slots 2 and 3 as zero. The channel-to-DAC mapping follows `board_config.h` and ADR 0015.

**Accepted:** `n_channels` 1 to 4, all pointers non-NULL, `out_words >= n_frames * 4`, `n_frames == 0` returns 0 without touching the buffer.
**Rejected (return -1, output buffer not modified):** `n_channels == 0` or `> 4`, `channels == NULL`, any used `channels[i] == NULL`, `interleaved == NULL` with `n_frames > 0`, `n_frames > SIZE_MAX / 4`, `out_words < n_frames * 4`.

**Expected values (tests first):** `n_channels = 4`, `n_frames = 2`, `ch[c][f] = 0x1000 * (c + 1) + f` gives `{0x1000, 0x2000, 0x3000, 0x4000, 0x1001, 0x2001, 0x3001, 0x4001}`. `n_channels = 2` with the same inputs gives `{0x1000, 0x2000, 0, 0, 0x1001, 0x2001, 0, 0}`. Value tolerance: exact. The existing NULL and size checks from `test_tdm_pack.c` are kept and adapted.

**Expected baseline:** the new assertions fail to compile or link because `tdm_pack_4slot` does not exist.

**Commands:** `python tools/check.py`; host build; `ctest --test-dir host/build --output-on-failure`; ASan/UBSan suite (`host/build-san`).

**Rules (5):** C1, C2, C4, R1 not applicable (not an `_rt` file), V3.

**Stop:** if the lane mapping in ADR 0015 changes at approval, stop and re-issue this card.

## T29b — `tdmout` two-lane driver and `board_config.h` constants

**State:** blocked on T28 merged, T29a merged, GATE-SPIKE (ADR 0015 Accepted), GATE-DOUT2.

**Objective:** `tdmout` runs I2S0 as master with a 4 x 32-bit frame and I2S1 as slave on the shared BCLK/WS, using the stock ESP-IDF clock path.

**Files:** `firmware/main/board_config.h` (CODEOWNERS), `firmware/components/tdmout/tdmout.c` (CODEOWNERS), `firmware/components/tdmout/include/tdmout.h` if the public signature must change (then C5: ADR note plus `docs/api.md`).

**Required inputs from the maintainer (S1, never guessed):** the GPIO for `XOVER_PIN_I2S_DOUT2`, the I2S1 slot mode and slot mask that the spike recorded in `docs/bringup-log.md`.

**`board_config.h` changes:** `XOVER_TDM_SLOTS_PER_FRAME 4`, `XOVER_TDM_SLOT_MASK 0x0F`, new `XOVER_PIN_I2S_DOUT2` added to every pin check (unusable-pin, strapping and distinct-bit checks), `XOVER_DAC2_TDM_OFFSET_BCK 0` on lane B. The assertion `XOVER_DAC2_TDM_OFFSET_BCK == 4 * XOVER_TDM_SLOT_BITS` must be replaced by a per-lane check: DAC1 offset equals `2 * XOVER_TDM_SLOT_BITS` on lane A, DAC2 offset is 0 on lane B, and every offset is smaller than the lane frame length.

**`tdmout.c` changes:**
- Update the four existing `_Static_assert`s to 4 slots and mask `0x0F`; keep the T28 frame-size assert.
- Delete `tdmout_apply_clock_override`, `TDMOUT_MCLK_DIV_*`, `TDMOUT_BCK_DIV` and every `i2s_ll_*` call. Set `mclk_multiple` to 384, which gives 18.432 MHz MCLK and a master `bclk_div` of 3 on the stock path (ADR 0015).
- Add the I2S1 slave channel and its DOUT2 pin. Enable I2S1 first, then I2S0, so both begin on the same frame.
- Document in `tdmout.h` that the caller must have ramped output to zero before `tdmout_stop` (AGENTS S7). `tdmout_stop` itself does not ramp.

**Expected baseline:** after T29a and with the constants changed first, the old `tdmout.c` fails to compile on its own asserts. This is the first-fail evidence.

**Acceptance:** four `target-build` jobs green (Actions run URL, C6); `python tools/check.py` PASS; host suite green. Everything about timing, alignment, MCLK value and pin behaviour stays UNVERIFIED until a human records scope results in `docs/bringup-log.md` (V1).

**Rules (5):** C1, C3, S1, S7, V2.

**Stop:**
- The spike shows DAC2 data late by one BCLK or more, or the driver reconfigures shared BCLK/WS as input-only: ADR 0015 fails and Option C applies. Do not start this card.
- The pin number or I2S1 mode is missing: write to `docs/questions.md`, stop.
- The change exceeds 80 production lines: split by file before coding.

## T29c — `pcm5122_status_is_running_48k_tdm` derives its expectations

**State:** blocked on T29b merged.

**Objective:** remove the hard-coded 256 BCK and 256 x fS expectations.

**Files:** `firmware/components/pcm5122/pcm5122_ctl.c` (CODEOWNERS), `host/tests/test_pcm5122_ctl.c`.

**Contract:** `bck_per_frame` is derived from `XOVER_TDM_SLOT_BITS * XOVER_TDM_SLOTS_PER_FRAME` (128 after T29b). `sck_ratio_code` is derived from the MCLK multiple (384 maps to code `0111`, Reg 0x5B DTSR, SLAS763C Table 104, as recorded in ADR 0015); the function must compare both derived values against the status registers, not constants.

**Tests first (one case each, exact values):** a status with `bck_per_frame` 128 and `sck_ratio_code` 7 returns running; a status with 256 BCK and code 6 (the old values) returns not running; a status with 128 BCK and code 6 returns not running; a status with 256 BCK and code 7 returns not running.

**Expected baseline:** the first case fails against the current implementation, which still expects 256 and code 6.

**Rules (5):** C1, C2, C4, S1, V3.

**Stop:** if a register field name or code is not in a cited datasheet section already recorded in `docs/hw-verification-notes.md` or ADR 0015, write it to `docs/questions.md` and stop that case (S1).

## T30 — PINWALK bring-up fixes

**State:** open. No dependency. Target code only; there is no host test for `main.c`, so the check is the T28 build job once it exists, and the hardware behaviour stays UNVERIFIED.

**Objective:** make PINWALK safe for the DAC mute line and correct for the I2C pins.

**Files:** `firmware/main/main.c`, `firmware/main/Kconfig.projbuild` (help text only).

**Changes (all in `firmware/main/main.c` unless noted):**
1. `XOVER_PIN_DAC_MUTE_N` is never driven high in PINWALK. When `CONFIG_XOVER_BRINGUP_PINWALK_MUTE` is set, the pin still has its 3 s slot in the walk, but is held low and only logged as "held low". It is never switched to `GPIO_MODE_INPUT`. Rationale: `app_main` drives it low as its first action (AGENTS S2) and the board has only a 470 ohm pull-down; the walk currently sets it high for 3 s and then leaves it floating.
2. SDA and SCL are configured `GPIO_MODE_OUTPUT_OD` in the walk, not push-pull `GPIO_MODE_OUTPUT`, because they are an open-drain bus with external pull-ups on the DAC boards.
3. The log line `PINWALK button gpio0=%d` prints `XOVER_PIN_ADMIN_BUTTON` instead of the literal `gpio0`.
4. `Kconfig.projbuild`: the help for `XOVER_BRINGUP_PINWALK_MUTE` says the mute pin is held low and logged, not walked high.

**Expected baseline:** none by test. The evidence is the diff plus a green `target-build` matrix (all four modes, including `pinwalk-mute`) after T28 and T29b have landed. If T30 merges before that, record the build result as UNVERIFIED.

**Rules (5):** C1, C3, S1, S2, V1.

**Stop:** if the current `main.c` no longer matches the behaviour above, re-issue the card. Do not change any other mode.

**Deferred, not in this card:** `BRINGUP_TONE_MAX_DBFS` is hard-coded -30 and not tied to `CONFIG_XOVER_BRINGUP_MAX_DBFS` (finding M1). Fix it when the tone mode lands: cap equals the smaller of -30 and the Kconfig value, with a test that a lower value lowers the cap.

## T30b — Cite or remove the `0x05` clock-error mask

**State:** open, partly human.

**Objective:** `PCM5122_CLK_ERR_FLAGS 0x05` in `firmware/components/pcm5122/pcm5122_ctl.c` has no datasheet citation (S1, finding M3).

**Files:** `firmware/components/pcm5122/pcm5122_ctl.c` (CODEOWNERS), `docs/hw-verification-notes.md`, `docs/questions.md` only if the datasheet is unclear.

**Rule of the card:** a human supplies the SLAS763C register and table that define the bits, or the card removes the mask and the check that uses it. The agent must not infer the bit meanings.

**Expected baseline:** none by test; `test_pcm5122_ctl` must stay green with unchanged expectations if the citation is added, and must be updated test-first if the mask is removed.

**Rules (5):** C1, S1, V1, V3, C8.

**Stop:** datasheet section not supplied.

## T33 — Hardware docs re-baseline for ADR 0015

**State:** blocked on T29b merged and GATE-DOUT2. Docs only, human review (CODEOWNERS covers `docs/design.md` and `docs/adr/`).

**Objective:** every doc that states 8 slots, 256 BCK or 12.288 MHz MCLK is made consistent with the merged constants.

**Files:** `docs/hw-pinmap.md` (sections 3 and 4: add DOUT2 and the BCLK/WS jumper to I2S1), `docs/hw-verification-notes.md` (section 1 row that says 8 slots of 32 bit is right), `docs/prebringup.md`, `docs/bom.md` (jumper wire), `docs/design.md`, `docs/adr/0009-dsp-timing-boundaries.md` only if it quotes block sizes.

**Contract:** each changed statement cites ADR 0015 or a datasheet section; no number appears that is not in `board_config.h` or a cited datasheet. Every hardware statement keeps the label UNVERIFIED until a human records the check in `docs/bringup-log.md`.

**Acceptance:** `rg "8 slots|256 BCK|12\.288" docs firmware` returns only historical text that is marked as such; `python tools/check.py` PASS.

**Rules (5):** C1, C7, S1, V1, V4.

**Stop:** two docs disagree after the edit; write to `docs/questions.md`.

## GATE-SPIKE — Step 1 bench check (human, about 30 minutes, no DAC)

Any ESP32-S3 board, ESP-IDF v5.5.6. Record each result in `docs/bringup-log.md`; the agent never writes HW-VERIFIED.

1. TDM master, 8 slots x 32 bit, `i2s_channel_init_tdm_mode`. Expected: `ESP_ERR_INVALID_ARG` and a log line about exceeding the maximum of 128. If it succeeds on v5.5.6, finding B1 is withdrawn and T29a to T29c shrink to documentation.
2. 4 slots x 32 bit, mask `0x0F`, `mclk_multiple` 384. Expected: OK. Scope: BCLK 6.144 MHz, MCLK 18.432 MHz, WS one-BCK pulse every 128 BCK, no `bclk_div` warning in the log.
3. I2S1 as slave on the shared BCLK/WS, I2S1 started before I2S0. Scope DOUT1 against DOUT0. Check that DAC2 data is not one or more BCLK late, and that the driver does not turn the shared BCLK/WS pins into input-only.
4. Write down the I2S1 slot mode and mask that worked. T29b needs them.

If item 3 fails: ADR 0015 falls back to Option C (8-channel TDM DAC); T29a to T29c and T33 are re-issued.

## GATE-ADR — ADR decisions (human)

ADR 0015 (TDM feed), 0016 (EQ float32 accepted limit) and 0017 (peak gain cap) are all Proposed. ADR 0015 becomes Accepted only after GATE-SPIKE passes. ADR 0016 and 0017 need an explicit accept or reject by the maintainer; Step 4 of the product plan stays blocked until then.

## GATE-DOUT2 — DAC2 data pin (human)

`XOVER_PIN_I2S_DOUT2` needs a GPIO number that is not a strapping, flash, PSRAM, USB or console pin and is distinct from every pin in `board_config.h`. It is a pin-map fact (AGENTS S2 authority list, S1), so the agent does not choose it.
