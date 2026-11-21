# ADR 0007: Frozen graph build and runtime ownership contract

- Status: **Superseded for the product architecture** by ADR 0012 (chain-per-output execution).
- Historical decision date: 2026-11-19
- Retained for traceability; it is not normative for new configuration, validation, build, response, or runtime work.

## Historical decision

This ADR specified a general-purpose DAG runtime with `dspg_validate`,
`dspg_build`, `dspg_process`, caller-owned arenas, topological execution,
and graph-generation ownership.

That architecture is retired by ADR 0012. The new product configuration is
a set of independent, ordered per-output chains, with optional shared input
pre-chains. There is no user-authored DAG, edge table, topological sort,
fan-out node, or general graph executor in the new design.

## Reuse boundary

Existing `dspblocks` filter design, SOS processing, response evaluation,
numeric-safety helpers, and verified host tests remain reusable where their
contracts still apply.

The old `dspgraph` implementation and its graph-specific contracts are not
to be extended for the new architecture. The replacement component and its
public interfaces are defined incrementally by ADR 0012 and the later
`dspchain` implementation task cards. Do not delete or rewrite historical
source as part of this ADR-only planning change; removal/replacement is a
separate, explicit migration task.

## Historical implementation notes

The old graph contract required caller-supplied aligned memory, no hidden
allocation, build on core 0, processing on the DSP owner, and acknowledged
reclamation after a generation switch. Those ownership principles remain
useful, but their graph-specific signatures and data model do not.

## Verification status

Historical planning contract only. This ADR does not claim that a graph
runtime was implemented or verified.
