# ADR 0017: Peak chain gain cap (bounds stacked EQ boosts)

- Status: Proposed (maintainer chose the policy on 2026-11-24, see Q7; becomes Accepted when this ADR is merged and CODEOWNERS review is done)
- Date: 2026-11-24
- Task: T31 follow-up (Q7, correction plan H6 open point)
- Relates to: ADR 0014 (firmware protection contract), ADR 0016, Q2 (+12 dB per filter)

## Context
Q2 caps each `filter` entry at +12 dB boost. That does not bound a chain: three
+12 dB PEQ filters at one frequency give a static bound of +36.0 dB
(HOST-VERIFIED with `tools/protection_oracle.py`, Q7). The oracle reports the
static bound `bound_db` but does not compare it with any limit, so such a config
is not rejected. The danger is the peak gain of the whole chain at any
frequency, not the number of boosts: boosts at far-apart frequencies do not
stack.

## Decision
1. Define the chain peak gain as the maximum over the evaluation grid of the
   static magnitude response of the output chain (pre chain, mix weights,
   filters and gain blocks), in dB, relative to the input. This is the existing
   static `bound_db` plus gain blocks; the oracle already computes it.
2. A config is rejected with `E_PROTECT` when the peak gain of any output chain
   exceeds the cap.
3. The default cap is +18 dB (enough for room correction, which is typically
   +6 to +12 dB; blocks a mistake such as three stacked +12 dB boosts).
4. The cap is a protection setting. It is stored with the protection record
   (provisioned, CODEOWNERS path), not in the uploaded config. The web
   interface may supply an optional lower value in the config; a value above
   the record's cap is rejected. The web interface can never raise the cap
   (AGENTS S4). Raising the cap above +18 dB needs provisioning with maintainer
   sign-off.
5. Cuts are not limited by this ADR (Q2 keeps -24 dB per filter).

## Alternatives considered
- Limit the number of boost filters: rejected, it blocks safe configurations
  (boosts at different frequencies) and allows unsafe ones (one +12 dB boost
  plus a gain block of +12 dB).
- A fixed +12 dB or +24 dB cap: +12 dB blocks legitimate tunings, +24 dB is too
  permissive for the first release.
- A web-adjustable cap that can go up: rejected, it breaks AGENTS S4.

## Consequences (include measured data when changing a gate)
Implementation card (not yet written, needs this ADR Accepted): add the cap
field to the protection record schema and to `tools/protection_oracle.py`,
return `E_PROTECT` above the cap, add a `configs/invalid/` fixture, tests for
three stacked +12 dB filters at one frequency (rejected), the same filters
spread over far-apart frequencies (accepted), one +12 dB filter (accepted), a
lower web value (accepted), and a web value above the record (rejected). The C
protection evaluator must apply the same rule; Python and C share the vectors in
`tools/protection_oracle_vectors.json`. UNVERIFIED: the evaluation grid
resolution is the oracle's existing grid (1/96 octave per PLAN risk table); a
very narrow peak between grid points is the already-recorded residual risk.
