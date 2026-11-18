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
        /* Pole offsets from z = 1: a1 = -2 + p, a2 = 1 - q. The subtractions are
         * exact when a1, a2 sit near -2 and 1 (Sterbenz), so p and q keep full
         * float32 precision where a1 and a2 themselves would lose it. */
        const float p = (s[sec].a1 + 2.0f) + s[sec].da1;
        const float q = (1.0f - s[sec].a2) - s[sec].da2;

        float z1 = st[sec].z1;
        float z2 = st[sec].z2;

        for (size_t i = 0U; i < n_frames; ++i) {
            const float x = src[i];
            const float y = b0 * x + z1;
            z1 = ((b1 * x + (y + y)) - p * y) + z2;
            z2 = (b2 * x - y) + q * y;
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

