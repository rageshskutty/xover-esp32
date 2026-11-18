# ADR 0004: Bounded JSON parser implementation
- Status: accepted
- Date: 2026-11-18
- Task: T06a2

## Context
Configuration uploads must use the same JSON parser on host and target, with
bounds applied before parsing untrusted input.

## Decision
Pin cJSON 1.7.18 at commit `acc76239bee01d8e9c858ae2cab296704e52d916` as a
submodule and compile its `cJSON.c` source on both host and target. Reject
empty or oversized bodies, nesting deeper than eight levels, malformed JSON,
and trailing data.

## Alternatives considered
Use the ESP-IDF bundled cJSON on target and a separate host parser, or write a
custom JSON parser. The former risks version drift; the latter adds parser
maintenance and security surface.

## Consequences (include measured data when changing a gate)
Host tests exercise the same pinned cJSON source used by the target component.
The existing 32 KB body-size and depth-eight limits are unchanged.
