# Execution protocol for bounded coding models

## Session start
Read only: the assigned prompt bundle, its named public/internal header excerpts, and the five rules listed in the bundle. Do not read the full planning corpus unless a referenced artifact is missing. Repository AGENTS.md remains authoritative if applicable safety rule conflicts.

## Work loop
1. Confirm branch is based on the named baseline and working tree is clean. Do not checkout, reset, amend or rewrite the source branch.
2. Read only the files named by the card and exact dependencies.
3. Add tests and golden assertions first. Run the focused test and record its failing output. Never change expected values after seeing implementation output unless a documented reference proves the original vector wrong; escalate instead.
4. Implement only the specified behavior and paths. No refactor, new dependency, signature change, scope expansion, or unrelated formatting.
5. Run focused test, full host tests and `python tools/check.py`. Use sanitizer checks where required. Do not claim an unrun command passed.
6. Inspect `git diff --check`, status and diff. Output only the final diff summary and verification evidence; do not paste whole files or unrelated repository context.

## Retry and escalation
Maximum three implementation attempts total. An attempt means a code/test change followed by the specified focused check. After a third failure, stop: preserve the best diagnostic, revert only the task's own uncommitted edits, and report the blocker. Do not retry by changing requirements or gates. Escalate immediately (no attempt consumed) if a signature/type/value is missing, headers conflict with card, test vectors are insufficient, a hardware fact is needed, a dependency/license decision appears, scope exceeds 80 production LOC, or a test requires changing a safety threshold.

## Diff-only completion format
Return: (1) one-sentence behavior, (2) changed paths and production LOC, (3) focused and full check names with pass/fail/UNVERIFIED, (4) expected failing test evidence, (5) deviations/blockers. Include no source dump and no unrelated recap. Never claim HW-VERIFIED; only cite an owner-authored bring-up log.
