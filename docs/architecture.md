# Architecture

```text
Phone UI -- config upload --> xoweb --> dspgraph (inactive graph)
                                         | parse / validate / protection check
                                         v atomic swap at audio block boundary
Audio decoder --> audiosrc --> DSP task (core 1) --> tdmout --> 3x PCM5122
                         ring buffer
```

DSP blocks expose initialization, block processing and frequency-response evaluation. The graph is a directed acyclic graph (DAG), bounded to a configured maximum node count. Build and validate a new graph off the audio thread; swap only at a safe block boundary. Never allocate memory or parse JSON in the real-time audio callback.

Initial implementation should prioritize deterministic TDM output and safe mute/startup behavior before streaming integration.
