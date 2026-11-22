# Task prompt bundles

Each file in this directory is copied as the complete task prompt. A bundle contains its objective, exact paths, signatures, vectors, rules, setup and stop criteria. It must not require the coding model to derive missing context from the repository.

Legacy graph-era prompts are retired. Current executable bundles are generated from the JSON schema and reviewed T16–T22 plan; task state and human gates control readiness.

## Bundle size target
Keep each prompt below 3,000 tokens including the exact card, normative signature and values, and five applicable rules. Link to ADRs for provenance, but paste all details needed to execute. If a task cannot meet that bound without omitting a decision, split it.
