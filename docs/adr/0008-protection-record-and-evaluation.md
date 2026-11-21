# ADR 0008: Protection curves and static evaluation

- Status: Superseded for firmware by ADR 0014 (0014-abstract-protection-block.md).
- Historical only: driver curves, driver/amp/supply records, and static response comparator language are not firmware contracts and must not generate firmware tasks or device JSON.
- Designer-side modelling and margin reporting are outside firmware. The firmware contract is exclusively the closed per-output abstract block in configs/schema.json.
