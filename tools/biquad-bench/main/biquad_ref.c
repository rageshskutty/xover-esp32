#include "biquad_ref.h"
#include <math.h>
#include <string.h>

void biquad_ref_process(const float *input,
                        float *output,
                        const float *coeffs,
                        float *state,
                        size_t len)
{
    float z1 = state[0];
    float z2 = state[1];

    const float b0 = coeffs[0];
    const float b1 = coeffs[1];
    const float b2 = coeffs[2];
    const float a1 = coeffs[3];
    const float a2 = coeffs[4];

    for (size_t i = 0; i < len; i++) {
        const float x = input[i];
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        output[i] = y;
    }

    state[0] = z1;
    state[1] = z2;
}

void biquad_ref_cascade(const float *input,
                        float *output,
                        const float *coeffs,
                        float *state,
                        size_t num_stages,
                        size_t len)
{
    if (num_stages == 0) {
        memcpy(output, input, len * sizeof(float));
        return;
    }

    biquad_ref_process(input, output, coeffs, state, len);

    for (size_t s = 1; s < num_stages; s++) {
        biquad_ref_process(output, output,
                           coeffs + s * 5,
                           state + s * 2,
                           len);
    }
}

static void normalise_and_store(float *c,
                                double b0, double b1, double b2,
                                double a0, double a1, double a2)
{
    c[0] = (float)(b0 / a0);
    c[1] = (float)(b1 / a0);
    c[2] = (float)(b2 / a0);
    c[3] = (float)(a1 / a0);
    c[4] = (float)(a2 / a0);
}

void biquad_rbj_lpf(float *c, float fc, float fs, float q)
{
    const double w0 = 2.0 * BIQUAD_PI * (double)fc / (double)fs;
    const double cosw0 = cos(w0);
    const double alpha = sin(w0) / (2.0 * (double)q);

    const double b0 = (1.0 - cosw0) / 2.0;
    const double b1 = 1.0 - cosw0;
    const double b2 = (1.0 - cosw0) / 2.0;
    const double a0 = 1.0 + alpha;
    const double a1 = -2.0 * cosw0;
    const double a2 = 1.0 - alpha;

    normalise_and_store(c, b0, b1, b2, a0, a1, a2);
}

void biquad_rbj_hpf(float *c, float fc, float fs, float q)
{
    const double w0 = 2.0 * BIQUAD_PI * (double)fc / (double)fs;
    const double cosw0 = cos(w0);
    const double alpha = sin(w0) / (2.0 * (double)q);

    const double b0 = (1.0 + cosw0) / 2.0;
    const double b1 = -(1.0 + cosw0);
    const double b2 = (1.0 + cosw0) / 2.0;
    const double a0 = 1.0 + alpha;
    const double a1 = -2.0 * cosw0;
    const double a2 = 1.0 - alpha;

    normalise_and_store(c, b0, b1, b2, a0, a1, a2);
}

void biquad_rbj_peaking(float *c, float fc, float fs, float q,
                        float gain_db)
{
    const double A = pow(10.0, (double)gain_db / 40.0);
    const double w0 = 2.0 * BIQUAD_PI * (double)fc / (double)fs;
    const double cosw0 = cos(w0);
    const double alpha = sin(w0) / (2.0 * (double)q);

    const double b0 = 1.0 + alpha * A;
    const double b1 = -2.0 * cosw0;
    const double b2 = 1.0 - alpha * A;
    const double a0 = 1.0 + alpha / A;
    const double a1 = -2.0 * cosw0;
    const double a2 = 1.0 - alpha / A;

    normalise_and_store(c, b0, b1, b2, a0, a1, a2);
}