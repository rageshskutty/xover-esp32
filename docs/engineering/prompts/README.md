# Task prompt bundles

Each file in this directory is copied as the complete task prompt. A bundle contains task objective, exact scope, signatures, test vectors, relevant rules and stop criteria. Never ask the coding model to derive missing context from the full repository.

Current first executable bundle: `T07a1.md`. Later bundles become executable only after dependencies are accepted and their state changes to queued in task_state.json.

## Bundle size target
Keep each prompt below 3,000 tokens including the exact card, relevant API/header excerpts and five applicable rules. Link to ADRs only for provenance; paste all normative signatures and values needed to execute. If a task cannot meet that bound without omitting a decision, split it.
