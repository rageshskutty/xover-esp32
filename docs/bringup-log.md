# Hardware bring-up log (human-maintained evidence)

Agents may add the procedure for a gate, never a result. Results are entered by
the human who ran the measurement. Each entry: date, firmware commit, setup,
instrument, numbers, pass/fail against the gate in docs/PLAN.md.

## Template
- Date:
- Firmware commit:
- Gate / task:
- Setup (amps/speakers disconnected? yes/no):
- Instrument and settings:
- Measured values:
- Result: PASS / FAIL (HW-VERIFIED)
- Notes:

## Protected block and per-output curves (human-owned; required before output enable)

The retired stop-band fields are not authoritative. Record source evidence
for every driver, amplifier, and supply value before deriving any output
curve. Agents must not fill this table with estimates.

| output | driver/model source | xmax_mm | re_ohm | pmax_w | tau_s | min_freq_hz | amp source | vpk_at_0dbfs | clip_v | supply source | curve revision/source |
|---|---|---:|---:|---:|---:|---:|---|---:|---:|---|---|
