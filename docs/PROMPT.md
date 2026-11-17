# AI configuration authoring prompt

Generate a version-1 xover-esp32 JSON graph using the supplied schema and only the driver data explicitly provided. Keep sample_rate_hz at 48000. Do not invent driver limits, GPIO assignments, DAC slot assignments or filter coefficients. Prefer semantic filter nodes; use raw biquad coefficients only when supplied with their sample rate and convention. Explain assumptions separately from the JSON. The device must independently validate every uploaded configuration.
