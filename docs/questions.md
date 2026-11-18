# Open questions (agents append here instead of guessing)

Format: one row per question. Humans fill in the answer column.

| id | task | question | blocking | answer | date |
|---|---|---|---|---|---|
| Q1 | T06a3 | docs/design.md section 4 said numeric fields except `order` may be `@var`, but configs/schema.json (normative per api.md section 3) allows `@var` only for `frequency_hz`. Which is intended? | no | Only `frequency_hz` may use `@var`; `design.md` now matches the schema. | 2026-11-18 |
