# Task prompt bundles

Each file in this directory is copied as the complete task prompt. A bundle contains its objective, exact paths, signatures, vectors, rules, setup and stop criteria. It must not require the coding model to derive missing context from the repository.

Bundles now exist for T07a1 through T07a5. Their execution readiness remains controlled by task state and dependencies; a bundle does not authorize starting a blocked card.

## Bundle size target
Keep each prompt below 3,000 tokens including the exact card, normative signature and values, and five applicable rules. Link to ADRs for provenance, but paste all details needed to execute. If a task cannot meet that bound without omitting a decision, split it.
