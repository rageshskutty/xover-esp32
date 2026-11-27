#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIQUAD_PI 3.14159265358979323846

void biquad_ref_process(const float *input,
                        float *output,
                        const float *coeffs,
                        float *state,
                        size_t len);

void biquad_ref_cascade(const float *input,
                        float *output,
                        const float *coeffs,
                        float *state,
                        size_t num_stages,
                        size_t len);

void biquad_rbj_lpf(float *coeffs, float fc, float fs, float q);
void biquad_rbj_hpf(float *coeffs, float fc, float fs, float q);
void biquad_rbj_peaking(float *coeffs, float fc, float fs, float q,
                        float gain_db);

#ifdef __cplusplus
}
#endif