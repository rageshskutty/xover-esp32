# Task cards T34–T38: planning integrity and bring-up hygiene

**Authored:** 2026-11-25, against `main` @ 4593a15. **Authority:** AGENTS.md (C1, C3, C6, C8, S1, S6, V1–V4), `docs/review/2026-11-24-correction-plan.md` (Step 6, Step 7.2, findings H1, M1, M5, M6), `docs/engineering/04_execution_protocol.md`.

Same format and sizing rules as T28–T33 in `03_task_catalog.md` (kept in a separate file so that the 325-line catalog is not retyped; fold into `03_task_catalog.md` when convenient). Every card needs the "Required completion evidence" list from that catalog. Nothing here needs a hardware fact, a pin, a datasheet value or a driver number (S1). None of these cards unblocks or touches T29a–T29c, T33 or any human gate.

**Order and gates**

| Card | State | Waits for |
|---|---|---|
| T34 | open | nothing |
| T35 | open | T34 merged (the new rules must be green on `main` the moment they land) |
| T36 | open | nothing (parallel with T34) |
| T37 | open | T35 merged (both edit `tools/check.py`; serialise) |
| T38 | open | nothing (parallel with T34, T36) |

**Findings from the 2026-11-25 re-check that these cards are based on (HOST-VERIFIED unless noted)**

- The correction plan's H1 (placeholder `task_state.json`) and H3 (orphan tests) are already fixed on `main`: `task_state.json` is real JSON and `tools/check.py` enforces test ownership with `deferred_tests`. No card is needed for either.
- M5 (graph/node text in docs) is mostly done. What remains in `design.md`, `api.md`, `architecture.md` and `requirements.md` are retirement notices and the live error code name `E_NODE_PARAM`; leave them. What is still stale: the AGENTS.md read order and workflow (T36).
- `task_state.json` is behind git: T30 (PR #58) and T30b (PR #59) are merged but marked `open`. Its `basis_commit` equivalent does not exist; `repository_files.json` has `basis_commit` cd2ae0d, 107 paths, not checked by anything.
- **Task IDs T24, T25, T26 are mislabelled in `task_state.json`** (all three carry `inferred: true`). Git history says: T24 = bring-up tone generator (PR #38), T25 = PCM5122 control and status decode (PR #39), T26 = bring-up app pin walk and I2C scan (PR #40). The file has T24 = control sequence, T26 = tone generator, and no T25. Consequence: `test_pcm5122_ctl` is owned by the wrong ID, and `03_task_catalog.md` repeats it ("`test_pcm5122_ctl` stay with ... T24").
- Six `TODO` comments in `firmware/` have no task id (C8): `xoweb.c`, `xoweb.h`, `audiosrc.c`, `audiosrc.h`, `pcm5122.c`, `pcm5122.h`. The audiosrc one is also stale: the SPSC ring exists in `audiosrc_ring.c`. `tools/check.py` has no C8 rule.
- `BRINGUP_TONE_MAX_DBFS` is still a hard-coded -30 and not tied to the Kconfig cap (M1). No caller exists yet.

---

## T34 — Planning state sync and basis commit

**State:** open. No dependency. JSON and one sentence of Markdown; no code.

**Objective:** `task_state.json` matches git history, and both planning files carry the same verifiable `basis_commit`.

**Files (exact, nothing else):**
- `docs/engineering/task_state.json`
- `docs/engineering/repository_files.json`
- `docs/engineering/03_task_catalog.md` (one sentence, see change 3)

**Production size:** about 25 changed lines of JSON, 1 line of Markdown, plus the regenerated path list (generated data, excluded from C1).

**Changes:**
1. In `task_state.json`:
   - T30 `state` becomes `merged`. T30b `state` becomes `merged`. Both stay `inferred: false`.
   - T24: `title` "Bring-up tone generator", `tests` `["test_bringup_tone"]`, `inferred` false.
   - T25 (new entry): `title` "PCM5122 control and status decode", `state` `merged`, `tests` `["test_pcm5122_ctl"]`, `inferred` false.
   - T26: `title` "Bring-up app pin walk and I2C scan", `state` `merged`, `tests` `[]`, `inferred` false.
   - Add the five cards of this file (T34–T38) as `open` with `tests` `[]`, `inferred` false. T34 itself is `merged` only after its PR merges, so it is `open` in the PR.
   - Set `updated` to the date of the change. Add top-level `"basis_commit"`: the 40-hex SHA of the `main` commit the branch is based on (`git rev-parse origin/main`).
   - Every test name must appear under exactly one task (the existing `check.py` rule enforces this): `test_pcm5122_ctl` moves from T24 to T25; `test_bringup_tone` moves from T26 to T24.
2. `repository_files.json`: set `basis_commit` to the same SHA and replace `paths` with the output of
   ```
   git ls-tree -r --full-tree <basis_commit> | awk '$2=="blob"{sub(/^[^\t]*\t/,""); print}' | sort
   ```
   (blobs only, so the `firmware/third_party/cjson` gitlink is excluded; this reproduces the current 107-path list when run at cd2ae0d). Write it with `json.dump(..., indent=2)`, one path per line.
3. `03_task_catalog.md`, section "Order and gates", paragraph "Test ownership": the sentence that says `test_pcm5122_ctl` "stay with T27 and T24" becomes "stay with T27 and T25". Change nothing else in the file.

**Accepted:** none beyond the contract above; this card is data.

**Expected baseline:** none by test. First-fail evidence is a `git log`/`git show` excerpt for each of PR #38, #39, #40, #58, #59 that justifies each changed state and title (C6).

**Commands:**
```
python tools/check.py
python -c "import json;json.load(open('docs/engineering/task_state.json'));json.load(open('docs/engineering/repository_files.json'))"
git ls-tree -r --full-tree $(python -c "import json;print(json.load(open('docs/engineering/task_state.json'))['basis_commit'])") | grep -c ' blob '
```
The last count must equal `len(paths)` in `repository_files.json`.

**Acceptance:**
1. `python tools/check.py` PASS (this includes the existing test-ownership rule).
2. Both files parse; `basis_commit` is identical in both and equals `git rev-parse origin/main` at branch time.
3. The path count equals the blob count at `basis_commit`.
4. T30 and T30b are `merged`; T24, T25, T26 match the git subjects above; no other task changed.
5. Diff touches only the three files above.

**Rules (5):** C1, C6, C7, V1, V4.

**Stop:**
- A PR subject in git history contradicts a title in this card. Do not choose; write it to `docs/questions.md` and stop that entry.
- Any other task state looks wrong. Report it in the PR; do not change it in this card.

---

## T35 — `check.py` planning-integrity rules

**State:** open. Waits for T34 merged.

**Objective:** `tools/check.py` fails when the planning files drift from git.

**Files (exact, nothing else):**
- `tools/check.py` (CODEOWNERS path)
- `.github/workflows/ci.yml` (CODEOWNERS path): `fetch-depth: 0` on the `guardrails` job checkout only

**Production size:** about 45 lines of Python plus a selftest of about 20 lines, 2 lines of YAML.

**Contract (new pure function next to `planning_errors`, git access injected so the selftest needs no repository):**
```python
def basis_errors(task_state, repo_files, is_ancestor, tree_paths):
    """is_ancestor(sha) -> True | False | None (None = cannot tell); tree_paths(sha) -> set of blob paths."""
```
Rules, each returning one message string on failure:
- B1: `task_state["basis_commit"]` and `repo_files["basis_commit"]` exist and match `^[0-9a-f]{40}$`.
- B2: the two values are equal.
- B3: `is_ancestor(sha)` is True. False → "basis_commit is not an ancestor of HEAD". None → "cannot verify basis_commit; full history needed (git fetch --unshallow)". The rule never skips silently.
- B4: `set(repo_files["paths"]) == tree_paths(sha)`, and `repo_files["paths"]` has no duplicates. Report at most 5 differing paths in the message.
- B5: every task `state` is one of `open`, `blocked`, `merged`, `needs_rework`.

Git helpers use `subprocess.run([...], check=False)` with a list argv (no shell): `git merge-base --is-ancestor <sha> HEAD` (exit 0 → True, 1 → False, anything else → None) and `git ls-tree -r --full-tree <sha>` filtered to lines whose object type is `blob`. Wire the function into `check_planning()` after the existing `planning_errors` call. Add the selftest to `selftest_planning()` in the same style as the existing cases.

**Tests first (selftest cases, exact):**
1. Consistent inputs return `[]`.
2. `basis_commit` of 39 characters → B1 message.
3. Different SHAs in the two files → B2 message.
4. `is_ancestor` returning False → B3 "not an ancestor"; returning None → B3 "cannot verify".
5. `paths` missing one file that `tree_paths` has → B4 message naming the file; an extra path → B4 message; a duplicated path → B4 message.
6. A task with `state` `"done"` → B5 message.

**Expected baseline:** the selftest fails with `NameError: basis_errors` (or the new cases are reported missed) before the function exists.

**Commands:** `python tools/check.py`; `git fetch --unshallow` first if the clone is shallow.

**Acceptance:**
1. Selftest cases 1–6 pass; before implementation they fail (paste both outputs, C6).
2. `python tools/check.py` PASS on the T34-merged `main`, including the new line `ok basis_commit ...`.
3. In CI the `guardrails` job is green with `fetch-depth: 0`. The other jobs' checkout steps are unchanged.
4. Mutation check, pasted: change one char of `basis_commit` in a scratch copy and `check.py` fails with the B1/B3 message; delete one path from `repository_files.json` and it fails with B4. Revert both.
5. S6 note in the PR: these rules gate documents, not protection configs; no config path changes. The maintainer reviews because both files are CODEOWNERS paths (V4).

**Rules (5):** C1, C3, C6, S6, V3.

**Stop:**
- `ci.yml` has more than one job that runs `check.py`. Report it; change only the job named `guardrails`.
- Existing selftest cases break. Do not edit them; stop and report.
- The rule needs a network call. It must not (X5 spirit); stop.

---

## T36 — AGENTS.md read order and README status

**State:** open. No dependency. Docs only, human review (AGENTS.md is a CODEOWNERS path).

**Objective:** an agent that follows AGENTS.md is sent to the live task list, not the retired `PLAN.md`.

**Files (exact, nothing else):** `AGENTS.md`, `README.md`, `docs/questions.md`.

**Changes (exact):**
1. `AGENTS.md` line 3: "then docs/PLAN.md (what to do)" becomes "then docs/engineering/task_state.json and docs/engineering/03_task_catalog.md (what to do; docs/PLAN.md is retired history)". Keep the rest of the line.
2. `AGENTS.md` section 2, the AGENT line: "(docs/PLAN.md)" becomes "(docs/engineering/03_task_catalog.md and the files named in its card order)".
3. `AGENTS.md` section 5 step 1: "Pick the lowest-numbered unblocked task in docs/PLAN.md." becomes "Pick the lowest-numbered task with state `open` in docs/engineering/task_state.json; its card is in docs/engineering/03_task_catalog.md or a later `docs/engineering/0N_task_cards_*.md`." Keep "Do not start a task whose dependencies are not merged."
4. `README.md`: add a "Status" paragraph above "Build order": architecture is chain-per-output (ADR 0012, ADR 0014, both Proposed); the DSP block library and host tests are implemented; no chain executor, protection evaluator or JSON loader exists in firmware; `idf.py build` builds the bring-up modes only; outputs must not be connected to speakers (AGENTS S2, S3); live task state is `docs/engineering/task_state.json`. At most 6 lines. No new claims about hardware.
5. `docs/questions.md`: add row Q11 (below), blocking "no".

**Q11 text (verbatim):** `configs/legacy/` (1 example and 16 invalid graph-era fixtures) is not referenced by `tools/check.py`, `host/CMakeLists.txt` or CI (checked with grep on 2026-11-25), and `dspgraph` was deleted. `03_task_catalog.md` (Step 3) still says legacy fixtures "continue to feed the old C validator regression test". Keep the directory as history, or delete it (git history keeps it)? Also, AGENTS R5 and C8 cite task T09, which is retired. Reword them when the chain runtime contract exists?

**Out of scope:** deleting `configs/legacy/`, editing AGENTS R5/C8, rewriting `docs/design.md`, `docs/api.md` or the catalog. Those need the maintainer's answer to Q11.

**Expected baseline:** none by test. `grep -n 'PLAN.md' AGENTS.md` lists 3 lines before and 0 lines that point to PLAN.md as the task list after (a mention of "retired history" is allowed).

**Commands:** `python tools/check.py`; `grep -n 'PLAN.md' AGENTS.md README.md`.

**Acceptance:**
1. `python tools/check.py` PASS.
2. The grep output before and after is pasted (C6).
3. README status paragraph states nothing that is not in `task_state.json`, `03_task_catalog.md` or an ADR (list the source for each sentence in the PR).
4. Diff contains only the five changes above.

**Rules (5):** C1, C7, S1, V1, V4.

**Stop:** the maintainer wants different wording for any AGENTS.md line — AGENTS.md wins over this card.

---

## T37 — C8 rule and TODO task ids

**State:** open. Waits for T35 merged (same file: `tools/check.py`).

**Objective:** no `TODO` in `firmware/` without a task id, enforced by `check.py`.

**Files (exact, nothing else):**
- `tools/check.py` (CODEOWNERS path)
- `firmware/components/xoweb/xoweb.c`, `firmware/components/xoweb/include/xoweb.h` (CODEOWNERS path)
- `firmware/components/pcm5122/pcm5122.c`, `firmware/components/pcm5122/include/pcm5122.h` (CODEOWNERS path)
- `firmware/components/audiosrc/audiosrc.c`, `firmware/components/audiosrc/include/audiosrc.h`

**Production size:** about 15 lines of Python plus about 12 of selftest; 6 one-line comment edits.

**Rule (pure function):**
```python
TODO_RE = re.compile(r"\bTODO\b(?!\(T[0-9]+[a-z]?\))")
def todo_errors(rel_path, text): ...   # one message per offending line: "<path>:<line>: TODO without task id (C8)"
```
Scan `firmware/**/*.c` and `*.h`, excluding `firmware/third_party/` and any `managed_components/`. A valid form is `TODO(T11)`, `TODO(T10)`, `TODO(T08)`.

**Comment edits (exact new text; the task ids are catalog sections, not guesses):**
- `xoweb.c`: `/* TODO(T11): authenticated config upload and response endpoints. */`
- `xoweb.h`: `/* TODO(T11): start/stop local configuration web server. */`
- `audiosrc.c`: `/* TODO(T10): decoder hook that pushes PCM into the ring (audiosrc_ring.c). */`
- `audiosrc.h`: `/* TODO(T10): PCM input API on top of audiosrc_ring.h. */`
- `pcm5122.c`: `/* TODO(T08): bring-up integration of pcm5122_ctl.c, after hardware interface is confirmed. */`
- `pcm5122.h`: `/* TODO(T08): PCM5122 initialization and mute API, see pcm5122_ctl.h. */`

T10, T11 and T08 are the owning entries in `03_task_catalog.md` ("T10–T15" are retired broad cards to be reissued as micro-cards; "T08" is split into T23–T27). When those are reissued, the TODO ids are updated in the reissuing card.

**Tests first (selftest, exact):**
1. `"/* TODO(T09): x */"` → no error. 2. `"/* TODO: x */"` → one error with the line number. 3. `"// TODO x"` → one error. 4. `"int todo_count;"` → no error (case-sensitive, whole word). 5. `"TODO(T9)"` → no error; `"TODO(9)"` → one error; `"TODO(T10) and TODO later"` → one error.

**Expected baseline:** on `main`, `check.py` with the new rule reports exactly 6 lines (the six above); that is the first-fail evidence (C6). After the comment edits it reports 0.

**Commands:** `python tools/check.py`; `cmake -S host -B host/build -DCMAKE_BUILD_TYPE=Release && cmake --build host/build && ctest --test-dir host/build --output-on-failure`.

**Acceptance:**
1. Baseline output with 6 reports is pasted, then final output with 0.
2. `check.py` PASS; host suite 13/13 (unchanged).
3. S6: the three CODEOWNERS component edits are comment-only. The PR states this and asks the maintainer to waive the "rejecting test" part of S6 for comment-only changes (V4); the existing suite is unchanged and is pasted.
4. The target-build jobs (T28) stay as they were; comment edits cannot change the object code, but the Actions run URL is pasted (V2: no toolchain in the agent sandbox).

**Rules (5):** C1, C8, S6, V2, V4.

**Stop:** a `TODO` exists in a file outside the list above. Do not assign an id by guessing; list it in the PR and stop that item.

---

## T38 — Bring-up tone level cap parameter (M1)

**State:** open. No dependency. Host-testable, no ESP-IDF.

**Objective:** `bringup_tone_fill` takes the caller's cap and enforces the smaller of that cap and the hard -30 dBFS limit, so a lower `CONFIG_XOVER_BRINGUP_MAX_DBFS` can lower the cap (finding M1, correction plan Step 7.2, AGENTS S3).

**Files (exact, nothing else):** `firmware/main/bringup_tone.h`, `firmware/main/bringup_tone.c`, `host/tests/test_bringup_tone.c`. The header is internal to `main` (not a component public header), so C5 is not triggered.

**Production size:** about 8 lines.

**Signature (new; `cap_dbfs` inserted after `level_dbfs`):**
```c
int bringup_tone_fill(float level_dbfs, float cap_dbfs, uint32_t start_frame, size_t n_frames,
                      int32_t *const channels[BRINGUP_TONE_CHANNELS]);
```
**Behaviour:** `effective_cap = min(BRINGUP_TONE_MAX_DBFS, cap_dbfs)`. Return -1 and leave all buffers untouched when `level_dbfs` or `cap_dbfs` is not finite, `cap_dbfs < BRINGUP_TONE_MIN_DBFS`, `level_dbfs < BRINGUP_TONE_MIN_DBFS`, or `level_dbfs > effective_cap`. Everything after that check is unchanged (n_frames == 0 returns 0; NULL checks; sample generation).

**Tests first (exact; N = 48000 buffers already in the test file):**
1. All existing calls pass `BRINGUP_TONE_MAX_DBFS` as `cap_dbfs`; every existing assertion stays and keeps its value.
2. `cap_dbfs = -40.0f`: level -40.0f → 0; level -39.0f → -1 and the buffer is unchanged (memcmp against a pre-filled pattern).
3. `cap_dbfs = -20.0f` (above the hard limit): level -30.0f → 0; level -29.0f → -1 (the hard limit wins).
4. `cap_dbfs = -120.0f`: level -120.0f → 0.
5. `cap_dbfs = -121.0f` → -1; `cap_dbfs = NAN` → -1; `cap_dbfs = INFINITY` → -1.
6. Amplitude check: with `cap_dbfs = -40.0f` and level -40.0f, the measured 400 Hz amplitude (existing `tone_amp`) is within 1% of `10^(-40/20) * 2147483647`. This reuses the existing tolerance style; if the existing test uses a different tolerance, use that one.

**Expected baseline:** the test file fails to compile (too many arguments / unknown parameter) before the change.

**Out of scope:** reading `CONFIG_XOVER_BRINGUP_MAX_DBFS` anywhere, any tone mode in `main.c`, `Kconfig.projbuild`. No caller exists; the Kconfig wiring belongs to the card that adds the tone playback mode (blocked on T29b), which must pass `CONFIG_XOVER_BRINGUP_MAX_DBFS` as `cap_dbfs`. Record that in the PR so it is not forgotten.

**Commands:** `python tools/check.py`; host build and `ctest --test-dir host/build --output-on-failure`; sanitizer suite (`host/build-san`, `-DENABLE_SANITIZERS=ON`).

**Acceptance:**
1. New assertions fail to compile on `main`, pass after the change (both outputs pasted).
2. Host suite 13/13 and sanitizer suite 13/13.
3. `python tools/check.py` PASS.
4. Diff contains only the three files.

**Rules (5):** C1, C2, C4, S3, V3.

**Stop:** a caller of `bringup_tone_fill` exists outside the test when the card starts. Update that caller only if it is one line and in `firmware/main`; otherwise stop and report.

---

## Not carded here, and why

| Work | Why no 7B card yet |
|---|---|
| Chain runtime contract, chain executor, DSP task | Needs a reviewed component contract first (`03_task_catalog.md`, "Chain runtime contract"); the design choices are not made. |
| T07b protection storage and provisioning | Human-blocked: record values, NVS layout, transport, CODEOWNERS approval. |
| C protection evaluator (Q7/T32 covered only the Python oracle) | Depends on the chain runtime contract and ADR 0014 acceptance. |
| T29a–T29c, T33 | Already carded; waiting on GATE-SPIKE, GATE-ADR, GATE-DOUT2. |
| Control plane (T10–T15 reissue), PCM1808 aux input | Dependent gates; the aux ADC also needs a hardware doc and a pin map entry (S1). |
| Q3, Q4, Q5, Q10, Q11 (Q11 is added to `docs/questions.md` by T36) | Maintainer answers needed first. |
