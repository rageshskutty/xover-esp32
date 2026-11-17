#include "dspblocks.h"

void dsp_sos_process(const dsp_sos_t *s, dsp_state_t *st, size_t n_sections,
                     const float *in, float *out, size_t n_frames)
{
    if (in == NULL || out == NULL || n_frames == 0U) {
        return;
    }

    if (n_sections == 0U || s == NULL || st == NULL) {
        if (in != out) {
            for (size_t i = 0U; i < n_frames; ++i) {
                out[i] = in[i];
            }
        }
        return;
    }

    for (size_t sec = 0U; sec < n_sections; ++sec) {
        const float *src = (sec == 0U) ? in : out;
        float *dst = out;

        const float b0 = s[sec].b0;
        const float b1 = s[sec].b1;
        const float b2 = s[sec].b2;
        const float a1 = s[sec].a1;
        const float a2 = s[sec].a2;

        float z1 = st[sec].z1;
        float z2 = st[sec].z2;

        for (size_t i = 0U; i < n_frames; ++i) {
            const float x = src[i];
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            dst[i] = y;
        }

        if (dsp_is_nan_or_inf(z1) || dsp_is_nan_or_inf(z2)) {
            z1 = 0.0f;
            z2 = 0.0f;
        }
        st[sec].z1 = dsp_flush_denormal(z1);
        st[sec].z2 = dsp_flush_denormal(z2);
    }
}

