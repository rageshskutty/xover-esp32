# Review of main @ e16d25d and step-by-step correction plan (rev. 2)

Date: 2026-11-24. Reviewer: Claude (agent). Scope: full clone of `main` (284 commits, T01-T27 merged).
Rev. 2: second pass that re-checked every finding against the code on `main` (8a01f53), ESP-IDF v5.5.1 source and docs, and a numeric re-run of the EQ error figures. Changes from rev. 1 are listed in section 5.
Labels follow AGENTS V1: **HOST-VERIFIED** = I ran it; **SOURCE-VERIFIED** = I read the cited upstream source, nothing run on hardware; **UNVERIFIED** = not checked. Nothing here is HW-VERIFIED.

## 0. What I ran (HOST-VERIFIED)

| Check | Result |
|---|---|
| `python tools/check.py` | PASS (all `configs/invalid/*` rejected with the expected code) |
| host build `-Wall -Wextra -Wconversion -Werror`, `ctest` | 13/13 pass |
| same with ASan + UBSan | 13/13 pass |
| `tools/test_scipy_compare.py` | PASS, worst case 2.9e-9 dB / 2.9e-10 deg vs SciPy |
| `tools/test_protection_oracle.py` | 26 tests OK |
| EQ float32 error re-run (NumPy, rev. 2) | reproduces the H5 table; denominator-only hi/lo test added |

Not run: coverage gate, `idf.py build` (no toolchain here), anything on hardware.

The DSP core (`dspblocks`), SPSC ring, S7 ramp helper and PCM5122 register table are in good shape. The problems are in the target layer (compiled locally per the T26/T27 PR texts, but not covered by CI and never run on hardware) plus planning-document integrity.

## 1. Findings, by severity

### BLOCKER

**B1. The 8 x 32-bit TDM frame is not possible on ESP32-S3. `tdmout_init()` will fail at runtime.** (SOURCE-VERIFIED, ESP-IDF v5.5.1; the repo pins v5.5.6, re-check that tag)
- `components/hal/esp32s3/include/hal/i2s_ll.h`: `I2S_LL_SLOT_FRAME_BIT_MAX 128`.
- `esp_driver_i2s/i2s_tdm.c`: `i2s_tdm_set_slot` returns `ESP_ERR_INVALID_ARG` when `total_slot * slot_bits > 128`. `tdmout.c` asks for 8 x 32 = 256.
- IDF programming guide, TDM section: on targets without `SOC_I2S_TDM_FULL_DATA_WIDTH` only 4 slots at 32 bit are supported. `soc_caps.h` for ESP32-S3 has no such define.
- Repo claims the opposite: `docs/hw-pinmap.md` s4, `docs/hw-verification-notes.md` s1 ("8 slots of 32 bit is right"), `board_config.h`, `tdmout.c` static asserts, `tdm_pack_8slot`, T24-T27, `pcm5122_status_is_running_48k_tdm` (`bck_per_frame == 256`).
- Why nothing caught it: CI has no target build, and the check is a runtime return code. `hw-verification-notes.md` only checked the PCM5122 side; its own item 3 says the ESP-IDF TDM docs "have not been read yet".
- Constraint behind it: PCM5122 needs 128 or 256 BCK per frame, 6 x 32 = 192 is illegal, and the S3 allows at most 128. So 6 channels at 32 bit cannot share one S3 TDM lane.
- Caveat: I read IDF source, not the S3 TRM. The 128-bit limit is a hardware constant in the LL header, so I expect it to hold on v5.5.6, but that is not checked.

### HIGH

**H1. `docs/engineering/task_state.json` contains the single word `PLACEHOLDER`.** (HOST-VERIFIED) `PLAN.md` names it as the gate authority ("superseded by ... task_state.json"). `AGENTS.md` does not mention it (rev. 1 said AGENTS did). `repository_files.json` has `basis_commit` cd2ae0d (stale, rev. 1 claim, not re-checked in rev. 2) and lists legacy paths.

**H2. CI never builds the firmware.** `ci.yml` has host jobs only, so `tdmout.c`, `main.c`, Kconfig modes and `-Werror` on target (AGENTS C3) are unchecked in CI. Correction to rev. 1: the target layer has been compiled; the T26 and T27 PR texts record local `idf.py build` successes. The gap is CI coverage, not that nobody compiled it. A compile-time assert on `I2S_LL_SLOT_FRAME_BIT_MAX` would have caught B1.

**H3. Four orphan tests on main.** `test_config_slot.c`, `test_app_state_machine.c`, `test_request_bounds.c`, `test_token_auth.c` include `config_slot.h`, `app_state_machine.h`, `xoweb_bounds.h`, `xoweb_auth.h`. The tests are in `host/tests/` and not in `host/CMakeLists.txt`, so they never build. The headers are not in `firmware/main` or `xoweb/include`. (HOST-VERIFIED on `main`; the "exist on no branch" claim from rev. 1 was not re-checked across other branches.)

**H4. PINWALK breaks S2/S3 when `XOVER_BRINGUP_PINWALK_MUTE=y`** (`firmware/main/main.c`). Severity lowered from HIGH to **MEDIUM**: the option is off by default.
- it drives `DAC_MUTE_N` high (= unmute) for 3 s;
- it then sets that pin to INPUT, leaving it floating on only the 470 ohm pull-down; it never goes back to output-low;
- the log text hard-codes `gpio0` while the pin comes from `board_config.h`;
- SDA/SCL are driven push-pull on an open-drain bus.

**H5. EQ blocks use plain float32 coefficients, below the repo's own gate G2** (`dspblocks.c`: `dsp_design_peq`, `_shelf`, `_allpass`, `_notch` set `da1 = da2 = 0`). Not new: ADR 0005 already lists this as a known limit that also needs the numerators handled. Response error from float32 rounding alone, against the double design (HOST-VERIFIED, NumPy):

| filter | max error 10-200 Hz |
|---|---:|
| 30 Hz, Q 5, -6 dB | 0.172 dB |
| 20 Hz, Q 3, +6 dB | 0.106 dB (0.089 dB inside the gated band, 20 Hz and up) |
| 40 Hz, Q 8, +6 dB | 0.055 dB |
| 60 Hz, Q 5, +6 dB | 0.021 dB |
| 100 Hz, Q 5, +6 dB | 0.005 dB |

G2 says float32 within 0.01 dB of double above 20 Hz. `test_f32.c` calls only `dsp_design_xo`, so the gate never saw these. The sub (room PEQ, subsonic HP) is exactly where it matters. Related: `dsp_check_stable` tests only the float hi part with float arithmetic; at 10 Hz the margin `1 + a1 + a2` is about 1.7e-6 against float eps 1.2e-7.

**Rev. 1's fix (hi/lo on the denominator only) does not work.** Errors in numerator and denominator partly cancel, so fixing one side exposes the other:

| Case | All float32 | Denominator hi/lo only | Numerator and denominator hi/lo |
|---|---:|---:|---:|
| 30 Hz, Q5, -6 dB | 0.172 dB | 0.202 dB | ~0 |
| 30 Hz, Q5, +6 dB | 0.056 dB | 0.115 dB | ~0 |
| 100 Hz, Q5, +6 dB | 0.005 dB | 0.012 dB | ~0 |

Both sides need hi/lo. That grows `dsp_sos_t` (public header), changes the process loop, and C5 requires an ADR.

**H6. Q2 still open: schema allows `gain_db` -24..+24, C designers reject above +12** (`dsp_design_peq` line 93). The oracle follows the schema, the firmware follows the C. A config that validates can fail at build. Already tracked as Q2. The schema's `filter` definition is shared with the protection-weighting filters, so changing the bound reaches beyond the EQ cards.

### MEDIUM

- **M1. (latent)** `BRINGUP_TONE_MAX_DBFS` is hard-coded -30 and not tied to `CONFIG_XOVER_BRINGUP_MAX_DBFS` (range -120..-30). `main.c` has no tone mode and nothing calls `bringup_tone_fill` yet, so this bites only when that mode is added.
- **M2. Rev. 1 was wrong about the stock clock path.** `tdmout.c` uses private HAL calls (`i2s_ll_tx_set_mclk`, `i2s_ll_tx_set_bck_div_num(..., 1)`). In v5.5.1, `i2s_tdm_calculate_clock` bumps any master `bclk_div <= 2` to 3. With a 128-BCK frame the stock config therefore gives **MCLK = 18.432 MHz (384 x fS)** plus a warning, not 12.288 MHz at divider 2. Deleting the override changes MCLK and would break `pcm5122_status_is_running_48k_tdm`, which hard-codes `sck_ratio_code == 6` (256 x fS) as well as `bck_per_frame`. Still true: I could not confirm from the TRM that `bck_div = 1` is legal, and `tdmout_stop()` disables the channel with no ramp (S7); document or enforce "caller ramps to zero first".
- **M3.** `pcm5122_ctl.c`: `PCM5122_CLK_ERR_FLAGS 0x05` has no datasheet citation (S1); I could not check what the bits mean. `bck_per_frame == 256` is hard-coded.
- **M4. (low, fails closed)** The config path uses integer `delay_samples` (schema max 960) and `dsp_delay_init` rejects values above capacity. The residual issue is `dsp_delay_us_to_samples`: no clamp to `DSP_MAX_DELAY_SAMPLES` (960) and the `(uint32_t)` cast of a large double is undefined behaviour. `dsp_mix_process` has no `n_inputs` bound.
- **M5.** Doc drift after the chain-v1 pivot: graph/node references remain in `design.md` (4), `api.md` (8), `architecture.md` (2), `requirements.md` (1) (counts from rev. 1, not re-counted); `configs/legacy/` holds 18 files; AGENTS read order still says "then docs/PLAN.md (what to do)" although PLAN.md is marked RETIRED; AGENTS R5/C8 cite retired task T09.
- **M6.** C8: stub files `audiosrc.c`, `pcm5122.c` carry `TODO:` with no task id; `xoweb.c` has a TODO with no task id (confirmed). README says "Quick start: idf.py build" but no production path exists yet (no DSP task, protection stage, C JSON loader). S2/S5/S7 are enforced only in Python tooling today. That is fine at this stage but should be stated, and the Python and C validators must share the vectors in `tools/protection_oracle_vectors.json`.

## 2. Correction plan (one card = one branch = one PR, tests first, no push to main)

### Step 0. Freeze (today, no code)
1. Hold new work that depends on T27 or on 256-BCK framing (T24 playback loop, bring-up checklist steps P5.x, BOM purchases not yet made).
2. Add `Q6` to `docs/questions.md` as **blocking**: "ESP32-S3 I2S limits a TDM frame to 128 bit. How do we feed 3 x PCM5122 (6 ch, 32 bit)?"

### Step 1. 30-minute spike on any ESP32-S3 dev board (human, no DAC needed)
B1 is already confirmed from IDF source and docs, so the spike's main value is the Option A alignment check (item 3), not re-confirming B1.
1. Build a minimal app: TDM master, 8 slots x 32 bit, call `i2s_channel_init_tdm_mode`. Expect `ESP_ERR_INVALID_ARG` and the "exceeds the maximum 128" log line. If it does *not* fail on v5.5.6, B1 is withdrawn and Steps 2-3 shrink to docs only.
2. Same with 4 slots x 32 bit, mask 0x0F, MCLK multiple 256. Expect OK. Scope: BCLK 6.144 MHz, MCLK 12.288 MHz, WS one-BCK pulse every 128 BCK.
3. Option A only: with I2S1 as slave on the shared BCLK/WS, scope DOUT1 against DOUT0. Check that DAC2's data is not one or more BCLK late, and that the shared BCLK/WS pins are not reconfigured as input-only by the driver.
4. Record all results in `docs/bringup-log.md`.

### Step 2. Decide the feed architecture (human; board_config.h and ADRs are CODEOWNERS)
| Option | What | Verdict |
|---|---|---|
| A | I2S0 master, 4 x 32 bit (128 BCK) carries DAC0 + DAC1 (offsets 0 and 64) on DOUT0. I2S1 as TDM/stereo **slave** on the same BCLK/WS, own DOUT1 to DAC2 (offset 0). Enable I2S1 first, then I2S0, so both start on the same frame. | Candidate. Keeps 32 bit, no new parts. Costs one GPIO and one jumper (BCLK/WS looped to I2S1 inputs). **Risk is documented, not hypothetical:** the IDF guide says a TDM slave needs `bclk_div` >= 8 and its data can lag by more than one BCLK, so "DAC2 one frame late" is expected to matter. The shared-pin configuration is unproven. |
| B | 8 x 16 bit = 128 BCK, 6 ch at 16 bit | Reject for the product (96 dB floor). Acceptable for the first tone test only. |
| C | Swap to an 8-channel TDM DAC | Hardware change, only if A fails the alignment check. |

Write the choice as **ADR 0015** (status Proposed, then maintainer approval). The choice between A and B depends on DAC wiring and required audio quality; it is left to the maintainer.

### Step 3. Re-baseline hardware docs and constants (docs plus `board_config.h`, human-reviewed)
1. `board_config.h`: `XOVER_TDM_SLOTS_PER_FRAME 4`, frame = 128 BCK, new `XOVER_PIN_I2S_DOUT2`, DAC2 offset 0 on lane B, mask `0x0F`; keep the pin-collision asserts.
2. **Rewrite the existing DAC2 offset assertion.** `board_config.h` currently requires the DAC2 offset to equal `4 * SLOT_BITS` (= 128). In a 4-slot, 128-BCK frame that is the whole frame length, so the assertion must become per-lane (offset < frame length within its own lane).
3. Update `hw-pinmap.md` (s3, s4), `hw-verification-notes.md` (s1 row "8 slots ... is right" is wrong), `prebringup.md`, `bom.md` (jumper wire), `design.md`, ADR 0009 if it quotes block sizes.

### Step 4. Add the target build to CI (card T28, ~60 lines)
1. New job using the `espressif/idf:v5.5.6` container: `idf.py set-target esp32s3 && idf.py build` for each bring-up mode (NONE, PINWALK, PINWALK + PINWALK_MUTE, I2CSCAN), `-Werror`.
2. In `tdmout.c` add `_Static_assert(XOVER_TDM_SLOT_BITS * XOVER_TDM_SLOTS_PER_FRAME <= I2S_LL_SLOT_FRAME_BIT_MAX, ...)`. Do this **before** Step 5 so the old code fails red, then goes green.

### Step 5. Rework `tdmout` and packing (card T29, <= 400 lines) — REVISED
**Blocked on one decision: MCLK (Step 5.0).** Tests first, then code.

0. **Choose the MCLK strategy before touching the clock code.** The stock IDF path does not give 256 x fS here (see M2). Options:
   - (a) Accept 384 x fS = 18.432 MHz. Requires confirming 384 x fS is an allowed PCM5122 SCK ratio (datasheet check, UNVERIFIED), and accepting the IDF warning.
   - (b) Set `mclk_multiple` to 512 = 24.576 MHz. Needs the same datasheet check for 512 x fS and the GPIO/clock-tree limits.
   - (c) Keep a divider-2 override (current behaviour), documented and CI-built. Needs the TRM answer on `bck_div` legality, which is unconfirmed.
   Record the choice in ADR 0015 (or a sibling ADR).
1. `tdm_pack_8slot` -> `tdm_pack_4slot` (4 words per frame); one packer call per lane. Update `test_tdm_pack.c`, keep the NULL/size checks.
2. `tdmout`: implement the Step 0 choice. Do **not** simply delete the `i2s_ll_*` override and `TDMOUT_BCK_DIV` and fall back to `I2S_TDM_CLK_DEFAULT_CONFIG` unless option (a) was chosen, because that changes MCLK to 384 x fS. Add the second lane per Step 2; start order slave then master; add a "ramped-to-zero" precondition for stop (S7).
3. `pcm5122_status_is_running_48k_tdm`: derive **both** expected values from the chosen config. `bck_per_frame` from `XOVER_TDM_SLOT_BITS * XOVER_TDM_SLOTS_PER_FRAME` (not 256), and `sck_ratio_code` from the chosen MCLK multiple (it is hard-coded to 6 = 256 x fS today). Update `test_pcm5122_ctl.c` with a case for each.
4. Everything stays UNVERIFIED until the human enters the scope result in `bringup-log.md`.

### Step 6. Planning-document integrity (docs only, one PR)
1. Replace `task_state.json` with real JSON (tasks, states, blocking gates, `basis_commit`); regenerate `repository_files.json`.
2. `tools/check.py`: fail if either file is not valid JSON, if `basis_commit` is not an ancestor of HEAD, and if any `host/tests/test_*.c` is neither in `host/CMakeLists.txt` nor listed in an allowlist with a task id.
3. H3: move the four orphan tests to `docs/engineering/fixtures/` (or register them behind `if(EXISTS <header>)`) until their cards (state, storage, bounds, auth) are implemented.
4. M5: remove graph/node text from `design.md`, `api.md`, `architecture.md`, `requirements.md` or mark it historical; fix the AGENTS read order (task_state.json and `03_task_catalog.md` instead of PLAN.md); drop the T09 references; give each stub `TODO` a task id (including `xoweb.c`).

### Step 7. Bring-up safety fixes (card T30, ~80 lines)
1. PINWALK (Medium): never drive `DAC_MUTE_N` high; if `PINWALK_MUTE` stays, walk it as "observe only", and always restore output-low (no `gpio_set_direction(..., INPUT)` for that pin). Use `GPIO_MODE_OUTPUT_OD` for SDA/SCL. Log `XOVER_PIN_ADMIN_BUTTON`, not a literal.
2. Bring-up tone (latent, do when the tone mode lands): cap = `CONFIG_XOVER_BRINGUP_MAX_DBFS` (min with -30), passed to `bringup_tone_fill`; test that a lower Kconfig value lowers the cap.
3. M3: cite or remove the `0x05` mask (Q-entry if the datasheet is unclear).

### Step 8. DSP precision (card T31; the filter-expansion card is blocked on Q2, so resolve H6 first) — REVISED
1. **Decide Q2** (maintainer). Recommendation: schema `gain_db` for `peq`/`lshelf`/`hshelf` = -24..+12, matching the C and the conservative protection stance (boosts are what push drivers). Because the schema `filter` definition is shared with protection-weighting filters, scope the change per filter type (or via the EQ cards' own definitions) rather than editing the shared bound. Update `schema.json`, the oracle, and add `configs/invalid/` cases at +12.1 dB.
2. Tests first: extend `test_f32.c` to PEQ, shelves, all-pass, notch at 20, 30, 40, 60, 100, 200 Hz, several Q, and compare executed float32 output against the double response (gate: 0.01 dB above 20 Hz, so the 20 Hz row is judged at 20 Hz and up, not 10 Hz).
3. **Decide the H5 approach (maintainer; ADR required either way, C5):**
   - (a) **Hi/lo on numerator and denominator** for PEQ/shelf/AP/notch. `dsp_sos_t` (public header) gains numerator low-parts, the process loop changes, and the measured error drops to ~0 in the cases above. Denominator-only hi/lo is **not** acceptable: it worsens the error (table in H5).
   - (b) Accept the documented limit (~0.089 dB at 20 Hz in the gated band, 0.172 dB worst case at 30 Hz Q5 -6 dB in the table) by loosening G2 for EQ blocks. V3 says do not loosen the gate, so this needs an explicit ADR 0005 addendum and maintainer sign-off.
4. Make `dsp_check_stable` evaluate in double on `a + da`.
5. Record the measured numbers in an ADR 0005 addendum.
6. M4: clamp `dsp_delay_us_to_samples` to 960 and range-check before the cast; bound `n_inputs` in `dsp_mix_process`.

### Step 9. Close out
1. Re-run: `tools/check.py`, host build + ctest, ASan/UBSan, SciPy compare, oracle, the new `idf.py build` job. Paste outputs into each PR under "Evidence" (C6).
2. Update `docs/engineering/task_state.json`; only then resume the blocked Step-4 items from PLAN (numerical conventions, memory/interface freeze, ADR approvals, sourced driver and amplifier data).
3. Human bench order for the first tone test: continuity and mute-polarity check, I2C scan (0x4C/0x4D/0x4E), BCLK/MCLK/WS on scope, DAC2 alignment against DAC0, then six tones at <= -30 dBFS with amps disconnected.

## 3. Suggested order and size

| Order | Step | Who | Blocks |
|---:|---|---|---|
| 1 | 0, 1 | human | everything hardware-related |
| 2 | 2 | human decision, agent drafts ADR | 3, 5 |
| 3 | 3, 4 | agent + human review (CODEOWNERS) | 5 |
| 4 | 6 | agent | nothing, cheap, do in parallel |
| 5 | 5 (after MCLK choice), 7 | agent | bring-up |
| 6 | 8 | agent after Q2 and the H5 decision | filter expansion |
| 7 | 9 | both | resuming the plan |

## 4. Open decisions (maintainer)

| ID | Decision | Depends on |
|---|---|---|
| D1 | B1 fix: Option A (two 4 x 32-bit lanes) or Option B (16-bit slots) | DAC wiring, audio quality needed, Step 1 item 3 result |
| D2 | MCLK: 384 x fS, 512 x fS, or divider-2 override | PCM5122 datasheet SCK table (not yet read), TRM on `bck_div` |
| D3 | H5: hi/lo on both numerator and denominator, or accept the documented limit via ADR | cost of a public-header change vs G2 |
| D4 | Q2: gain bound, and whether it is scoped per filter type | shared `filter` schema definition |

## 5. Changes from rev. 1

- B1: added `i2s_ll.h` / `i2s_tdm_set_slot` detail; spike retargeted at the Option A alignment check.
- H1: `AGENTS.md` does not mention `task_state.json` (`PLAN.md` does); `basis_commit` claim not re-checked.
- H2: removed "never been compiled for the target"; T26/T27 record local builds. The gap is CI coverage.
- H3: verification scoped to `main`; other branches not re-scanned.
- H4: severity HIGH -> MEDIUM (off by default).
- H5: 20 Hz row is 0.089 dB inside the gated band; noted as a known limit in ADR 0005; the denominator-only fix is wrong, both sides need hi/lo, with ADR.
- H6: noted as already tracked as Q2; shared schema definition widens the blast radius.
- M1: marked latent. M2: corrected stock MCLK (18.432 MHz, not 12.288 MHz) and the `sck_ratio_code` break. M4: marked low, fails closed. M6: `xoweb.c` TODO confirmed.
- Step 3: added the `4 * SLOT_BITS` offset assertion. Step 5: added the MCLK decision and `sck_ratio_code`. Step 8: replaced the fix. Option A: slave lag documented in the IDF guide.

## 6. What I did not verify
- ESP-IDF v5.5.6 specifically (read v5.5.1). The 128-bit frame limit is a hardware constant, so I expect it to hold.
- ESP32-S3 TRM wording on the 128-bit frame and on `bck_div` minimum (source read only).
- The PCM5122 datasheet, including whether 384 x fS or 512 x fS is an allowed SCK ratio, and any register meaning beyond what the repo cites.
- Behaviour of two I2S controllers sharing BCLK/WS on the S3 (Option A is a design proposal, not a proven configuration).
- Other branches (H3), `basis_commit` (H1), doc reference counts (M5).
- Wi-Fi/DSP timing budgets (G4), coverage gate G2, anything on hardware.
