# ADR 0006: Graph validator public API
- Status: proposed (needs human approval, CODEOWNERS path)
- Date: 2026-11-18
- Task: T06a3

## Context
T06 splits into validator, build and engine PRs. The public header
`dspgraph.h` was a stub, and docs/api.md section 2.2 is to be frozen at T06
(C5). The device has no JSON Schema library, so the schema rules in
configs/schema.json must be re-implemented in C.

## Decision
`dspgraph.h` gains `dspg_issue_t`, `dspg_report_t` (8 issues max, as in
api.md) and `dspg_validate(json, len, report)`, returning 0 or -1. The later
`dspg_build` calls the same validator first. Codes follow api.md section 1:
wrong JSON type, unknown key or unknown node type report `E_SCHEMA`; a missing
or out-of-range node parameter reports `E_NODE_PARAM`; Bessel reports
`E_UNSUPPORTED`; more than 64 nodes reports `E_RESOURCE`. Graph-level rules
(references, cycles, fan-in, slots, variables, limits, section cap) landed in T06a4 and run only when
the structural pass found no issue; biquad stability lands in T06a5; `dspg_protect_t` lands with T07.

## Alternatives considered
Return an opaque graph from `dspg_build` immediately (too large for C1), or
embed a generic JSON Schema engine (code size, new dependency, X4).

## Consequences
The C validator and tools/check.py now implement the same rules twice;
T06a4 adds a differential test (tools/test_validator_diff.py) so they cannot drift. Per-type key tables in
dspgraph_validate.c must change together with configs/schema.json.
