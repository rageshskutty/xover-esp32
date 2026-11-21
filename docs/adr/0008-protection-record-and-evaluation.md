# ADR 0008: Per-output protection curves and static safety evaluation

- Status: Superseded by ADR 0014 (2026-11-22).
- Scope: Historical record only; this document is not normative and must not be used to generate firmware tasks, schemas, APIs, or protection data.

The firmware protection contract, schema, designer/firmware trust boundary, limiter behavior, and SciPy-oracle role are now defined exclusively by [ADR 0014](0014-firmware-protection-contract.md).

Any former production C comparator, T/S-derived firmware protection record, driver/amplifier/PSU provisioning interface, and related gates are retired from firmware scope. Driver and hardware models belong to designer-side tooling only.
