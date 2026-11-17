#pragma once

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

/* response: core 0 only */
#define DSP_GRID_POINTS_DEFAULT 1054U /* 10 Hz to 20 kHz at ~1/96 octave */

dsp_err_t dsp_sos_eval(const dsp_sos_t *s, size_t n_sections, double fs_hz,
                       double f_hz, double *re, double *im);

dsp_err_t dsp_log_grid(double f_min_hz, double f_max_hz, double *out_freqs, size_t n_points);

#ifdef __cplusplus
}
#endif
