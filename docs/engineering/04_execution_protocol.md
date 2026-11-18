# Small-model execution protocol

## Durable state and read order
At every fresh session read, in order: root `AGENTS.md`; `docs/engineering/00_repository_inventory.md`; `01_architecture_and_constraints.md`; `02_contracts_and_verification.md`; `task_state.json`; then only the current task card plus its named source/header/tests. Use legacy `docs/PLAN.md`, `docs/design.md`, `docs/api.md`, schema, ADRs and source as authorities where this blueprint points to them. If a conflict exists, stop and report the exact files/lines; do not implement an interpretation.

## Queue protocol
1. Read task_state; select first `queued` item whose dependencies are all `done` and whose human decision blockers are closed.
2. Copy exactly one task card from `03_task_catalog.md` into the coding session. Include current commit/base SHA and scope.
3. Implement only listed files. Before public header changes, add ADR and update api.md. New dependency requires license, pinned version and ADR.
4. Run task-specific tests and common host checks. Keep before/after command output. Never label an unrun check as passed.
5. Report file list, summary, tests/build outputs (HOST-VERIFIED/HW-VERIFIED/UNVERIFIED), deviations, outstanding questions, and resulting commit SHA.
6. Reviewer updates task_state to done only after acceptance evidence; otherwise return to queued/blocked with blocker. Do not advance dependents early.
7. After a hardware gate, only the human records HW-VERIFIED result and log citation.

## Scope lock
Allowed: implement only task objective and listed files; update directly related doc and status. Disallowed: architecture/API changes not in accepted ADR, extra dependencies, unrelated formatting, source changes for a documentation-only card, weakening thresholds, changing golden vectors to hide regressions, asserting hardware facts from inference. If the task needs a new file or contract, pause and ask for an ADR/task card rather than self-expanding.

## State meanings
`blocked`: human decision/evidence or prerequisite missing; include blocker owner and exact resolution. `queued`: ready after deps. `in_progress`: one active task only, branch/PR recorded. `review`: implementation complete awaiting evidence/review. `done`: acceptance verified and merged to the agreed integration base. `deferred`: intentionally outside current v1, reason and revisit condition recorded. Never infer done from a PR title.

## Integration and regression checklist
- [ ] One task, one branch, one PR; <=400 changed lines excluding golden/generated.
- [ ] Exact contract/ADR updated for any public interface.
- [ ] No hardware assumptions without cited source; protections remain fail-closed.
- [ ] RT path obeys R1-R6; internal SRAM allocations happen before publish.
- [ ] `python tools/check.py`, host build, CTest, sanitizer/coverage as applicable pass with output.
- [ ] Existing invalid fixtures return expected error codes; golden and analytic response behavior preserved.
- [ ] Target/hardware claims labeled and logged by authorized human.
- [ ] Task completion report and task_state updated with commit and evidence.
