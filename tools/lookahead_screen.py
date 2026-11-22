#!/usr/bin/env python3
"""Host screening of limiter lookahead L and attack A (ADR 0014 section 3).

HOST-SCREENING only: linear sealed-box excursion weighting (2nd-order low-pass,
Q = 0.707) as detector, instantaneous-peak detector with 17 ms hold, full-scale
burst 6 dB above the limit. Not a loudspeaker simulation and not a gate.
Prints the worst excursion overshoot (dB above the limit) over bursts at
0.6, 1.0 and 1.5 x Fc. Reference algorithm: delay audio by L; r = min(1,
T / held weighted peak) from the undelayed signal; gain = moving average over
the last A samples of the minimum of r over the last A samples; gain may rise
only through a one-pole release.
"""
import numpy as np
from scipy.ndimage import maximum_filter1d, minimum_filter1d, uniform_filter1d
from scipy.signal import bilinear, lfilter

FS = 48000
Q_TC = 0.7071
HOLD_MS = 17.0
RELEASE_MS = 100.0
OVER_DB = 6.0


def overshoot_db(fc_hz, burst_hz, lookahead, attack):
    n = int(0.6 * FS)
    t = np.arange(n) / FS
    x = np.zeros(n)
    on = (t > 0.10) & (t < 0.35)
    x[on] = np.sin(2.0 * np.pi * burst_hz * t[on])
    w_c = 2.0 * np.pi * fc_hz
    b, a = bilinear([w_c * w_c], [1.0, w_c / Q_TC, w_c * w_c], FS)
    xs = lfilter(b, a, x)
    limit = np.max(np.abs(xs)) / 10.0 ** (OVER_DB / 20.0)
    hold = int(HOLD_MS * 1e-3 * FS)
    det = maximum_filter1d(np.abs(xs), hold, origin=(hold - 1) // 2)
    r = np.minimum(1.0, limit / np.maximum(det, 1e-12))
    g = uniform_filter1d(minimum_filter1d(r, attack, origin=(attack - 1) // 2),
                         attack, origin=(attack - 1) // 2)
    alpha = np.exp(-1.0 / (RELEASE_MS * 1e-3 * FS))
    gain = np.empty(n)
    state = 1.0
    for i in range(n):
        state = g[i] if g[i] < state else alpha * state + (1.0 - alpha) * g[i]
        gain[i] = state
    y = np.concatenate([np.zeros(lookahead), x[:n - lookahead]]) * gain
    return 20.0 * np.log10(np.max(np.abs(lfilter(b, a, y))) / limit)


def worst(fc_hz, lookahead_ms, attack_ms):
    la, at = int(lookahead_ms * 48), int(attack_ms * 48)
    return max(overshoot_db(fc_hz, fc_hz * k, la, at) for k in (0.6, 1.0, 1.5))


def main():
    lookaheads = (2, 4, 6, 8, 10, 12, 14, 16)
    print("worst overshoot, dB above limit; columns = lookahead L in ms")
    print("%-34s" % "" + " ".join("%5d" % v for v in lookaheads))
    for fc in (30, 50, 80):
        tau0 = 1e3 / (Q_TC * 2.0 * np.pi * fc)
        print("Fc=%d Hz, tau0=1/(Q*wc)=%.1f ms" % (fc, tau0))
        print("  %-32s" % "A = L (coupled)" +
              " ".join("%5.2f" % worst(fc, v, v) for v in lookaheads))
        print("  %-32s" % "A = 2 ms (decoupled)" +
              " ".join("%5.2f" % worst(fc, v, 2.0) for v in lookaheads))


if __name__ == "__main__":
    main()
