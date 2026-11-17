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

## Protection limits provisioned (required before the S3 cap is lifted)
| output | stop_below_hz | atten_below_db | stop_above_hz | atten_above_db | max_gain_db | source (datasheet/measurement) |
|---|---|---|---|---|---|---|
