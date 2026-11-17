# Audio source integration

Candidate: https://github.com/schreibfaul1/ESP32-audioI2S

## Callback observed in the selected source tree

The integration hook identified for this project is:

```cpp
void audio_process_i2s(int32_t* outBuff, int16_t validSamples, bool* continueI2S);
```

The exact upstream commit hash still needs to be recorded before the library is added as a pinned dependency. Do not treat the signature as stable across arbitrary upstream revisions.

## Integration goal

Decoded PCM enters a bounded ring buffer; a dedicated real-time DSP task consumes fixed-size blocks and sends TDM output. Keep decoder/network work off the DSP task. Confirm licensing, ESP-IDF/Arduino compatibility, PSRAM needs and supported formats for the pinned revision before adding it as a dependency.
