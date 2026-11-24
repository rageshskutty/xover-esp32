# Review of main @ e16d25d and step-by-step correction plan

Date: 2026-11-24. Reviewer: Claude (agent). Scope: full clone of `main` (284 commits, T01-T27 merged).
Labels follow AGENTS V1: **HOST-VERIFIED** = I ran it; **SOURCE-VERIFIED** = I read the cited upstream source, nothing run on hardware; **UNVERIFIED** = not checked. Nothing here is HW-VERIFIED.

## 0. What I ran (HOST-VERIFIED)

| Check | Result |
|---|---|
| `python tools/check.py` | PASS (all `configs/invalid/*` rejected with the expected code) |
| host build `-Wall -Wextra -Wconversion -Werror`, `ctest` | 13/13 pass |
| same with ASan + UBSan | 13/13 pass |
| `tools/test_scipy_compare.py` | PASS, worst case 2.9e-9 dB / 2.9e-10 deg vs SciPy |
| `tools/test_protection_oracle.py` | 26 tests OK |

Not run: coverage gate, `idf.py build` (no toolchain here), anything on hardware.

The DSP core (`dspblocks`), SPSC ring, S7 ramp helper and PCM5122 register table are in good shape. The problems are in the layer that has never been compiled for the target or run on hardware, plus planning-document integrity.

## 1. Findings, by severity

### BLOCKER

**B1. The 8 x 32-bit TDM frame is not possible on ESP32-S3. `tdmout_init()` will fail at runtime.** (SOURCE-VERIFIED, ESP-IDF v5.5.1; the repo pins v5.5.6, re-check that tag)
- `components/hal/esp32s3/include/hal/i2s_ll.h:37`: `I2S_LL_SLOT_FRAME_BIT_MAX 128`.
- `esp_driver_i2s/i2s_tdm.c:119`: `i2s_tdm_set_slot` returns `ESP_ERR_INVALID_ARG` when `total_slot * slot_bits > 128`. Repo asks for 8 x 32 = 256.
- IDF programming guide, TDM section: on targets without `SOC_I2S_TDM_FULL_DATA_WIDTH` only 4 slots at 32 bit are supported. `soc_caps.h` for ESP32-S3 has no such define.
- Repo claims the opposite: `docs/hw-pinmap.md` s4, `docs/hw-verification-notes.md` s1 ("8 slots of 32 bit is right"), `board_config.h`, `tdmout.c` static asserts, `tdm_pack_8slot`, T24-T27, `pcm5122_status_is_running_48k_tdm` (`bck_per_frame == 256`).
- Why nothing caught it: CI has no target build, and the check is a runtime return code.
- Constraint behind it: PCM5122 needs 128 or 256 BCK per frame, 6 x 32 = 192 is illegal, and the S3 allows at most 128. So 6 channels at 32 bit cannot share one S3 TDM lane.
- Caveat: I read IDF source, not the S3 TRM. A 30-minute bench spike (Step 1) settles it.

### HIGH

**H1. `docs/engineering/task_state.json` contains the single word `PLACEHOLDER`.** AGENTS/PLAN name it as the gate authority ("superseded by ... task_state.json"). `repository_files.json` has `basis_commit` cd2ae0d (stale) and lists legacy paths. (HOST-VERIFIED)

**H2. CI never builds the firmware.** `ci.yml` has host jobs only, so `tdmout.c`, `main.c`, Kconfig modes, `-Werror` on target (AGENTS C3) are unchecked. A compile-time assert on `I2S_LL_SLOT_FRAME_BIT_MAX` would have caught B1.

**H3. Four orphan tests on main.** `test_config_slot.c`, `test_app_state_machine.c`, `test_request_bounds.c`, `test_token_auth.c` include `config_slot.h`, `app_state_machine.h`, `xoweb_bounds.h`, `xoweb_auth.h`. Those headers exist on no branch and the tests are not in `host/CMakeLists.txt`, so they never build. (HOST-VERIFIED via `git log --all`)

**H4. PINWALK breaks S2/S3 when `XOVER_BRINGUP_PINWALK_MUTE=y`** (`firmware/main/main.c`):
- it drives `DAC_MUTE_N` high (= unmute) for 3 s;
- line 92 then sets that pin to INPUT, leaving it floating on only the 470 ohm pull-down; it never goes back to output-low;
- the log text hard-codes `gpio0` while the pin comes from `board_config.h`;
- SDA/SCL are driven push-pull on an open-drain bus.

**H5. EQ blocks use plain float32 coefficients, below the repo's own gate G2** (`dspblocks.c`: `dsp_design_peq`, `_shelf`, `_allpass`, `_notch` set `da1 = da2 = 0`). Response error from float32 rounding alone, measured by me in NumPy against the double design (HOST-VERIFIED):

| filter | max error 10-200 Hz |
|---|---:|
| 30 Hz, Q 5, -6 dB | 0.172 dB |
| 20 Hz, Q 3, +6 dB | 0.106 dB |
| 40 Hz, Q 8, +6 dB | 0.055 dB |
| 60 Hz, Q 5, +6 dB | 0.021 dB |
| 100 Hz, Q 5, +6 dB | 0.005 dB |

G2 says float32 within 0.01 dB of double above 20 Hz. `test_f32.c` calls only `dsp_design_xo`, so the gate never saw these. The sub (room PEQ, subsonic HP) is exactly where it matters. Related: `dsp_check_stable` tests only the float hi part with float arithmetic; at 10 Hz the margin `1 + a1 + a2` is about 1.7e-6 against float eps 1.2e-7.

**H6. Q2 still open: schema allows `gain_db` -24..+24, C designers reject above +12** (`dsp_design_peq` line 93). The oracle follows the schema, the firmware follows the C. A config that validates can fail at build.

### MEDIUM

- **M1.** `BRINGUP_TONE_MAX_DBFS` is hard-coded -30 and not tied to `CONFIG_XOVER_BRINGUP_MAX_DBFS` (range -120..-30). Setting -60 in Kconfig does not lower the generator cap.
- **M2.** `tdmout.c` uses private HAL calls (`i2s_ll_tx_set_mclk`, `i2s_ll_tx_set_bck_div_num(…, 1)`). I could not confirm from the TRM that `bck_div = 1` is legal on the S3, and IDF's own TDM clock code avoids a divider of 2 or less. With a 128-BCK frame the stock IDF path yields BCLK 6.144 MHz, MCLK 12.288 MHz, divider 2, so the override is unnecessary. `tdmout_stop()` disables the channel with no ramp (S7); document or enforce "caller ramps to zero first".
- **M3.** `pcm5122_ctl.c`: `PCM5122_CLK_ERR_FLAGS 0x05` has no datasheet citation (S1); `bck_per_frame == 256` is hard-coded.
- **M4.** `dsp_delay_us_to_samples` does not clamp to `DSP_MAX_DELAY_SAMPLES` (960) and the `(uint32_t)` cast of a large double is undefined behaviour. `dsp_mix_process` has no `n_inputs` bound.
- **M5.** Doc drift after the chain-v1 pivot: graph/node references remain in `design.md` (4), `api.md` (8), `architecture.md` (2), `requirements.md` (1); `configs/legacy/` holds 18 files; AGENTS read order still says "then docs/PLAN.md (what to do)" although PLAN.md is marked RETIRED; AGENTS R5/C8 cite retired task T09.
- **M6.** C8: stub files `audiosrc.c`, `xoweb.c`, `pcm5122.c` carry `TODO:` with no task id. README says "Quick start: idf.py build" but no production path exists yet (no DSP task, protection stage, C JSON loader). S2/S5/S7 are enforced only in Python tooling today. That is fine at this stage but should be stated, and the Python and C validators must share the vectors in `tools/protection_oracle_vectors.json`.

## 2. Correction plan (one card = one branch = one PR, tests first, no push to main)

### Step 0. Freeze (today, no code)
1. Hold new work that depends on T27 or on 256-BCK framing (T24 playback loop, bring-up checklist steps P5.x, BOM purchases not yet made).
2. Add `Q6` to `docs/questions.md` as **blocking**: "ESP32-S3 I2S limits a TDM frame to 128 bit. How do we feed 3 x PCM5122 (6 ch, 32 bit)?"

### Step 1. 30-minute spike on any ESP32-S3 dev board (human, no DAC needed)
1. Build a minimal app: TDM master, 8 slots x 32 bit, call `i2s_channel_init_tdm_mode`. **Expect `ESP_ERR_INVALID_ARG`** and the "exceeds the maximum 128" log line.
2. Same with 4 slots x 32 bit, mask 0x0F, MCLK multiple 256. Expect OK. Scope: BCLK 6.144 MHz, MCLK 12.288 MHz, WS one-BCK pulse every 128 BCK.
3. Record both in `docs/bringup-log.md`. If step 1 does *not* fail on v5.5.6, B1 is withdrawn and Steps 2-3 shrink to docs only.

### Step 2. Decide the feed architecture (human; board_config.h and ADRs are CODEOWNERS)
| Option | What | Verdict |
|---|---|---|
| A | I2S0 master, 4 x 32 bit (128 BCK) carries DAC0 + DAC1 (offsets 0 and 64) on DOUT0. I2S1 as TDM/stereo **slave** on the same BCLK/WS, own DOUT1 to DAC2 (offset 0). Enable I2S1 first, then I2S0, so both start on the same frame. | Recommended. Keeps 32 bit, no new parts. Costs one GPIO and one jumper (BCLK/WS looped to I2S1 inputs). Needs a scope check that DAC2 is not one frame late. |
| B | 8 x 16 bit = 128 BCK, 6 ch at 16 bit | Reject for the product (96 dB floor). Acceptable for the first tone test only. |
| C | Swap to an 8-channel TDM DAC | Hardware change, only if A fails the alignment check. |

Write the choice as **ADR 0015** (status Proposed, then maintainer approval).

### Step 3. Re-baseline hardware docs and constants (docs plus `board_config.h`, human-reviewed)
1. `board_config.h`: `XOVER_TDM_SLOTS_PER_FRAME 4`, frame = 128 BCK, new `XOVER_PIN_I2S_DOUT2`, DAC2 offset 0 on lane B, mask `0x0F`; keep the pin-collision asserts.
2. Update `hw-pinmap.md` (s3, s4), `hw-verification-notes.md` (s1 row "8 slots ... is right" is wrong), `prebringup.md`, `bom.md` (jumper wire), `design.md`, ADR 0009 if it quotes block sizes.

### Step 4. Add the target build to CI (card T28, ~60 lines)
1. New job using the `espressif/idf:v5.5.6` container: `idf.py set-target esp32s3 && idf.py build` for each bring-up mode (NONE, PINWALK, PINWALK + PINWALK_MUTE, I2CSCAN), `-Werror`.
2. In `tdmout.c` add `_Static_assert(XOVER_TDM_SLOT_BITS * XOVER_TDM_SLOTS_PER_FRAME <= I2S_LL_SLOT_FRAME_BIT_MAX, ...)`. Do this **before** Step 5 so the old code fails red, then goes green.

### Step 5. Rework `tdmout` and packing (card T29, <= 400 lines)
Tests first, then code:
1. `tdm_pack_8slot` -> `tdm_pack_4slot` (4 words per frame); one packer call per lane. Update `test_tdm_pack.c`, keep the NULL/size checks.
2. `tdmout`: delete the `i2s_ll_*` override and `TDMOUT_BCK_DIV`; use stock `I2S_TDM_CLK_DEFAULT_CONFIG` (mclk multiple 256); add the second lane per Step 2; start order slave then master; add a "ramped-to-zero" precondition for stop (S7).
3. `pcm5122_status_is_running_48k_tdm`: compare against `XOVER_TDM_SLOT_BITS * XOVER_TDM_SLOTS_PER_FRAME`, not 256. Update `test_pcm5122_ctl.c`.
4. Everything stays UNVERIFIED until the human enters the scope result in `bringup-log.md`.

### Step 6. Planning-document integrity (docs only, one PR)
1. Replace `task_state.json` with real JSON (tasks, states, blocking gates, `basis_commit`); regenerate `repository_files.json`.
2. `tools/check.py`: fail if either file is not valid JSON, if `basis_commit` is not an ancestor of HEAD, and if any `host/tests/test_*.c` is neither in `host/CMakeLists.txt` nor listed in an allowlist with a task id.
3. H3: move the four orphan tests to `docs/engineering/fixtures/` (or register them behind `if(EXISTS <header>)`) until their cards (state, storage, bounds, auth) are implemented.
4. M5: remove graph/node text from `design.md`, `api.md`, `architecture.md`, `requirements.md` or mark it historical; fix the AGENTS read order (task_state.json and `03_task_catalog.md` instead of PLAN.md); drop the T09 references; give each stub `TODO` a task id.

### Step 7. Bring-up safety fixes (card T30, ~80 lines)
1. PINWALK: never drive `DAC_MUTE_N` high; if `PINWALK_MUTE` stays, walk it as "observe only", and always restore output-low (no `gpio_set_direction(..., INPUT)` for that pin). Use `GPIO_MODE_OUTPUT_OD` for SDA/SCL. Log `XOVER_PIN_ADMIN_BUTTON`, not a literal.
2. Bring-up tone: cap = `CONFIG_XOVER_BRINGUP_MAX_DBFS` (min with -30), passed to `bringup_tone_fill`; test that a lower Kconfig value lowers the cap.
3. M3: cite or remove the `0x05` mask (Q-entry if the datasheet is unclear).

### Step 8. DSP precision (card T31; the filter-expansion card is blocked on Q2, so resolve H6 first)
1. **Decide Q2** (maintainer). Recommendation: schema `gain_db` for `peq`/`lshelf`/`hshelf` = -24..+12, matching the C and the conservative protection stance (boosts are what push drivers). Update `schema.json`, the oracle, and add `configs/invalid/` cases at +12.1 dB.
2. Tests first: extend `test_f32.c` to PEQ, shelves, all-pass, notch at 20, 30, 40, 60, 100, 200 Hz, several Q, and compare executed float32 output against the double response (gate: 0.01 dB above 20 Hz).
3. Fix: route PEQ/shelf/AP/notch through `set_den` (hi/lo) like the crossovers; make `dsp_check_stable` evaluate in double on `a + da`. Record the result in an ADR 0005 addendum with the measured numbers (V3: do not loosen the gate).
4. M4: clamp `dsp_delay_us_to_samples` to 960 and range-check before the cast; bound `n_inputs` in `dsp_mix_process`.

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
| 5 | 5, 7 | agent | bring-up |
| 6 | 8 | agent after Q2 | filter expansion |
| 7 | 9 | both | resuming the plan |

## 4. What I did not verify
- ESP32-S3 TRM wording on the 128-bit frame and on `bck_div` minimum (source read only).
- ESP-IDF v5.5.6 specifically (read v5.5.1).
- Any PCM5122 register meaning beyond what the repo cites.
- Behaviour of two I2S controllers sharing BCLK/WS on the S3 (Option A is a design proposal, not a proven configuration).
- Wi-Fi/DSP timing budgets (G4), coverage gate G2.
