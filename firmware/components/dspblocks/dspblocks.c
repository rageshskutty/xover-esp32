#include "dspblocks.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

dsp_err_t dsp_check_stable(const dsp_sos_t *s)
{
    if (!s) return DSP_E_PARAM;
    if (isnan(s->b0) || isnan(s->b1) || isnan(s->b2) || isnan(s->a1) || isnan(s->a2) ||
        isinf(s->b0) || isinf(s->b1) || isinf(s->b2) || isinf(s->a1) || isinf(s->a2)) {
        return DSP_E_UNSTABLE;
    }
    return (fabsf(s->a2) < 1.0f && (1.0f + s->a1 + s->a2) > 0.0f && (1.0f - s->a1 + s->a2) > 0.0f)
        ? DSP_OK : DSP_E_UNSTABLE;
}

static dsp_sos_t make_biquad(dsp_mode_t mode, double fc, double q, double fs)
{
    double K = tan(M_PI * fc / fs);
    dsp_sos_t s;
    if (q <= 0.0) {
        double n = 1.0 + K;
        s.b0 = (float)((mode == DSP_MODE_LOWPASS ? K : 1.0) / n);
        s.b1 = (float)((mode == DSP_MODE_LOWPASS ? K : -1.0) / n);
        s.b2 = 0.0f;
        s.a1 = (float)((K - 1.0) / n);
        s.a2 = 0.0f;
    } else {
        double n = 1.0 + (K / q) + (K * K);
        s.b0 = (float)((mode == DSP_MODE_LOWPASS ? K * K : 1.0) / n);
        s.b1 = (float)((mode == DSP_MODE_LOWPASS ? 2.0 : -2.0) * (double)s.b0);
        s.b2 = s.b0;
        s.a1 = (float)(2.0 * (K * K - 1.0) / n);
        s.a2 = (float)((1.0 - (K / q) + (K * K)) / n);
    }
    return s;
}

dsp_err_t dsp_design_xo(dsp_family_t fam, dsp_mode_t mode, unsigned order, double fc, double fs, dsp_cascade_t *out)
{
    if (!out || fs <= 0.0) return DSP_E_PARAM;
    if (fc < 10.0 || fc > 0.45 * fs) return DSP_E_RANGE;
    if (mode != DSP_MODE_LOWPASS && mode != DSP_MODE_HIGHPASS) return DSP_E_PARAM;
    if (fam == DSP_FAMILY_BESSEL) return DSP_E_UNSUPPORTED;

    uint8_t n_sec = 0U;
    double q_list[DSP_MAX_SOS];

    if (fam == DSP_FAMILY_BUTTERWORTH) {
        if (order < 1U || order > 8U) return DSP_E_PARAM;
        n_sec = (uint8_t)((order + 1U) / 2U);
        if (order % 2U == 0U) {
            for (unsigned k = 1U; k <= order / 2U; ++k)
                q_list[k - 1U] = 1.0 / (2.0 * cos((double)(2U * k - 1U) * M_PI / (double)(2U * order)));
        } else {
            for (unsigned k = 1U; k <= (order - 1U) / 2U; ++k)
                q_list[k - 1U] = 1.0 / (2.0 * cos((double)k * M_PI / (double)order));
            q_list[n_sec - 1U] = -1.0;
        }
    } else if (fam == DSP_FAMILY_LINKWITZ_RILEY) {
        if (order != 2U && order != 4U && order != 6U && order != 8U) return DSP_E_PARAM;
        n_sec = (uint8_t)(order / 2U);
        if (order == 2U) q_list[0] = 0.5;
        else if (order == 4U) q_list[0] = q_list[1] = 1.0 / sqrt(2.0);
        else if (order == 6U) { q_list[0] = q_list[1] = 1.0; q_list[2] = 0.5; }
        else {
            q_list[0] = q_list[1] = 1.0 / (2.0 * cos(M_PI / 8.0));
            q_list[2] = q_list[3] = 1.0 / (2.0 * cos(3.0 * M_PI / 8.0));
        }
    } else return DSP_E_PARAM;

    out->n = n_sec;
    for (uint8_t i = 0U; i < n_sec; ++i) {
        out->sos[i] = make_biquad(mode, fc, q_list[i], fs);
        if (dsp_check_stable(&out->sos[i]) != DSP_OK) return DSP_E_UNSTABLE;
    }
    return DSP_OK;
}

dsp_err_t dsp_design_peq(double f, double q, double gain_db, double fs, dsp_sos_t *out)
{
    if (!out || fs <= 0.0) return DSP_E_PARAM;
    if (f < 10.0 || f > 0.45 * fs || q < 0.1 || q > 20.0 || gain_db < -24.0 || gain_db > 12.0) return DSP_E_RANGE;
    double w0 = 2.0 * M_PI * f / fs, a = sin(w0) / (2.0 * q), A = pow(10.0, gain_db / 40.0), c = cos(w0);
    double a0 = 1.0 + a / A;
    out->b0 = (float)((1.0 + a * A) / a0); out->b1 = (float)(-2.0 * c / a0); out->b2 = (float)((1.0 - a * A) / a0);
    out->a1 = out->b1; out->a2 = (float)((1.0 - a / A) / a0);
    return dsp_check_stable(out);
}

dsp_err_t dsp_design_shelf(dsp_shelf_t shape, double f, double gain_db, double fs, dsp_sos_t *out)
{
    if (!out || fs <= 0.0) return DSP_E_PARAM;
    if (shape != DSP_SHELF_LOW && shape != DSP_SHELF_HIGH) return DSP_E_PARAM;
    if (f < 10.0 || f > 0.45 * fs || gain_db < -24.0 || gain_db > 12.0) return DSP_E_RANGE;
    double w0 = 2.0 * M_PI * f / fs, A = pow(10.0, gain_db / 40.0), c = cos(w0), s = sin(w0) * sqrt(2.0 * A);
    double b0, b1, b2, a0, a1, a2;
    if (shape == DSP_SHELF_LOW) {
        b0 = A * ((A + 1.0) - (A - 1.0) * c + s); b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * c);
        b2 = A * ((A + 1.0) - (A - 1.0) * c - s); a0 = (A + 1.0) + (A - 1.0) * c + s;
        a1 = -2.0 * ((A - 1.0) + (A + 1.0) * c); a2 = (A + 1.0) + (A - 1.0) * c - s;
    } else {
        b0 = A * ((A + 1.0) + (A - 1.0) * c + s); b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * c);
        b2 = A * ((A + 1.0) + (A - 1.0) * c - s); a0 = (A + 1.0) - (A - 1.0) * c + s;
        a1 = 2.0 * ((A - 1.0) - (A + 1.0) * c); a2 = (A + 1.0) - (A - 1.0) * c - s;
    }
    out->b0 = (float)(b0 / a0); out->b1 = (float)(b1 / a0); out->b2 = (float)(b2 / a0);
    out->a1 = (float)(a1 / a0); out->a2 = (float)(a2 / a0);
    return dsp_check_stable(out);
}

dsp_err_t dsp_design_allpass(double f, double q, double fs, dsp_sos_t *out)
{
    if (!out || fs <= 0.0) return DSP_E_PARAM;
    if (f < 10.0 || f > 0.45 * fs || q < 0.1 || q > 20.0) return DSP_E_RANGE;
    double w0 = 2.0 * M_PI * f / fs, a = sin(w0) / (2.0 * q), a0 = 1.0 + a;
    out->b0 = (float)((1.0 - a) / a0); out->b1 = (float)(-2.0 * cos(w0) / a0); out->b2 = 1.0f;
    out->a1 = out->b1; out->a2 = out->b0;
    return dsp_check_stable(out);
}

dsp_err_t dsp_design_notch(double f, double q, double fs, dsp_sos_t *out)
{
    if (!out || fs <= 0.0) return DSP_E_PARAM;
    if (f < 10.0 || f > 0.45 * fs || q < 0.1 || q > 20.0) return DSP_E_RANGE;
    double w0 = 2.0 * M_PI * f / fs, a = sin(w0) / (2.0 * q), a0 = 1.0 + a;
    out->b0 = (float)(1.0 / a0); out->b1 = (float)(-2.0 * cos(w0) / a0); out->b2 = out->b0;
    out->a1 = out->b1; out->a2 = (float)((1.0 - a) / a0);
    return dsp_check_stable(out);
}

dsp_err_t dsp_sos_eval(const dsp_sos_t *s, size_t n_sections, double fs_hz,
                       double f_hz, double *re, double *im)
{
    if (!s || !re || !im) return DSP_E_PARAM;
    if (n_sections == 0U || n_sections > 96U) return DSP_E_PARAM;
    if (fs_hz <= 0.0 || isnan(fs_hz) || isinf(fs_hz)) return DSP_E_PARAM;
    if (f_hz < 0.0 || f_hz > 0.5 * fs_hz || isnan(f_hz) || isinf(f_hz)) return DSP_E_RANGE;

    const double w = 2.0 * M_PI * f_hz / fs_hz;
    const double c1 = cos(w);
    const double s1 = sin(w);
    const double c2 = cos(2.0 * w);
    const double s2 = sin(2.0 * w);

    double tr = 1.0;
    double ti = 0.0;

    for (size_t i = 0U; i < n_sections; ++i) {
        if (isnan(s[i].b0) || isnan(s[i].b1) || isnan(s[i].b2) ||
            isnan(s[i].a1) || isnan(s[i].a2) ||
            isinf(s[i].b0) || isinf(s[i].b1) || isinf(s[i].b2) ||
            isinf(s[i].a1) || isinf(s[i].a2)) {
            return DSP_E_UNSTABLE;
        }

        const double b0 = (double)s[i].b0;
        const double b1 = (double)s[i].b1;
        const double b2 = (double)s[i].b2;
        const double a1 = (double)s[i].a1;
        const double a2 = (double)s[i].a2;

        const double nr = b0 + b1 * c1 + b2 * c2;
        const double ni = -(b1 * s1 + b2 * s2);
        const double dr = 1.0 + a1 * c1 + a2 * c2;
        const double di = -(a1 * s1 + a2 * s2);

        const double den = dr * dr + di * di;
        if (den <= 1e-30 || isnan(den)) {
            return DSP_E_UNSTABLE;
        }

        const double hr = (nr * dr + ni * di) / den;
        const double hi = (ni * dr - nr * di) / den;

        const double ntr = tr * hr - ti * hi;
        const double nti = tr * hi + ti * hr;
        tr = ntr;
        ti = nti;
    }

    if (isnan(tr) || isnan(ti) || isinf(tr) || isinf(ti)) {
        return DSP_E_UNSTABLE;
    }

    *re = tr;
    *im = ti;
    return DSP_OK;
}

dsp_err_t dsp_log_grid(double f_min_hz, double f_max_hz, double *out_freqs, size_t n_points)
{
    if (!out_freqs || n_points < 2U || n_points > 100000U) return DSP_E_PARAM;
    if (f_min_hz <= 0.0 || isnan(f_min_hz) || isinf(f_min_hz)) return DSP_E_PARAM;
    if (f_max_hz <= f_min_hz || isnan(f_max_hz) || isinf(f_max_hz)) return DSP_E_PARAM;

    out_freqs[0] = f_min_hz;
    out_freqs[n_points - 1U] = f_max_hz;

    const double ratio = f_max_hz / f_min_hz;
    const double span = (double)(n_points - 1U);

    for (size_t i = 1U; i + 1U < n_points; ++i) {
        const double factor = (double)i / span;
        out_freqs[i] = f_min_hz * pow(ratio, factor);
    }

    return DSP_OK;
}

