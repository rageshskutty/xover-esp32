#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DSP_OK = 0,
    DSP_E_PARAM,
    DSP_E_RANGE,
    DSP_E_UNSTABLE,
    DSP_E_UNSUPPORTED,
    DSP_E_NOMEM
} dsp_err_t;

typedef enum {
    DSP_FAMILY_BUTTERWORTH = 0,
    DSP_FAMILY_LINKWITZ_RILEY,
    DSP_FAMILY_BESSEL
} dsp_family_t;

typedef enum {
    DSP_MODE_LOWPASS = 0,
    DSP_MODE_HIGHPASS
} dsp_mode_t;

typedef enum {
    DSP_SHELF_LOW = 0,
    DSP_SHELF_HIGH
} dsp_shelf_t;

typedef struct {
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
} dsp_sos_t;     /* a0 = 1 */

typedef struct {
    float z1;
    float z2;
} dsp_state_t;

#define DSP_MAX_SOS 4U                                        /* 8th order */

typedef struct {
    dsp_sos_t sos[DSP_MAX_SOS];
    uint8_t n;
} dsp_cascade_t;

/* design: core 0 only, double precision inside */
dsp_err_t dsp_design_xo(dsp_family_t family, dsp_mode_t mode, unsigned order,
                        double fc_hz, double fs_hz, dsp_cascade_t *out);
dsp_err_t dsp_design_peq(double f_hz, double q, double gain_db, double fs_hz, dsp_sos_t *out);
dsp_err_t dsp_design_shelf(dsp_shelf_t shape, double f_hz, double gain_db, double fs_hz, dsp_sos_t *out);
dsp_err_t dsp_design_allpass(double f_hz, double q, double fs_hz, dsp_sos_t *out);
dsp_err_t dsp_design_notch(double f_hz, double q, double fs_hz, dsp_sos_t *out);
dsp_err_t dsp_check_stable(const dsp_sos_t *s);

/* process: RT-safe (file dsp_sos_rt.c); in == out allowed */
void dsp_sos_process(const dsp_sos_t *s, dsp_state_t *st, size_t n_sections,
                     const float *in, float *out, size_t n_frames);

/* Simple blocks and numeric safety (T05) */
typedef struct { float current; float target; float step; uint32_t remaining; } dsp_gain_t;
typedef struct { float *buffer; size_t capacity; size_t delay_samples; size_t write_pos; } dsp_delay_t;
#define DSP_MAX_DELAY_SAMPLES 960U /* 20 ms at 48 kHz */

/* Core 0 helpers */
float dsp_db_to_linear(float db);
float dsp_linear_to_db(float lin);
uint32_t dsp_delay_us_to_samples(double delay_us, double fs_hz);

/* Real-time block operations (blocks_rt.c) */
void dsp_gain_init(dsp_gain_t *g, float initial_gain);
void dsp_gain_set_target(dsp_gain_t *g, float target, uint32_t ramp_frames);
void dsp_gain_process(dsp_gain_t *g, const float *in, float *out, size_t n_frames);
void dsp_polarity_process(bool invert, const float *in, float *out, size_t n_frames);
dsp_err_t dsp_delay_init(dsp_delay_t *d, float *buffer, size_t capacity, size_t delay_samples);
dsp_err_t dsp_delay_set_delay(dsp_delay_t *d, size_t delay_samples);
void dsp_delay_process(dsp_delay_t *d, const float *in, float *out, size_t n_frames);
void dsp_mix_process(const float *const *inputs, const float *gains, size_t n_inputs,
                     float *out, size_t n_frames);

static inline bool dsp_is_nan_or_inf(float x)
{
    union { float f; uint32_t u; } p;
    p.f = x;
    return (p.u & 0x7F800000U) == 0x7F800000U;
}

static inline float dsp_flush_denormal(float x)
{
    union { float f; uint32_t u; } p;
    p.f = x;
    return ((p.u & 0x7F800000U) == 0U) ? 0.0f : x;
}

bool dsp_check_nan_inf(const float *buf, size_t n_frames);
void dsp_flush_denormals(float *buf, size_t n_frames);
bool dsp_sanitize_block(float *buf, size_t n_frames);

/* response: core 0 only */
#define DSP_MAX_EVAL_SECTIONS 96U
#define DSP_GRID_POINTS_DEFAULT 1054U /* 10 Hz to 20 kHz at ~1/96 octave */
#define DSP_GRID_POINTS_MAX 100000U

dsp_err_t dsp_sos_eval(const dsp_sos_t *s, size_t n_sections, double fs_hz,
                       double f_hz, double *re, double *im);

/* Core 0 only. Fills n_points strictly increasing logarithmic frequencies,
 * including both endpoints. Requires 2..DSP_GRID_POINTS_MAX points and
 * finite 0 < f_min_hz < f_max_hz. */
dsp_err_t dsp_log_grid(double f_min_hz, double f_max_hz, double *out_freqs, size_t n_points);

#ifdef __cplusplus
}
#endif
