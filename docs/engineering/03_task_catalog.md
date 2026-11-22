# 7B execution catalog

## Current authority

The product configuration is the parametric filter-stack contract in
configs/schema.json. ADR 0012 defines chain-per-output execution; ADR 0014
defines protection and fail-closed behavior. Graph/DAG implementation cards
are retired. Do not infer new public C interfaces from legacy graph headers.

JSON is the source of truth. Firmware validates it and expands each filter
into biquad sections once at configuration load. The audio task executes
bounded, precompiled per-output tables; it does not parse JSON, design
filters, allocate, log, lock, or perform unbounded work.

## Required completion evidence

For each card, report exact files, production LOC, test names, command lines
and exit codes, expected failing baseline, final output, diff summary, and
deviations. Run tools/check.py, host build, and CTest. Run ASan/UBSan for
parsing, configuration, and DSP arithmetic changes. Hardware results are
UNVERIFIED unless an owner records them in docs/bringup-log.md. Never
regenerate golden vectors merely to make a failure pass.

## Retired planning material

The former graph-era protection-record prompt remains only as a retirement
notice. Other obsolete graph-era protection prompts and the proposed
numerical-contract document have been removed. They are not queued work.
Historical ADRs remain for provenance; ADR 0014 is the current authority.

## Human-blocked work

- Real protection provisioning requires owner-approved output values and
  documented hardware evidence. Synthetic examples are not hardware profiles.
- Board/DAC integration requires exact module, IDF version, pin map, DAC
  address/straps/registers, slot map, and safety provisions.
- Chain execution-table layout, memory ownership, response API, and target
  budget require an explicit reviewed contract before runtime coding.

## Next implementation sequence

T16–T22 are the next real implementation work after schema-derived task
cards and the pull-request gate list are reconciled. Each card must remain a
small, independently testable change with explicit inputs, outputs, bounds,
and stop conditions. Do not infer card content from historical graph tasks.

## 7B execution criteria

Before starting, a bundle must give one objective, exact paths, immutable
signatures, accepted/rejected inputs, expected values and tolerances,
commands, expected baseline failure, five or fewer applicable rules, and
stop conditions. A card passes only if no design/API/safety choice is deferred;
production scope is 30–80 LOC or split; numerical assertions have explicit
tolerances; tests precede implementation; host logic has no hardware
dependency; human gates are blocked, never guessed; retries are bounded.
