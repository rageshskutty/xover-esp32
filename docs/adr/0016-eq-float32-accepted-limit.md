# ADR 0016: EQ block float32 coefficient accuracy, accepted limit for the first release

- Status: Proposed (maintainer chose to accept the limit on 2026-11-24; it is Accepted when this ADR is merged)
- Date: 2026-11-24
- Task: review correction plan H5 / Step 8 (card T31)
- Relates to: ADR 0005 (G2 gate, "known limit" paragraph)

## Context
G2 requires float32 execution within 0.01 dB of double above 20 Hz. ADR 0005
fixed this for the crossovers (delta form on the denominator) and recorded that
EQ blocks (`peq`, shelves, all-pass, notch) keep plain float32 coefficients.
Coefficient-only response error against the double design (HOST-VERIFIED, NumPy):

| filter | max error 10-200 Hz |
|---|---:|
| 30 Hz, Q 5, -6 dB | 0.172 dB |
| 20 Hz, Q 3, +6 dB | 0.106 dB (0.089 dB at 20 Hz and up) |
| 40 Hz, Q 8, +6 dB | 0.055 dB |
| 60 Hz, Q 5, +6 dB | 0.021 dB |
| 100 Hz, Q 5, +6 dB | 0.005 dB |

ADR 0005 measured the worst case over Q 1..10 and gain -6/+6/+12 dB: 30 Hz
0.254 dB, 40 Hz 0.085 dB, 60 Hz 0.073 dB, 100 Hz 0.020 dB, 200 Hz 0.005 dB.
Applying hi/lo to the denominator only does not help: it worsens the error
(0.172 -> 0.202 dB at 30 Hz, Q5, -6 dB) because the numerator and denominator
errors partly cancel. A real fix needs numerator and denominator both in hi/lo,
which grows `dsp_sos_t` (public header) and changes the process loop.

## Decision
Accept the limit for the first release instead of changing `dsp_sos_t`.
1. G2 (0.01 dB) stays unchanged for the crossovers. For EQ blocks the accepted
   coefficient-only response error is at most 0.26 dB, valid for centre
   frequency 30 Hz and above, Q up to 10, boost up to +12 dB (the cap in Q2).
2. Lock the measured numbers in `host/tests/test_f32.c` (EQ rows at 30, 40, 60,
   100, 200 Hz, several Q) with a 0.26 dB limit so the error cannot get worse
   unnoticed. The test documents the limit; it does not claim 0.01 dB.
3. Not covered by this ADR and unmeasured: centre frequency 20-30 Hz (the schema
   minimum is 20 Hz), Q above 10 (the schema allows 20), and float32 state
   rounding noise of low-frequency EQ sections. Using EQ filters outside the
   accepted range is allowed by the schema but is outside this ADR.
4. Revisit before the next release: measure Q 10-20 and 20-30 Hz, check the
   WCET headroom once T09 lands, then decide whether to implement numerator and
   denominator hi/lo.

## Alternatives considered
- Hi/lo on numerator and denominator now: error about 0 dB in the cases above, but changes a public header and the process loop, costs roughly 2x multiply-accumulates per EQ biquad (estimate, WCET UNVERIFIED), and needs its own card.
- Hi/lo on the denominator only: rejected, it makes the error worse.
- Hi/lo only for centre frequencies below about 60 Hz: error is within the gate at 100 Hz (0.005 dB), but two filter code paths add complexity and risk for a small gain.

## Consequences (include measured data when changing a gate)
A 30 Hz Q5 EQ filter can deviate by up to 0.17-0.25 dB from its designed
response. For a subwoofer working above 30 Hz and tuned by measurement this is
below what can be heard or measured in a room, but it is a deliberate deviation
from G2 (V3: do not loosen a gate silently), so it is recorded here. Nothing in
the protection stage depends on EQ block accuracy at this level.
