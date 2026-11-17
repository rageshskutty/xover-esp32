# ADR 0003: Per-node config schema contract
- Status: accepted
- Date: 2026-11-18
- Task: T06a1

## Context
T06 requires config validation before graph construction. The existing schema
allowed arbitrary node keys and left most node parameter shape checks to the
semantic validator, so malformed node structures could reach later stages.

## Decision
Make `configs/schema.json` normative for node structure. It rejects unknown
keys that do not belong to a node type, requires parameters for each node type,
and bounds parameter shapes and ranges, including frequency, Q, gain, sample
rate, channel count, mix width, delay duration, and Linkwitz-Riley orders.
Semantic validation still reports graph relationships and established error
codes such as
`E_NODE_PARAM`, `E_GRAPH_CYCLE`, and `E_PROTECT`.

## Alternatives considered
Keep all node validation in the semantic layer, or reject every node parameter
failure as `E_SCHEMA`. The former duplicates basic structural checks; the
latter would break the public error-code contract in `docs/api.md`.

## Consequences (include measured data when changing a gate)
Schema checks now fail earlier on malformed node objects. The host reference
checker evaluates schema and semantic rules together so schema range failures
do not hide established semantic codes in invalid-config fixtures. No safety
gate or hardware limit changes.
