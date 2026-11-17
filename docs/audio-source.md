# Audio source integration

Candidate: https://github.com/schreibfaul1/ESP32-audioI2S

Do not assume its callback signature or sample format until the exact selected release is inspected. Integration goal: decoded PCM enters a bounded ring buffer; a dedicated real-time DSP task consumes fixed-size blocks and sends TDM output. Keep decoder/network work off the DSP task. Confirm licensing, ESP-IDF/Arduino compatibility, PSRAM needs and supported formats before adding it as a dependency.
