# Requirements

- 2.1 system: stereo two-way mains plus sub; six outputs.
- ESP32-S3 with three PCM5122 DACs sharing an I2S TDM bus.
- DSP executes on the ESP32-S3; DACs are used as DACs.
- Sample rate: 48 kHz in v1.
- Precompiled DSP blocks; JSON config connects blocks and sets parameters.
- Config upload from phone over Wi-Fi; device validates graph and protection limits.
- DSP task pinned to core 1; networking, decoding and configuration on core 0.
- Block set planned: crossover, PEQ, shelf, all-pass, notch, biquad, gain, delay, polarity and mix. FIR deferred.
- Exact ESP-IDF, Arduino component and audio-source versions must be pinned after compatibility testing.

## Open decisions
- Confirm exact ESP32-S3 module, PSRAM and flash size.
- Confirm GPIO pin map, TDM slot mapping and PCM5122 address straps.
- Confirm audio source and input transport.
- Define driver protection limits from datasheets and measurements before enabling output.
