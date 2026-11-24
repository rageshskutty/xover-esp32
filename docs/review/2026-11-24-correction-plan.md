# Review of main @ e16d25d and step-by-step correction plan (rev. 3)

Date: 2026-11-24. Reviewer: Claude (agent). Scope: full clone of `main` (284 commits, T01-T27 merged).
Rev. 2: second pass that re-checked every finding against the code on `main` (8a01f53), ESP-IDF v5.5.1 source and docs, and a numeric re-run of the EQ error figures.
Rev. 3: maintainer decisions D1-D4 recorded (2026-11-24); see section 4. Changes from earlier revisions are listed in section 5.
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
| Q2 branch `fix/q2-filter-gain-cap` (rev. 3) | `check.py` PASS incl. new fixture, oracle tests 27 OK; host C build not re-run (no C change) |

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
- **Decision D1 (2026-11-24): Option A, two 4 x 32-bit lanes.** Recorded in ADR 0015 (Proposed).

### HIGH

**H1. `docs/engineering/task_state.json` contains the single word `PLACEHOLDER`.** (HOST-VERIFIED) `PLAN.md` names it as the gate authority ("superseded by ... task_state.json"). `AGENTS.md` does not mention it (rev. 1 said AGENTS did). `repository_files.json` has `basis_commit` cd2ae0d (stale, rev. 1 claim, not re-checked in rev. 2) and lists legacy paths.

**H2. CI never builds the firmware.** `ci.yml` has host jobs only, so `tdmout.c`, `main.c`, Kconfig modes and `-Werror` on target (AGENTS C3) are unchecked in CI. Correction to rev. 1: the target layer has been compiled; the T26 and T27 PR texts record local `idf.py build` successes. The gap is CI coverage, not that nobody compiled it. A compile-time assert on `I2S_LL_SLOT_FRAME_BIT_MAX` would have caught B1.

**H3. Four orphan tests on main.** `test_config_slot.c`, `test_app_state_machine.c`, `test_request_bounds.c`, `test_token_auth.c` include `config_slot.h`, `app_state_machine.h`, `xoweb_bounds.h`, `xoweb_auth.h`. The tests are in `host/tests/` and not in `host/CMakeLists.txt`, so they never build. The headers are not in `firmware/main` or `xoweb/include`. (HOST-VERIFIED on `main`; the "exist on no branch" claim from rev. 1 was not re-checked across other branches.)

**H4. PINWALK breaks S2/S3 when `XOVER_BRINGUP_PINWALK_MUTE=y`** (`firmware/main/main.c`). Severity lowered from HIGH to **MEDIUM**: the option is off by default.
- it drives `DAC_MUTE_N` high (= unmute) for 3 s;
- it then sets that pin to INPUT, leaving it floating on only the 470 ohm pull-down; it never goes back to output-low;
- the log text hard-codes `gpio0` while the pin comes from `board_config.h`;
- SDA/SCL are driven push-pull on an open-drain bus.

**H5. EQ blocks use plain float32 coefficients, below the repo's own gate G2** (`dspblocks.c`: `dsp_design_peq`, `_shelf`, `_allpass`, `_notch` set `da1 = da2 = 0`). Not new: ADR 0005 already lists this as a known limit that also needs the numerators handled (its measured worst case over Q 1..10 and gain -6/+6/+12 dB is 0.254 dB at 30 Hz). Response error from float32 rounding alone, against the double design (HOST-VERIFIED, NumPy):

| filter | max error 10-200 Hz |
|---|---:|
| 30 Hz, Q 5, -6 dB | 0.172 dB |
| 20 Hz, Q 3, +6 dB | 0.106 dB (0.089 dB inside the gated band, 20 Hz and up) |
| 40 Hz, Q 8, +6 dB | 0.055 dB |
| 60 Hz, Q 5, +6 dB | 0.021 dB |
| 100 Hz, Q 5, +6 dB | 0.005 dB |

G2 says float32 within 0.01 dB of double above 20 Hz. `test_f32.c` calls only `dsp_design_xo`, so the gate never saw these. Related: `dsp_check_stable` tests only the float hi part with float arithmetic; at 10 Hz the margin `1 + a1 + a2` is about 1.7e-6 against float eps 1.2e-7.

Rev. 1's fix (hi/lo on the denominator only) does not work. Errors in numerator and denominator partly cancel, so fixing one side exposes the other:

| Case | All float32 | Denominator hi/lo only | Numerator and denominator hi/lo |
|---|---:|---:|---:|
| 30 Hz, Q5, -6 dB | 0.172 dB | 0.202 dB | ~0 |
| 30 Hz, Q5, +6 dB | 0.056 dB | 0.115 dB | ~0 |
| 100 Hz, Q5, +6 dB | 0.005 dB | 0.012 dB | ~0 |

Both sides need hi/lo, which grows `dsp_sos_t` (public header) and needs an ADR (C5).
**Decision D3 (2026-11-24): accept the limit for the first release, ADR 0016 (Proposed).** Accepted bound: at most 0.26 dB for centre frequency 30 Hz and above, Q up to 10, boost up to +12 dB. Not measured and outside the decision: 20-30 Hz, Q 10-20, float32 state rounding noise. Revisit before the next release.

**H6. Q2: schema allowed `gain_db` -24..+24, C designers reject above +12** (`dsp_design_peq` line 93, `dsp_design_shelf` line 106; they accept -24). The oracle followed the schema, the firmware follows the C. A config that validated could fail at build.
**Decision D4 (2026-11-24): boost capped at +12 dB, cut stays at -24 dB, one bound for every `filter` entry.** The schema's `filter` definition is shared by the pre chains, output chains and protection weighting, and there is one C implementation, so a second, scoped bound would recreate the same mismatch for weighting filters once the C loader exists. Weighting shapes that need more than +12 dB can be expressed with cuts plus a threshold shift. Implemented in branch `fix/q2-filter-gain-cap`: `configs/schema.json` (`gain_db` max 12), `tools/protection_oracle.py` (`FILTER_GAIN_DB`), `tools/test_protection_oracle.py` (+12.1 rejected, +12 and -24 accepted), new `configs/invalid/filter_boost_above_cap.json`, `docs/questions.md` Q2 answered. The C code needs no change. Open point: a per-filter cap does not bound stacked boosts (three +12 dB filters at one frequency make +36 dB); not verified that the protection stage's total-gain handling covers this.

### MEDIUM

- **M1. (latent)** `BRINGUP_TONE_MAX_DBFS` is hard-coded -30 and not tied to `CONFIG_XOVER_BRINGUP_MAX_DBFS` (range -120..-30). `main.c` has no tone mode and nothing calls `bringup_tone_fill` yet, so this bites only when that mode is added.
- **M2. Rev. 1 was wrong about the stock clock path.** `tdmout.c` uses private HAL calls (`i2s_ll_tx_set_mclk`, `i2s_ll_tx_set_bck_div_num(..., 1)`). In v5.5.1, `i2s_tdm_calculate_clock` bumps any master `bclk_div <= 2` to 3. With a 128-BCK frame the stock config therefore gives **MCLK = 18.432 MHz (384 x fS)** plus a warning, not 12.288 MHz at divider 2. Deleting the override with the default multiple changes MCLK and would break `pcm5122_status_is_running_48k_tdm`, which hard-codes `sck_ratio_code == 6` (256 x fS) as well as `bck_per_frame`. Still true: I could not confirm from the TRM that `bck_div = 1` is legal, and `tdmout_stop()` disables the channel with no ramp (S7); document or enforce "caller ramps to zero first".
- **M3.** `pcm5122_ctl.c`: `PCM5122_CLK_ERR_FLAGS 0x05` has no datasheet citation (S1); I could not check what the bits mean. `bck_per_frame == 256` is hard-coded.
- **M4. (low, fails closed)** The config path uses integer `delay_samples` (schema max 960) and `dsp_delay_init` rejects values above capacity. The residual issue is `dsp_delay_us_to_samples`: no clamp to `DSP_MAX_DELAY_SAMPLES` (960) and the `(uint32_t)` cast of a large double is undefined behaviour. `dsp_mix_process` has no `n_inputs` bound.
- **M5.** Doc drift after the chain-v1 pivot: graph/node references remain in `design.md` (4), `api.md` (8), `architecture.md` (2), `requirements.md` (1) (counts from rev. 1, not re-counted); `configs/legacy/` holds 18 files; AGENTS read order still says "then docs/PLAN.md (what to do)" although PLAN.md is marked RETIRED; AGENTS R5/C8 cite retired task T09.
- **M6.** C8: stub files `audiosrc.c`, `pcm5122.c` carry `TODO:` with no task id; `xoweb.c` has a TODO with no task id (confirmed). README says "Quick start: idf.py build" but no production path exists yet (no DSP task, protection stage, C JSON loader). S2/S5/S7 are enforced only in Python tooling today. That is fine at this stage but should be stated, and the Python and C validators must share the vectors in `tools/protection_oracle_vectors.json`.

## 2. Correction plan (one card = one branch = one PR, tests first, no push to main)

### Step 0. Freeze (today, no code)
1. Hold new work that depends on T27 or on 256-BCK framing (T24 playback loop, bring-up checklist steps P5.x, BOM purchases not yet made).
2. Add `Q6` to `docs/questions.md` as **blocking**: "ESP32-S3 I2S limits a TDM frame to 128 bit. How do we feed 3 x PCM5122 (6 ch, 32 bit)?" Answer: Option A, ADR 0015.

### Step 1. 30-minute spike on any ESP32-S3 dev board (human, no DAC needed)
B1 is already confirmed from IDF source and docs, so the spike's main value is the Option A alignment check (item 3), not re-confirming B1.
1. Build a minimal app: TDM master, 8 slots x 32 bit, call `i2s_channel_init_tdm_mode`. Expect `ESP_ERR_INVALID_ARG` and the "exceeds the maximum 128" log line. If it does *not* fail on v5.5.6, B1 is withdrawn and Steps 2-3 shrink to docs only.
2. Same with 4 slots x 32 bit, mask 0x0F, `mclk_multiple` 384. Expect OK. Scope: BCLK 6.144 MHz, MCLK 18.432 MHz, WS one-BCK pulse every 128 BCK. Check there is no `bclk_div` warning in the log.
3. Option A: with I2S1 as slave on the shared BCLK/WS, scope DOUT1 against DOUT0. Check that DAC2's data is not one or more BCLK late, and that the shared BCLK/WS pins are not reconfigured as input-only by the driver.
4. Record all results in `docs/bringup-log.md`.

### Step 2. Feed architecture: decided, Option A (ADR 0015)
| Option | What | Verdict |
|---|---|---|
| A | I2S0 master, 4 x 32 bit (128 BCK) carries DAC0 + DAC1 (offsets 0 and 64) on DOUT0. I2S1 as TDM/stereo **slave** on the same BCLK/WS, own DOUT1 to DAC2 (offset 0). Enable I2S1 first, then I2S0, so both start on the same frame. | **Chosen (D1).** Keeps 32 bit, no new parts. Costs one GPIO and one jumper (BCLK/WS looped to I2S1 inputs). Risk is documented, not hypothetical: the IDF guide says a TDM slave needs `bclk_div` >= 8 and its data can lag by more than one BCLK, so "DAC2 one frame late" is expected to matter. The shared-pin configuration is unproven; ADR 0015 stays Proposed until Step 1 item 3 passes. |
| B | 8 x 16 bit = 128 BCK, 6 ch at 16 bit | Reject for the product (96 dB floor). Acceptable for the first tone test only. |
| C | Swap to an 8-channel TDM DAC | Hardware change, the fallback if A fails the alignment check. |

### Step 3. Re-baseline hardware docs and constants (docs plus `board_config.h`, human-reviewed)
1. `board_config.h`: `XOVER_TDM_SLOTS_PER_FRAME 4`, frame = 128 BCK, new `XOVER_PIN_I2S_DOUT2`, DAC2 offset 0 on lane B, mask `0x0F`; keep the pin-collision asserts.
2. **Rewrite the existing DAC2 offset assertion.** `board_config.h` currently requires the DAC2 offset to equal `4 * SLOT_BITS` (= 128). In a 4-slot, 128-BCK frame that is the whole frame length, so the assertion must become per-lane (offset < frame length within its own lane).
3. Update `hw-pinmap.md` (s3, s4), `hw-verification-notes.md` (s1 row "8 slots ... is right" is wrong), `prebringup.md`, `bom.md` (jumper wire), `design.md`, ADR 0009 if it quotes block sizes.

### Step 4. Add the target build to CI (card T28, ~60 lines)
1. New job using the `espressif/idf:v5.5.6` container: `idf.py set-target esp32s3 && idf.py build` for each bring-up mode (NONE, PINWALK, PINWALK + PINWALK_MUTE, I2CSCAN), `-Werror`.
2. In `tdmout.c` add `_Static_assert(XOVER_TDM_SLOT_BITS * XOVER_TDM_SLOTS_PER_FRAME <= I2S_LL_SLOT_FRAME_BIT_MAX, ...)`. Do this **before** Step 5 so the old code fails red, then goes green.

### Step 5. Rework `tdmout` and packing (card T29, <= 400 lines)
Tests first, then code.

0. **MCLK: decided, 384 x fS = 18.432 MHz (D2), pending one datasheet check.** `mclk_multiple = 384` gives master `bclk_div` 3 on the stock IDF path: no private HAL calls and no warning. Fallback: 512 x fS = 24.576 MHz (divider 4, same code, higher clock). Rejected: the divider-2 override (relies on `bck_div = 1`, unconfirmed in the TRM, fights IDF's clamp). **Before coding, read the PCM5122 SCK ratio table and confirm 384 x fS is allowed (UNVERIFIED today); if not, use 512 x fS.**
1. `tdm_pack_8slot` -> `tdm_pack_4slot` (4 words per frame); one packer call per lane. Update `test_tdm_pack.c`, keep the NULL/size checks.
2. `tdmout`: delete the `i2s_ll_*` override and `TDMOUT_BCK_DIV`; set `mclk_multiple` to 384; add the second lane per Step 2; start order slave then master; add a "ramped-to-zero" precondition for stop (S7).
3. `pcm5122_status_is_running_48k_tdm`: derive **both** expected values from the config. `bck_per_frame` from `XOVER_TDM_SLOT_BITS * XOVER_TDM_SLOTS_PER_FRAME` (not 256), and `sck_ratio_code` from the MCLK multiple (hard-coded to 6 = 256 x fS today). Update `test_pcm5122_ctl.c` with a case for each.
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

### Step 8. DSP precision and filter bounds (card T31) — REVISED (rev. 3)
1. **Q2 / D4: done in branch `fix/q2-filter-gain-cap`** (schema, oracle, test, invalid fixture, `questions.md`). Remaining: confirm the protection stage's total-gain handling covers stacked boosts (open point under H6).
2. Tests first: extend `test_f32.c` to PEQ, shelves, all-pass, notch at 30, 40, 60, 100, 200 Hz and several Q up to 10, and lock the measured coefficient-only response error with a 0.26 dB limit (D3, ADR 0016). The crossover tests keep the 0.01 dB gate.
3. **D3: the EQ float32 limit is accepted for release 1 (ADR 0016)**; `dsp_sos_t` does not change in this release. Make `dsp_check_stable` evaluate in double on `a + da`.
4. Before the next release: measure Q 10-20 and 20-30 Hz and the float32 state rounding noise, check WCET headroom once T09 lands, then decide whether to implement numerator and denominator hi/lo (public header change, new ADR, V3).
5. M4: clamp `dsp_delay_us_to_samples` to 960 and range-check before the cast; bound `n_inputs` in `dsp_mix_process`.

### Step 9. Close out
1. Re-run: `tools/check.py`, host build + ctest, ASan/UBSan, SciPy compare, oracle, the new `idf.py build` job. Paste outputs into each PR under "Evidence" (C6).
2. Update `docs/engineering/task_state.json`; only then resume the blocked Step-4 items from PLAN (numerical conventions, memory/interface freeze, ADR approvals, sourced driver and amplifier data).
3. Human bench order for the first tone test: continuity and mute-polarity check, I2C scan (0x4C/0x4D/0x4E), BCLK/MCLK/WS on scope, DAC2 alignment against DAC0, then six tones at <= -30 dBFS with amps disconnected.

## 3. Suggested order and size

| Order | Step | Who | Blocks |
|---:|---|---|---|
| 1 | 0, 1 | human | everything hardware-related |
| 2 | 2 (decided), ADR 0015 approval | human | 3, 5 |
| 3 | 3, 4 | agent + human review (CODEOWNERS) | 5 |
| 4 | 6, Q2 branch | agent | nothing, cheap, do in parallel |
| 5 | 5 (after the SCK table check), 7 | agent | bring-up |
| 6 | 8 | agent | filter expansion |
| 7 | 9 | both | resuming the plan |

## 4. Decisions (maintainer, 2026-11-24)

| ID | Decision | Outcome | Where recorded | Status |
|---|---|---|---|---|
| D1 | B1 fix | Option A, two 4 x 32-bit lanes | ADR 0015 | Decided; ADR Proposed until the Step 1 alignment check passes |
| D2 | MCLK | 384 x fS (18.432 MHz), fallback 512 x fS, no divider-2 override | ADR 0015, Step 5.0 | Decided; **PCM5122 SCK table check still open (UNVERIFIED)** |
| D3 | H5 EQ float32 | Accept the limit for release 1 (0.26 dB bound for f >= 30 Hz, Q <= 10, boost <= +12 dB) | ADR 0016 | Decided; revisit before the next release |
| D4 | Q2 gain bound | Boost +12 dB, cut -24 dB, one bound for all filter entries | `fix/q2-filter-gain-cap`, `questions.md` Q2 | Decided; implemented and host-tested in that branch |

## 5. Changes from earlier revisions

Rev. 3 (this revision):
- Recorded D1-D4 and where each is implemented; added ADR 0015 and ADR 0016 (both Proposed).
- H5: noted the ADR 0005 worst case (0.254 dB) and the accepted bound; H6: decided, implemented on a separate branch. Step 1 item 2 uses `mclk_multiple` 384 (BCLK 6.144 MHz, MCLK 18.432 MHz). Step 5 item 0 now states the decision; Step 8 replaced by the accepted-limit plan.

Rev. 2 (from rev. 1):
- B1: added `i2s_ll.h` / `i2s_tdm_set_slot` detail; spike retargeted at the Option A alignment check.
- H1: `AGENTS.md` does not mention `task_state.json` (`PLAN.md` does); `basis_commit` claim not re-checked.
- H2: removed "never been compiled for the target"; T26/T27 record local builds. The gap is CI coverage.
- H3: verification scoped to `main`; other branches not re-scanned. H4: severity HIGH -> MEDIUM (off by default).
- H5: 20 Hz row is 0.089 dB inside the gated band; known limit in ADR 0005; the denominator-only fix is wrong, both sides need hi/lo, with ADR.
- H6: already tracked as Q2; shared schema definition widens the blast radius.
- M1: latent. M2: corrected stock MCLK (18.432 MHz, not 12.288 MHz) and the `sck_ratio_code` break. M4: low, fails closed. M6: `xoweb.c` TODO confirmed.
- Step 3: added the `4 * SLOT_BITS` offset assertion. Step 5: added the MCLK decision and `sck_ratio_code`. Step 8: replaced the fix. Option A: slave lag documented in the IDF guide.

## 6. What I did not verify
- ESP-IDF v5.5.6 specifically (read v5.5.1). The 128-bit frame limit is a hardware constant, so I expect it to hold.
- ESP32-S3 TRM wording on the 128-bit frame and on `bck_div` minimum (source read only).
- The PCM5122 datasheet, including whether 384 x fS or 512 x fS is an allowed SCK ratio (needed before Step 5), and any register meaning beyond what the repo cites.
- Behaviour of two I2S controllers sharing BCLK/WS on the S3 (Option A is a design proposal, not a proven configuration).
- EQ float32 error below 30 Hz, above Q 10, and the state rounding noise (outside ADR 0016).
- That the protection stage bounds stacked EQ boosts.
- Other branches (H3), `basis_commit` (H1), doc reference counts (M5).
- The host C build and ctest on the Q2 branch (no C change, but not re-run; no cmake in my sandbox).
- Wi-Fi/DSP timing budgets (G4), coverage gate G2, anything on hardware.
