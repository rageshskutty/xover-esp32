# API contracts

Status: PROPOSED. Frozen at the review of task T06; after that, changes need an
ADR (AGENTS.md C5). Names are proposals; an agent may refine them in the task
that implements them, in the same PR, if the contract here is updated.

## 1. Error codes (shared by C and HTTP)
| code | meaning |
|---|---|
| E_JSON | not parseable, too deep, too large |
| E_SCHEMA | fails configs/schema.json (wrong type, unknown key, sample rate) |
| E_NODE_PARAM | missing or out-of-range node parameter |
| E_GRAPH_REF | duplicate id or edge to unknown id |
| E_GRAPH_CYCLE | graph has a cycle |
| E_GRAPH_FANIN | wrong number of incoming edges |
| E_GRAPH_DEAD | node without an outgoing edge, or no output |
| E_SLOT | duplicate or out-of-range slot or channel |
| E_VAR_UNDEFINED | @var not declared |
| E_LIMIT | variable outside its limits |
| E_PROTECT | response violates a device protection limit |
| E_RESOURCE | too many nodes or biquad sections |
| E_UNSUPPORTED | valid but not implemented (for example Bessel) |
| E_AUTH | missing or wrong token |
| E_BUSY | a swap or upload is in progress |

tools/check.py implements the config-level codes and is the reference for them.

## 2. C modules

### 2.1 dspblocks (non-RT design, RT process)
    typedef enum { DSP_OK = 0, DSP_E_PARAM, DSP_E_RANGE, DSP_E_UNSTABLE,
                   DSP_E_UNSUPPORTED, DSP_E_NOMEM } dsp_err_t;
    typedef struct { float b0, b1, b2, a1, a2; } dsp_sos_t;     /* a0 = 1 */
    typedef struct { float z1, z2; } dsp_state_t;
    #define DSP_MAX_SOS 4                                        /* 8th order */
    typedef struct { dsp_sos_t sos[DSP_MAX_SOS]; uint8_t n; } dsp_cascade_t;

    /* design: core 0 only, double precision inside */
    dsp_err_t dsp_design_xo(dsp_family_t, dsp_mode_t, unsigned order,
                            double fc_hz, double fs_hz, dsp_cascade_t *out);
    dsp_err_t dsp_design_peq(double f_hz, double q, double gain_db, double fs_hz, dsp_sos_t *out);
    dsp_err_t dsp_design_shelf(dsp_shelf_t, double f_hz, double gain_db, double fs_hz, dsp_sos_t *out);
    dsp_err_t dsp_design_allpass(double f_hz, double q, double fs_hz, dsp_sos_t *out);
    dsp_err_t dsp_design_notch(double f_hz, double q, double fs_hz, dsp_sos_t *out);
    dsp_err_t dsp_check_stable(const dsp_sos_t *s);

    /* process: RT-safe (file dsp_sos_rt.c); in == out allowed */
    void dsp_sos_process(const dsp_sos_t *s, dsp_state_t *st, size_t n_sections,
                         const float *in, float *out, size_t n_frames);

    /* response: core 0 only */
    #define DSP_MAX_EVAL_SECTIONS 96U
    dsp_err_t dsp_sos_eval(const dsp_sos_t *s, size_t n_sections, double fs_hz,
                           double f_hz, double *re, double *im);
    #define DSP_GRID_POINTS_DEFAULT 1054U
    #define DSP_GRID_POINTS_MAX 100000U
    dsp_err_t dsp_log_grid(double f_min_hz, double f_max_hz,
                           double *out_freqs, size_t n_points);

`dsp_log_grid` is core-0-only. It writes a strictly increasing logarithmic
grid with exact endpoints; both frequencies must be finite, positive, and
`f_max_hz > f_min_hz`. `n_points` must be in 2..`DSP_GRID_POINTS_MAX`. It
returns `DSP_E_RANGE` if the range cannot represent every point distinctly.
The default 1054-point grid spans 10 Hz to 20 kHz at approximately 1/96 octave.

### 2.2 dspgraph
    typedef struct dspg dspg_t;
    typedef struct { char code[20]; char path[48]; char msg[96]; } dspg_issue_t;
    typedef struct { dspg_issue_t items[8]; uint8_t n; } dspg_report_t;
    typedef struct { char out_id[16]; float max_gain_db;
                     float stop_below_hz, atten_below_db;
                     float stop_above_hz, atten_above_db; } dspg_protect_t;

    /* core 0. Parses, validates, designs filters, applies protection.
       On failure returns NULL and fills the report; never touches the live graph. */
    dspg_t *dspg_build(const char *json, size_t len,
                       const dspg_protect_t *protect, size_t n_protect,
                       dspg_report_t *report);
    int  dspg_set_var(dspg_t *g, const char *name, double value, dspg_report_t *r);
    int  dspg_eval(const dspg_t *g, const char *out_id, double f_hz, double *re, double *im);
    void dspg_free(dspg_t *g);

    /* engine: submit from core 0, consumed by the DSP task at a block boundary */
    int  dspg_engine_submit(dspg_t *g);                 /* E_BUSY if a swap is pending */
    /* RT (file dspg_rt.c): float planar buffers, n_frames <= 64 */
    void dspg_engine_process(const float *const in[2], float *const out[8], uint32_t n_frames);
    typedef struct { uint32_t blocks, underruns, clips, faults, max_cycles, p999_cycles; } dspg_stats_t;
    void dspg_engine_stats(dspg_stats_t *s);

### 2.3 audiosrc
    int  audiosrc_init(size_t ring_frames);             /* internal SRAM ring */
    /* called from the ESP32-audioI2S hook (core 0): copies, blocks when full */
    void audiosrc_push(const int32_t *interleaved_stereo, size_t n_frames);
    /* DSP task (core 1): returns frames read, zero-fills and counts an underrun on shortage */
    size_t audiosrc_pull(float *l, float *r, size_t n_frames);

### 2.4 tdmout
    typedef struct { int mclk, bclk, ws, dout; uint32_t fs; uint8_t slots; } tdmout_cfg_t;
    int  tdmout_init(const tdmout_cfg_t *cfg);          /* pins come from board_config.h only */
    int  tdmout_write(const int32_t *frames, size_t n_frames, uint32_t timeout_ms);
    void tdmout_mute(bool on);                          /* writes zeros, never stops clocks */

### 2.5 pcm5122
    typedef struct { uint8_t i2c_addr; uint8_t tdm_offset_bclk; } pcm5122_cfg_t;
    int pcm5122_init(const pcm5122_cfg_t *cfg);         /* values from board_config.h + datasheet */
    int pcm5122_mute(uint8_t i2c_addr, bool on);

## 3. Config file
Normative schema: configs/schema.json. Node parameter table: docs/design.md
section 4. Semantic rules and error codes: tools/check.py. T06 tightens the
schema so per-type parameters are validated there too.

## 4. HTTP API v1
Base `http://xover.local/api/v1`. Reads are open on the LAN. Writes need
`Authorization: Bearer <token>` (token in NVS). Write rate limit: 5 per minute.

| Method | Path | Body | Success | Errors |
|---|---|---|---|---|
| GET | /status | - | 200 {fw, fs, state, cfg_crc32, cpu_p999_pct, underruns, clips, faults, protect: "ok"/"missing"} | - |
| GET | /config | - | 200 the active JSON | 404 none |
| POST | /config | application/json <= 32 KB | 200 {ok:true, crc32} | 400 E_JSON, 401 E_AUTH, 409 E_BUSY, 413, 422 {ok:false, errors:[{code,path,msg}]} |
| PUT | /vars/{name} | {value:number} | 200 {ok:true, value} | 401, 404 E_VAR_UNDEFINED, 422 E_LIMIT or E_PROTECT |
| GET | /response?out=<id>&points=256&fmin=10&fmax=20000 | - | 200 {f:[], mag_db:[], phase_deg:[]} | 404, 422 |
| POST | /mute | {mute:bool} | 200 | 401 |
| GET | /limits | - | 200 the device protection records (read-only) | - |

Rules: a failed POST or PUT leaves the running config untouched. `/response`
is computed by the device from its real coefficients (`dspg_eval`), so the page
only plots. mDNS name `xover.local`. Max 4 concurrent connections.

## 5. NVS layout
| namespace | key | value |
|---|---|---|
| protect | ver | u8 = 1 |
| protect | <out_id> | blob dspg_protect_t |
| protect | crc | u32 over all records |
| net | token | string, <= 32 chars |
| net | wifi_ssid, wifi_pass | strings |
| cfg | active_slot | u8 (0 = xocfg_a, 1 = xocfg_b) |

Config JSON is stored in the `xocfg_a` / `xocfg_b` partitions (64 KB each) with a
header {magic, version, length, crc32}. The inactive slot is written first, then
`active_slot` flips; a bad CRC at boot falls back to the other slot.
