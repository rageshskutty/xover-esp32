# Designer-side input and legacy-reference inventory

This docs-only inventory records designer-side driver/amp/curve inputs and legacy references. Physical inputs are permitted in designer tooling only; none is a firmware or device JSON contract.

## Source model file
- docs/designer-side/driver-inputs.example.yaml contains designer-side driver model placeholders. It is not part of device JSON or a firmware configuration contract.

## Legacy negative fixtures
These files exercise retired separately provisioned driver/amp/curve inputs or obsolete device shapes. Their expected result is now E_SCHEMA because the current closed schema rejects the legacy document before any retired semantic protection checks. They are not deployable device configurations:
- configs/invalid/empty_chain_protection_failure.json: obsolete output-chain/protection shape; rejected by the current closed schema.
- configs/invalid/vpk_exceeds_clip_v.json: legacy vpk_at_0dbfs and clip_v test data.
- configs/invalid/x_model_validation_unavailable.json: legacy x_model (fc_hz, Qtc, mm_per_v) test data.
- configs/invalid/protected_missing_key.json: legacy vpk_at_0dbfs, clip_v, re_ohm, x_model, min_freq_hz, lim_bounds, and lim_default metadata.
- configs/invalid/protected_unknown_key.json: legacy xmax_m field.
- configs/invalid/missing_subsonic_highpass.json: legacy _test_min_freq_hz curve metadata.
- configs/invalid/orphan_protected_record.json, protected_record_missing_for_output.json, and protected_tdm_slot_limit.json: legacy external protected-record joins/slot metadata.
- configs/invalid/protection_ts_field_rejected.json: intentional negative test; a T/S field must be rejected by the closed schema.

These fixtures are retained only as migration evidence until separate host-tool/fixture cleanup. They must never be uploaded to a device or used as positive schema examples.

## Historical planning references
- docs/PLAN.md contains retired graph/DAG task planning text and synthetic-record references. It is explicitly a retired legacy plan; those tasks are not current firmware gates.

## Host tooling references
- tools/check.py contains legacy host-only test_protected handling and names such as vpk_at_0dbfs, clip_v, xmax_mm, re_ohm, pmax_w, x_model, min_freq_hz, lim_bounds, and lim_default. It is not firmware code or the device JSON schema. It is explicitly retired from defining protection semantics and is pending later host-tool cleanup.
- tools/protection_oracle.py and tools/test_protection_oracle.py consume designer-supplied abstract response curves and min_freq_hz boundaries. They are designer-side reference tools, not firmware inputs. Retain and re-scope them as described in ADR 0014.

No driver T/S, volts, watts, or driver-curve fields are permitted in normative device JSON.
