# ADR 0011: Configuration mutation, commit, and response ownership

- Status: Proposed planning contract; human safety review and CODEOWNERS approval required.
- Date: 2026-11-21
- Reconciles the held draft PR #16 with the chain-per-output architecture.
- Depends on: ADR 0012 and ADR 0013

## Scope

This ADR preserves the useful configuration-transaction and fail-closed
mutation rules from the held graph-era ADR 0011. It removes graph-specific
response traversal, `dspg_set_var`, and graph-statistics declarations. The
replacement chain response and statistics interfaces will be specified with
the `dspchain` component in a later implementation contract.

## Configuration transaction gate

All configuration mutations share one transaction gate. A concurrent
mutation must return `E_BUSY` before building a candidate, writing storage,
or changing live state. The winning transaction derives its candidate from
the currently active configuration generation after acquiring the gate.

A candidate is parsed, schema-validated, semantically validated, statically
protection-checked, and compiled off the DSP task. It is not published until
the DSP owner acknowledges the safe boundary transition. The DSP task remains
the sole owner of active chain state.

No network/API path can modify the separately provisioned protected block or
its output limit curves.

## Explicit persistence

The product policy is that configuration is persisted only after an explicit
user commit. Slider movement, preview, or an uncommitted edit must not cause
flash writes. The exact distinction between preview, live apply, and commit,
and whether commit is persist-before-swap or apply-then-persist, remains a
human-reviewed interface decision. Until that decision is recorded, agents
must not invent an endpoint or write ordering.

For an explicitly committed configuration, retain the existing A/B safety
principles unless the human-approved commit protocol replaces them:
1. write the inactive slot;
2. read it back and validate its header, length, schema/version, and CRC;
3. request the owner-acknowledged transition at a safe audio boundary;
4. publish the active-slot selector only at the protocol-defined commit point;
5. report success only after all required steps have succeeded.

A rejected owner transition leaves the old active configuration selected.
A staged inactive slot without selector publication is ignored at boot.
Power loss before selector publication must recover the previous committed
configuration.

A selector-write failure after the DSP owner has acknowledged a new live
configuration is a storage fault. The system must mute and enter its
fail-closed fault state; it must not report success. Recovery must not
silently treat the staged slot as committed.

## Swap acknowledgement and fault behavior

The held PR #16 proposed a 250 ms owner-acknowledgement timeout. Retain
250 ms as a provisional planning value only; it requires measurement and
human approval before implementation. If the timeout expires, the swap
outcome is uncertain because the DSP owner may still complete it. The
application must mute, enter the fail-closed fault path, avoid publishing the
selector, and not report success.

The held PR #20 state-machine bundle defines `FAULT_MUTED` as terminal for
the lifetime of the state-machine instance. The current product assumption is
that this latch is cleared only by a power-cycle/reinitialization, followed
by fresh protection and configuration validation. This is provisional and
requires human safety review; no ordinary API operation may clear it.

The transaction gate is released on every outcome. While a latched fault is
active, configuration mutations return `E_BUSY` and perform no build or
write.

## Flash wear

The one-persisted-mutation-per-1000-ms rule in the held PR #16 is not adopted
as a sufficient flash-wear policy. At that rate it could still exhaust a
two-slot flash arrangement rapidly. A flash endurance budget, commit
frequency policy, and any debounce/coalescing behavior require an explicit
human decision. No agent may invent a wear budget or treat a rate floor as
proof of endurance.

## Response ownership

Response reporting and static protection both use the chain-per-output
model. The response path must expose each input-channel-to-output transfer
separately; it must not collapse stereo inputs into a single phase-dependent
response. Static protection combines the per-input magnitudes conservatively
using the `Σ |w| · |H|` rule in ADRs 0008 and 0010.

The response API, exact frequency bounds, output statistics snapshot, and
cross-task ownership are deferred to the `dspchain` public contract. No
`dspg_*` graph API from the held draft is normative for the new architecture.

## Verification status

Planning contract only. No transaction, persistence, response, state-machine,
or hardware behavior has been verified in this ADR update.
