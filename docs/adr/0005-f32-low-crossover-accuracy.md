# ADR 0005: Float32 accuracy of low crossovers (G2)
- Status: proposed (needs human approval, touches a public header)
- Date: 2026-11-18
- Task: T03b (fix to T03/T05, found while checking the repo against docs/PLAN.md)

## Context
G2 requires float32 execution within 0.01 dB of double above 20 Hz. The existing
scipy golden vectors round the reference coefficients to float32 first, so they
could not see coefficient quantisation. `host/tests/test_f32.c` now compares the
library against a closed-form double reference (bilinear Butterworth / LR
magnitude) and fails on the old code:

| case (fs 48 kHz, 20 Hz - 20 kHz) | response error | execution error |
|---|---|---|
| LR4 fc 70 Hz | 0.0165 dB at 20 Hz | 0.0169 dB |
| LR8 fc 80 Hz | 0.0119 dB at 20 Hz | 0.0185 dB |

## Decision
Escalation step (1) of design.md section 8, no double precision in RT code (R1):
1. `dsp_sos_t` gains `da1`, `da2`: the float32 low parts of the denominator
   coefficients, so `a1 + da1` carries about 48 bits. Crossover designs fill
   them; EQ designs set them to 0.
2. `dsp_sos_process` runs the recursion in delta form. With a1 = -2 + p and
   a2 = 1 - q, p = (a1 + 2) + da1 and q = (1 - a2) - da2 are computed once per
   section per call (the subtractions are exact, Sterbenz) and the loop is
   `z1 = ((b1*x + (y+y)) - p*y) + z2; z2 = (b2*x - y) + q*y`.
3. `dsp_sos_eval` uses `a + da` in double.
4. The execution half of the test checks tones whose ideal level is >= -60 dB.
   Below that the output is at the float32 noise floor by nature (G4 owns it).
   Human: please confirm this floor. The 0.01 dB gate itself is unchanged.

## Alternatives considered (LR4 HP fc 70 Hz, 20 Hz tone, error vs double)
- DF2T with hi/lo products inside the loop: -0.0152 dB, fails the gate.
- DF1 with hi/lo: -0.0163 dB, fails the gate.
- Double precision for low sections: forbidden by R1, not needed.
- Delta form (chosen): +0.0016 dB.

## Consequences (include measured data when changing a gate)
HOST-VERIFIED: worst response error 0.0000 dB, worst execution error 0.0034 dB
over 4 shapes x LP/HP x 5 fc x 5 tones. Per-sample cost is about 10 flops
against 9 before; per-section per-call cost is 2 extra adds. WCET is
UNVERIFIED until T09. `dsp_sos_t` grows from 20 to 28 bytes (6 ch x 20 sections
= 3.4 KB, inside the 32 KB pool).
Known limit, not fixed here: EQ blocks (peq, shelf, allpass, notch) keep plain
float32 coefficients. Coefficient-only error of a float32 PEQ vs double, worst
over Q 1..10 and gain -6/+6/+12 dB: 30 Hz 0.254 dB, 40 Hz 0.085 dB, 60 Hz
0.073 dB, 100 Hz 0.020 dB, 200 Hz 0.005 dB. Needs its own task (numerators too).
