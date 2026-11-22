# Open questions (agents append here instead of guessing)

Format: one row per question. Humans fill in the answer column.

| id | task | question | blocking | answer | date |
|---|---|---|---|---|---|
| Q1 | T06a3 | docs/design.md section 4 said numeric fields except `order` may be `@var`, but configs/schema.json (normative per api.md section 3) allows `@var` only for `frequency_hz`. Which is intended? | no | Only `frequency_hz` may use `@var`; `design.md` now matches the schema. | 2026-11-18 |
| Q2 | oracle re-point | `configs/schema.json` allows `gain_db` in [-24, 24] for `peq`, `lshelf` and `hshelf`, but `dsp_design_peq` and `dsp_design_shelf` return `DSP_E_RANGE` above +12 dB (observed rc 2 at +24 dB). Which bound is intended? The oracle follows the schema. | no (blocks the filter-expansion card) |  | 2026-11-22 |
| Q3 | oracle re-point | The schema does not define: `q` for `lp1 hp1 ap1 lshelf hshelf` (the C designers have no q for these; shelves are fixed at Q = 1/sqrt(2)); `gain_db` on types without gain (the oracle rejects non-zero); `ap1` (no C designer; the oracle uses the canonical bilinear first-order all-pass, magnitude 1 regardless); arbitrary-Q `lp2`/`hp2` (the C `make_biquad` is static, so the public API only reaches Butterworth and Linkwitz-Riley Q). Please state the intended semantics before the filter-expansion card is written. | no (blocks the filter-expansion card) |  | 2026-11-22 |
