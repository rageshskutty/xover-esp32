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

/* process: RT-safe (file dsp_sos_rt.c); in == out allowed */
void dsp_sos_process(const dsp_sos_t *s, dsp_state_t *st, size_t n_sections,
                     const float *in, float *out, size_t n_frames);

#ifdef __cplusplus
}
#endif
