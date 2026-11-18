# Design

Status: PROPOSED. Items marked VERIFY must be confirmed against a datasheet,
the source of a pinned dependency, or a measurement before code relies on them.

## 1. Fixed facts
| Item | Value | Source |
|---|---|---|
| Sample rate | 48 kHz only | ESP32-audioI2S supports 16-48 kHz in, fixed 44.1/48 kHz out |
| Outputs | 6 used of 8 TDM slots: L woofer, L tweeter, R woofer, R tweeter, sub A, sub B | requirements.md |
| TDM frame | 8 slots x 32 bit | design choice |
| BCLK | 48000 x 8 x 32 = 12.288 MHz | arithmetic |
| MCLK | 256 x fs = 12.288 MHz, from the ESP32 | design choice; VERIFY PCM5122 clocking mode |
| DSP block | 64 frames = 1.333 ms | design choice |
| DSP core | core 1; everything else on core 0 | requirements.md |
| FPU | single precision in hardware; double is software | ESP32-S3 |
| Audio source | ESP32-audioI2S hook `audio_process_i2s(int32_t*, int16_t, bool*)` | verified in source at 5efbc99 |

## 2. Signal path
    ESP32-audioI2S (core 0) -> hook: int32 stereo -> audiosrc ring (SPSC)
        -> DSP task (core 1): int32->float, graph process, float->int32, clip
        -> tdmout i2s_channel_write (8-slot TDM) -> 3x PCM5122

- Input scaling: float = int32 / 2^31. Output: float x 2^31 with saturation to
  int32 range. Headroom: the graph may exceed 0 dBFS internally (float); the
  output stage saturates and counts a clip event.
- Filters run in float32 (direct form II transposed). Coefficients are designed
  in double on core 0, then rounded to float.
- Pacing: the DSP task is paced by the I2S DMA (blocking write). The ring
  producer (decoder, core 0) must block when the ring is full, because with
  `*continueI2S = false` the library no longer paces itself on I2S.

## 3. Filter conventions
- Transfer function: H(z) = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2),
  a0 = 1. This is the scipy.signal convention.
- DF2T: y = b0 x + z1; z1 = b1 x - a1 y + z2; z2 = b2 x - a2 y.
- VituixCAD export: its "Generic" and "Generic -a12" options differ in the sign
  of a1/a2. Which one matches this convention is VERIFY via a golden vector (T13).
- Design uses the bilinear transform with frequency pre-warping, so the
  analog -3.01 dB point lands exactly on fc.
- Butterworth order N: ceil(N/2) sections (an odd order has one first-order
  section stored as a biquad with b2 = a2 = 0).
- Linkwitz-Riley order N (even): two cascaded Butterworth of order N/2.
  Sections = N/2. LR2 = two first-order sections = one biquad.
- LR summing gotcha: for N mod 4 = 0 (LR4, LR8) the LP and HP branches sum to an
  all-pass (|H| = 1) in phase. For N mod 4 = 2 (LR2, LR6) one branch must be
  polarity-inverted to sum flat. The graph does not do this automatically.
- Bessel: normalisation (-3 dB at fc versus constant group delay) is
  VERIFY against VituixCAD. Until decided, `Bessel` is rejected by the device
  loader (E_UNSUPPORTED) even though the schema lists it.
- RBJ cookbook for peq, low/high shelf, notch and all-pass (Q-based).

## 4. Block specifications
| type | required params | range | sections |
|---|---|---|---|
| input | channel | 0..1 | 0 |
| output | slot | 0..7, unique | 0 |
| crossover | mode (lowpass/highpass), family, order, frequency_hz | BW 1..8, LR 2/4/6/8; f 10..0.45 fs | BW ceil(N/2), LR N/2 |
| peq | frequency_hz, q, db | q 0.1..20, db -24..+12 | 1 |
| shelf | shape (low/high), frequency_hz, db | db -24..+12 | 1 |
| allpass | frequency_hz, q | | 1 |
| notch | frequency_hz, q | | 1 |
| biquad | b[3], a[2], sample_rate_hz = 48000 | stability checked | 1 |
| gain | db | -60..+12 | 0 |
| delay | delay_us | 0..20000 (rounded to whole samples, 20.83 us each) | 0 |
| polarity | invert (bool) | | 0 |
| mix | gains[] (one per incoming edge) | | 0 |

Only `frequency_hz` may be supplied as an `@var`. Ranges above are the device
limits; check.py enforces the subset it can (see tools/check.py).

## 5. Graph semantics
- Directed acyclic graph, at most 64 nodes, evaluated in topological order.
- Exactly one incoming edge for every node except `input` (none) and `mix`
  (one per gain). Fan-out is free. Every non-output node has an outgoing edge.
- Parallel paths are not latency-balanced. The user sets `delay` where needed.
- Variables: `vars` maps name -> number. `limits` narrows allowed values.
  Changing a variable recomputes only the nodes that reference it.
- Cost model: total biquad sections <= 96 (initial; replaced by a measured
  figure in T09). Measured cycles per section per sample = UNMEASURED.

## 6. Protection model (fail closed)
Device limits live in NVS, namespace `protect`, one record per output:
`out_id, max_gain_db, stop_below_hz, atten_below_db, stop_above_hz, atten_above_db`
(either stop band may be disabled). Written only by tools/provision_protect.py
over serial.

On every load, for each output o with a record:
1. Compute H_o(f), the complete response from the input to that output
   (including shared upstream nodes), on a grid of 1/96 octave from 10 Hz to
   20 kHz plus the exact band-edge frequencies.
2. Reject if 20 log10 |H_o(f)| > max_gain_db anywhere.
3. Reject if 20 log10 |H_o(f)| > -atten_below_db for any f <= stop_below_hz.
4. Reject if 20 log10 |H_o(f)| > -atten_above_db for any f >= stop_above_hz.
5. An output with no record is forced muted (fail closed).
Known limit: a very narrow, high-Q feature can fall between grid points. Peq q
is capped at 20 and the grid is dense; document any residual risk in an ADR.

## 7. State machine
    BOOT_MUTED -> LIMITS_OK (NVS valid) -> CONFIG_OK (validated) -> RAMP_UP -> RUN
    RUN -> SWAP (ramp down, swap pointer at block boundary, ramp up) -> RUN
    any state -> FAULT_MUTED on NaN/Inf, underrun burst, I2S error, limits invalid
Ramps: down >= 5 ms, up >= 20 ms. v1 swaps by mute-ramp-swap-ramp
(zero state on swap); crossfaded swaps are a later ADR.

## 8. Real-time architecture and budgets
| Item | Rule / value |
|---|---|
| DSP task | pinned to core 1, highest app priority, stack in internal SRAM |
| Block | 64 frames; ring depth 4 blocks (256 frames = 5.3 ms) |
| I2S DMA | 4..6 descriptors x 64 frames; latency about 5 ms |
| Core 0 | Wi-Fi, lwIP, HTTP, decoder, graph build, logging |
| Memory (internal SRAM) | graph pool <= 32 KB; filter state ~1 KB per 6 ch x 20 sections |
| Block time target | p99.9 <= 0.60 ms (45%), max <= 1.00 ms (75%), 1 h soak, Wi-Fi active |
| Underruns | 0 in a 1 h soak |
| DSP noise floor | <= -110 dBFS unweighted 20 Hz-20 kHz, full graph (G4) |
| Swap click | no step above -60 dBFS (G5) |
| Allowed blocking calls in RT | i2s_channel_write, ring read with timeout |

Float32 precision risk: an LR4 at 70 Hz, fs = 48 kHz has poles within about 0.0065
of z = 1. If G4's noise floor or the 20 Hz+ response error fails, escalate in
this order: (1) DF1 with error feedback or a better structure on the low
sections, (2) re-order the cascade, (3) double precision for those sections only
(costs cycles; needs an ADR with measured numbers).

## 9. Hardware (VERIFY against datasheets)
- PCM5122: serial audio port in TDM/DSP format with a data offset register so
  several chips can share one bus. Expected offset for chip n taking two
  adjacent 32-bit slots: 64 x n BCLK. VERIFY with the datasheet before use.
- Three chips need distinct I2C addresses (strap pins) or an I2C mux. VERIFY
  the boards.
- Provide MCLK from the ESP32 rather than relying on the DAC's PLL from BCLK
  (better jitter); VERIFY the PCM5122 clock configuration.
- Start-up: DAC soft-mute asserted, amp mute (if a GPIO exists) asserted, I2S
  running with zeros, then ramp.

## 10. Security
LAN only, bearer token for writes, constant-time compare, write rate limit
5 per minute, body <= 32 KB, JSON depth <= 8. HTTPS is a later ADR. Protection
limits are not reachable from the network.
