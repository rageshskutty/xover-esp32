# ADR 0009: DSP timing measurement boundaries
- Status: Accepted for planning; thresholds remain provisional until hardware evidence
- Date: 2026-11-19

At 48 kHz and 64 frames the period is 1.333333 ms. Keep 64 frames for initial instrumentation. Measure separately with ESP32 cycle counter:
1. DSP execution: immediately before graph processing to after six active outputs; excludes ring wait, conversion and I2S.
2. Full block service: before ring read/conversion to after corresponding I2S write returns; includes conversion, graph, packing and blocking DMA.
3. Deadline margin: block period minus DSP execution; does not represent queueing latency or full service.

Record raw cycles and microseconds. Use measured CPU frequency; lock frequency or record per-sample frequency. Measure empty-interval instrumentation overhead and report it. Do not silently subtract it.

Existing p99.9 <=0.60 ms and max <=1.00 ms apply only to DSP execution (45% and 75% of period). Full service must meet actual next-block deadline; report p50/p99/p99.9/max and misses independently. Zero underruns over one hour with Wi-Fi is independent. Report warm-up separately, sample count, CPU/Wi-Fi, topology, per-output path SOS count, conversion, all distributions, misses, underruns, SRAM low-water and overhead. Only human bringup log can establish HW-VERIFIED. Never loosen gates after failure without measured evidence, ADR and owner approval.
