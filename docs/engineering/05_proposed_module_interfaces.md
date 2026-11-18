# Proposed module interfaces (review before implementation)

These are API proposals, not existing source contracts. They must be accepted in an ADR and copied into `docs/api.md` before any implementation. All status-returning APIs use module-specific errors; exact enum values must match the approved error table in docs/api.md. No hardware pin, address, slot offset, register value or protection limit is encoded here.

## audiosrc — fixed-capacity SPSC ring
```c
typedef struct audiosrc_ring audiosrc_ring_t;
typedef struct { uint32_t overrun_frames, underrun_frames; size_t readable_frames; } audiosrc_stats_t;
audiosrc_err_t audiosrc_ring_init(audiosrc_ring_t *r, void *storage,
    size_t storage_bytes, size_t capacity_frames, uint8_t channels);
audiosrc_err_t audiosrc_write(audiosrc_ring_t *r, const int32_t *interleaved,
    size_t frames, size_t *written_frames);
audiosrc_err_t audiosrc_read(audiosrc_ring_t *r, int32_t *interleaved,
    size_t requested_frames, size_t *read_frames);
void audiosrc_get_stats(const audiosrc_ring_t *r, audiosrc_stats_t *out);
```
Storage and struct memory are caller-owned, fixed for lifetime, never allocated by RT calls. One producer and one consumer only. Reads/writes are bounded and nonblocking; short count is explicit. DSP task may use a documented wait only if approved FreeRTOS primitive cannot block on a core-0 lock; resolve that against AGENTS R1 before implementation. Samples are signed 32-bit interleaved PCM, two channels, 48 kHz. Decoder callback adapts to this API on core 0. Ring capacity and overflow policy are measured/selected in T09; no guessed buffering duration.

## tdmout — eight-slot frames
```c
typedef struct tdmout tdmout_t;
typedef struct { int32_t slot[8]; } tdmout_frame_t;
tdmout_err_t tdmout_init(tdmout_t *out, const tdmout_config_t *cfg);
tdmout_err_t tdmout_start(tdmout_t *out);
tdmout_err_t tdmout_write_block(tdmout_t *out,
    const tdmout_frame_t *frames, size_t n_frames, uint32_t timeout_ticks);
tdmout_err_t tdmout_mute(tdmout_t *out, bool muted);
tdmout_err_t tdmout_stop(tdmout_t *out);
```
Opaque object and DMA storage are internal-SRAM-owned by component, constructed on core 0 before start; caller owns frame buffer for call duration. `write_block` is the only expected blocking I2S call, bounded by supplied timeout; allowed only from DSP task. Slot-to-DAC routing is config supplied from verified board data and must never be guessed. TDM format 8×32 at 48k is a design target pending D2/D5/hardware validation.

## pcm5122 — board-specific DAC control
```c
typedef struct pcm5122_bus pcm5122_bus_t;
pcm5122_err_t pcm5122_bus_init(pcm5122_bus_t *bus, const pcm5122_bus_config_t *cfg);
pcm5122_err_t pcm5122_probe_all(pcm5122_bus_t *bus, pcm5122_status_t *status);
pcm5122_err_t pcm5122_set_hardware_mute(pcm5122_bus_t *bus, bool muted);
pcm5122_err_t pcm5122_configure_tdm(pcm5122_bus_t *bus,
    const pcm5122_tdm_config_t *cfg);
```
Config structs and bus object are caller-owned for lifetime. Calls are core-0/control-plane only and may block on I2C with explicit timeout; never called in DSP process. Address count, mute GPIO, I2C pin mapping, register values and device modes must come from human board record/cited datasheet. Until then compilation must fail closed for unset required board macros.

## dspgraph — immutable compiled plan
Retain current `dspg_validate(json,len,report)`. Candidate API (not frozen):
```c
typedef struct dspg dspg_t;
typedef struct { void *memory; size_t bytes; size_t max_frames; } dspg_arena_t;
dspg_err_t dspg_measure(const char *json, size_t len, dspg_requirements_t *need,
    dspg_report_t *report); /* core 0; bounded parse/validate */
dspg_err_t dspg_build(const char *json, size_t len, dspg_arena_t arena,
    dspg_t **out, dspg_report_t *report); /* core 0; no ownership of arena */
dspg_err_t dspg_process(dspg_t *graph, const float *input[2],
    float *output[8], size_t frames); /* RT; bounded; graph state DSP-task-owned */
void dspg_destroy(dspg_t *graph); /* core 0 only after acknowledged quiescence */
```
Measurement is required to allocate exact caller arena before build. Graph and state memory live in internal SRAM, but API does not allocate. Build failure sets `*out=NULL`; process accepts only 1..configured max frames and no aliasing unless ADR explicitly permits. Destroy before quiescence is invalid. Graph publication/retirement is owned by runtime, not dspgraph.

## runtime / protection / web
Do not create one monolithic public runtime API before boot and swap state machine ADR. The API must expose boot/init, start, stop, request-swap, fault/status snapshot with generation and counters; request-swap owns candidate until accepted, active graph release only after audio-task acknowledgment. Protection check is read-only over parsed config plus read-only protection records and is called before graph publish. Web APIs are control-only: start/stop, authenticated config upload, read-only status/response. No endpoint can write protection values. Exact HTTP routes/status body must be ADR-reviewed before code.

## Error, concurrency and lifetime rules
- APIs returning errors must define null/length/range/timeout behavior and leave outputs either zeroed or unchanged as declared.
- Core-0 init/build/destruction is separate from core-1 processing. No mutex shared with DSP, no unbounded wait, no allocation in RT.
- Caller-owned buffers remain valid until synchronous call returns; asynchronous ownership transfer must state exact acknowledgment point.
- No cross-task raw pointer mutation except immutable atomic publication with release/acquire ordering and safe retirement.
- All max sizes are compile-time or caller-declared and validated before entering RT code.
