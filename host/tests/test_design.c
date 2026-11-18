#include "dspblocks.h"
#include "golden/golden_vectors.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define TOL 1e-5f

static void eval_complex(const dsp_sos_t *s, size_t n, double f, double fs, double *re, double *im)
{
    double w = 2.0 * M_PI * f / fs, c1 = cos(w), s1 = sin(w), c2 = cos(2.0 * w), s2 = sin(2.0 * w);
    double tr = 1.0, ti = 0.0;
    for (size_t i = 0U; i < n; ++i) {
        double nr = (double)s[i].b0 + (double)s[i].b1 * c1 + (double)s[i].b2 * c2;
        double ni = -((double)s[i].b1 * s1 + (double)s[i].b2 * s2);
        double dr = 1.0 + (double)s[i].a1 * c1 + (double)s[i].a2 * c2;
        double di = -((double)s[i].a1 * s1 + (double)s[i].a2 * s2);
        double den = dr * dr + di * di, hr = (nr * dr + ni * di) / den, hi = (ni * dr - nr * di) / den;
        double ntr = tr * hr - ti * hi;
        ti = tr * hi + ti * hr;
        tr = ntr;
    }
    *re = tr; *im = ti;
}

static double eval_db(const dsp_sos_t *s, size_t n, double f, double fs)
{
    double re, im;
    eval_complex(s, n, f, fs, &re, &im);
    return 20.0 * log10(sqrt(re * re + im * im));
}

static bool test_golden(void)
{
    printf("Running golden vector tests (%u XO, %u EQ)...\n", GOLDEN_XO_COUNT, GOLDEN_EQ_COUNT);
    for (size_t i = 0U; i < GOLDEN_XO_COUNT; ++i) {
        const golden_xo_entry_t *g = &GOLDEN_XO_ENTRIES[i];
        dsp_cascade_t out;
        if (dsp_design_xo(g->family, g->mode, g->order, g->fc_hz, g->fs_hz, &out) != DSP_OK || out.n != g->n_sections)
            return false;
        for (uint8_t s = 0U; s < g->n_sections; ++s) {
            if (fabsf(out.sos[s].b0 - g->expected_sos[s].b0) > TOL ||
                fabsf(out.sos[s].b1 - g->expected_sos[s].b1) > TOL ||
                fabsf(out.sos[s].b2 - g->expected_sos[s].b2) > TOL ||
                fabsf(out.sos[s].a1 - g->expected_sos[s].a1) > TOL ||
                fabsf(out.sos[s].a2 - g->expected_sos[s].a2) > TOL) return false;
        }
    }
    for (size_t i = 0U; i < GOLDEN_EQ_COUNT; ++i) {
        const golden_eq_entry_t *g = &GOLDEN_EQ_ENTRIES[i];
        dsp_sos_t out = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        dsp_err_t err = DSP_OK;
        if (strcmp(g->type, "peq") == 0) err = dsp_design_peq(g->f0_hz, g->q, g->gain_db, g->fs_hz, &out);
        else if (strcmp(g->type, "shelf") == 0) err = dsp_design_shelf((dsp_shelf_t)g->shape, g->f0_hz, g->gain_db, g->fs_hz, &out);
        else if (strcmp(g->type, "allpass") == 0) err = dsp_design_allpass(g->f0_hz, g->q, g->fs_hz, &out);
        else if (strcmp(g->type, "notch") == 0) err = dsp_design_notch(g->f0_hz, g->q, g->fs_hz, &out);
        if (err != DSP_OK || fabsf(out.b0 - g->expected_sos.b0) > TOL || fabsf(out.b1 - g->expected_sos.b1) > TOL ||
            fabsf(out.b2 - g->expected_sos.b2) > TOL || fabsf(out.a1 - g->expected_sos.a1) > TOL ||
            fabsf(out.a2 - g->expected_sos.a2) > TOL) return false;
    }
    printf("  PASS: all golden vectors match within %e\n", (double)TOL);
    return true;
}

static bool test_gate_g2(void)
{
    printf("Running Gate G2 analytic checks...\n");
    const double fc = 1000.0, fs = 48000.0;
    for (unsigned o = 1U; o <= 8U; ++o) {
        dsp_cascade_t lp, hp;
        if (dsp_design_xo(DSP_FAMILY_BUTTERWORTH, DSP_MODE_LOWPASS, o, fc, fs, &lp) != DSP_OK ||
            dsp_design_xo(DSP_FAMILY_BUTTERWORTH, DSP_MODE_HIGHPASS, o, fc, fs, &hp) != DSP_OK) return false;
        if (fabs(eval_db(lp.sos, lp.n, fc, fs) - (-3.0103)) > 0.005 ||
            fabs(eval_db(hp.sos, hp.n, fc, fs) - (-3.0103)) > 0.005) return false;
    }
    dsp_cascade_t lr4_lp, lr4_hp, lr8_lp, lr8_hp;
    dsp_design_xo(DSP_FAMILY_LINKWITZ_RILEY, DSP_MODE_LOWPASS, 4U, fc, fs, &lr4_lp);
    dsp_design_xo(DSP_FAMILY_LINKWITZ_RILEY, DSP_MODE_HIGHPASS, 4U, fc, fs, &lr4_hp);
    dsp_design_xo(DSP_FAMILY_LINKWITZ_RILEY, DSP_MODE_LOWPASS, 8U, fc, fs, &lr8_lp);
    dsp_design_xo(DSP_FAMILY_LINKWITZ_RILEY, DSP_MODE_HIGHPASS, 8U, fc, fs, &lr8_hp);
    if (fabs(eval_db(lr4_lp.sos, lr4_lp.n, fc, fs) - (-6.0206)) > 0.005) return false;

    for (double f = 10.0; f <= 20000.0; f *= 1.5) {
        double r1, i1, r2, i2;
        eval_complex(lr4_lp.sos, lr4_lp.n, f, fs, &r1, &i1); eval_complex(lr4_hp.sos, lr4_hp.n, f, fs, &r2, &i2);
        if (fabs(20.0 * log10(sqrt((r1+r2)*(r1+r2) + (i1+i2)*(i1+i2)))) > 0.001) return false;
        eval_complex(lr8_lp.sos, lr8_lp.n, f, fs, &r1, &i1); eval_complex(lr8_hp.sos, lr8_hp.n, f, fs, &r2, &i2);
        if (fabs(20.0 * log10(sqrt((r1+r2)*(r1+r2) + (i1+i2)*(i1+i2)))) > 0.001) return false;
    }
    printf("  PASS: BW -3.01 dB, LR4 -6.02 dB, and |LP+HP|=0 dB verified\n");
    return true;
}

static bool test_validation(void)
{
    printf("Running validation & stability checks...\n");
    dsp_cascade_t c; dsp_sos_t s;
    if (dsp_design_xo(DSP_FAMILY_BESSEL, DSP_MODE_LOWPASS, 4U, 1000.0, 48000.0, &c) != DSP_E_UNSUPPORTED) return false;
    if (dsp_design_xo(DSP_FAMILY_BUTTERWORTH, DSP_MODE_LOWPASS, 2U, 5.0, 48000.0, &c) != DSP_E_RANGE) return false;
    if (dsp_design_xo(DSP_FAMILY_BUTTERWORTH, DSP_MODE_LOWPASS, 2U, 23000.0, 48000.0, &c) != DSP_E_RANGE) return false;
    if (dsp_design_xo(DSP_FAMILY_BUTTERWORTH, DSP_MODE_LOWPASS, 9U, 1000.0, 48000.0, &c) != DSP_E_PARAM) return false;
    if (dsp_design_xo(DSP_FAMILY_LINKWITZ_RILEY, DSP_MODE_LOWPASS, 3U, 1000.0, 48000.0, &c) != DSP_E_PARAM) return false;
    if (dsp_design_peq(1000.0, 0.05, 0.0, 48000.0, &s) != DSP_E_RANGE) return false;
    if (dsp_design_peq(1000.0, 1.0, 15.0, 48000.0, &s) != DSP_E_RANGE) return false;
    if (dsp_design_xo(DSP_FAMILY_BUTTERWORTH, DSP_MODE_LOWPASS, 2U, 1000.0, 48000.0, NULL) != DSP_E_PARAM) return false;
    if (dsp_check_stable(NULL) != DSP_E_PARAM) return false;
    dsp_sos_t ok = { 0.5f, 0.25f, 0.125f, -0.5f, 0.25f, 0.0f, 0.0f }, bad = { 0.5f, 0.25f, 0.125f, -0.5f, 1.05f, 0.0f, 0.0f };
    if (dsp_check_stable(&ok) != DSP_OK || dsp_check_stable(&bad) != DSP_E_UNSTABLE) return false;
    printf("  PASS: validation & stability passed\n");
    return true;
}

int main(void)
{
    printf("=== dspblocks test_design ===\n");
    if (!test_golden() || !test_gate_g2() || !test_validation()) {
        printf("FAILED\n");
        return 1;
    }
    printf("ALL TESTS PASSED\n");
    return 0;
}
