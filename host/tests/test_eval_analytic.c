#include "dspblocks.h"
#include "golden/golden_vectors.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double wrap_deg(double deg)
{
    double rad = deg * (M_PI / 180.0);
    return atan2(sin(rad), cos(rad)) * (180.0 / M_PI);
}

/* 1. Single known biquad against an independently calculated complex response */
static bool test_single_biquad(void)
{
    printf("Running single biquad analytic check...\n");
    const dsp_sos_t s = { .b0 = 0.5f, .b1 = 0.2f, .b2 = 0.1f, .a1 = -0.3f, .a2 = 0.2f };
    const double fs = 48000.0;
    const double test_freqs[3] = { 100.0, 1000.0, 10000.0 };

    for (size_t i = 0U; i < 3U; ++i) {
        const double f = test_freqs[i];
        double re = 0.0, im = 0.0;
        if (dsp_sos_eval(&s, 1U, fs, f, &re, &im) != DSP_OK) {
            printf("  FAIL: dsp_sos_eval returned error for single biquad at %f Hz\n", f);
            return false;
        }

        const double w = 2.0 * M_PI * f / fs;
        const double c1 = cos(w), s1 = sin(w);
        const double c2 = cos(2.0 * w), s2 = sin(2.0 * w);
        const double b0 = (double)s.b0, b1 = (double)s.b1, b2 = (double)s.b2;
        const double a1 = (double)s.a1, a2 = (double)s.a2;
        const double nr = b0 + b1 * c1 + b2 * c2;
        const double ni = -(b1 * s1 + b2 * s2);
        const double dr = 1.0 + a1 * c1 + a2 * c2;
        const double di = -(a1 * s1 + a2 * s2);
        const double den = dr * dr + di * di;
        const double exp_re = (nr * dr + ni * di) / den;
        const double exp_im = (ni * dr - nr * di) / den;

        if (fabs(re - exp_re) > 1e-12 || fabs(im - exp_im) > 1e-12) {
            printf("  FAIL: single biquad mismatch at %f Hz: got (%e, %e), exp (%e, %e)\n",
                   f, re, im, exp_re, exp_im);
            return false;
        }
    }

    printf("  PASS: single biquad matches independent analytic calculation (< 1e-12)\n");
    return true;
}

/* 2. Multiple cascaded sections against the product of their individual responses */
static bool test_cascade_product(void)
{
    printf("Running cascade product check...\n");
    dsp_cascade_t cascade;
    if (dsp_design_xo(DSP_FAMILY_BUTTERWORTH, DSP_MODE_LOWPASS, 8U, 1000.0, 48000.0, &cascade) != DSP_OK) {
        printf("  FAIL: dsp_design_xo failed\n");
        return false;
    }

    const double fs = 48000.0;
    const double test_freqs[4] = { 50.0, 500.0, 1000.0, 5000.0 };

    for (size_t i = 0U; i < 4U; ++i) {
        const double f = test_freqs[i];

        /* Product of individual sections */
        double prod_re = 1.0, prod_im = 0.0;
        for (uint8_t s = 0U; s < cascade.n; ++s) {
            double r_s = 0.0, i_s = 0.0;
            if (dsp_sos_eval(&cascade.sos[s], 1U, fs, f, &r_s, &i_s) != DSP_OK) {
                printf("  FAIL: dsp_sos_eval failed for section %u\n", s);
                return false;
            }
            const double npr = prod_re * r_s - prod_im * i_s;
            const double npi = prod_re * i_s + prod_im * r_s;
            prod_re = npr;
            prod_im = npi;
        }

        /* Combined cascade evaluation */
        double cas_re = 0.0, cas_im = 0.0;
        if (dsp_sos_eval(cascade.sos, cascade.n, fs, f, &cas_re, &cas_im) != DSP_OK) {
            printf("  FAIL: dsp_sos_eval failed for full cascade\n");
            return false;
        }

        if (fabs(cas_re - prod_re) > 1e-12 || fabs(cas_im - prod_im) > 1e-12) {
            printf("  FAIL: cascade product mismatch at %f Hz: got (%e, %e), prod (%e, %e)\n",
                   f, cas_re, cas_im, prod_re, prod_im);
            return false;
        }
    }

    printf("  PASS: cascade evaluation exactly matches product of individual sections (< 1e-12)\n");
    return true;
}

/* 3. DC and representative frequency evaluation */
static bool test_dc_and_representative(void)
{
    printf("Running DC and representative frequency checks...\n");
    const double fs = 48000.0;
    const double fc = 1000.0;

    for (unsigned o = 1U; o <= 8U; ++o) {
        dsp_cascade_t lp, hp;
        if (dsp_design_xo(DSP_FAMILY_BUTTERWORTH, DSP_MODE_LOWPASS, o, fc, fs, &lp) != DSP_OK ||
            dsp_design_xo(DSP_FAMILY_BUTTERWORTH, DSP_MODE_HIGHPASS, o, fc, fs, &hp) != DSP_OK) {
            return false;
        }

        /* At DC (f = 0): lowpass is 1.0 (0 dB), highpass is 0.0 (-inf dB) */
        double re = 0.0, im = 0.0;
        if (dsp_sos_eval(lp.sos, lp.n, fs, 0.0, &re, &im) != DSP_OK) return false;
        if (fabs(re - 1.0) > 1e-4 || fabs(im) > 1e-12) {
            printf("  FAIL: LP DC response not 1.0: got (%f, %f)\n", re, im);
            return false;
        }

        if (dsp_sos_eval(hp.sos, hp.n, fs, 0.0, &re, &im) != DSP_OK) return false;
        const double hp_dc_mag = sqrt(re * re + im * im);
        if (hp_dc_mag > 1e-12) {
            printf("  FAIL: HP DC magnitude not zero: got %e\n", hp_dc_mag);
            return false;
        }

        /* At Nyquist (f = fs/2): highpass is 1.0 (0 dB), lowpass is 0.0 */
        if (dsp_sos_eval(hp.sos, hp.n, fs, 0.5 * fs, &re, &im) != DSP_OK) return false;
        if (fabs(re - 1.0) > 1e-4 || fabs(im) > 1e-12) {
            printf("  FAIL: HP Nyquist response not 1.0: got (%f, %f)\n", re, im);
            return false;
        }

        if (dsp_sos_eval(lp.sos, lp.n, fs, 0.5 * fs, &re, &im) != DSP_OK) return false;
        const double lp_nyq_mag = sqrt(re * re + im * im);
        if (lp_nyq_mag > 1e-12) {
            printf("  FAIL: LP Nyquist magnitude not zero: got %e\n", lp_nyq_mag);
            return false;
        }

        /* At fc: Butterworth magnitude is -3.0103 dB */
        if (dsp_sos_eval(lp.sos, lp.n, fs, fc, &re, &im) != DSP_OK) return false;
        const double lp_fc_db = 20.0 * log10(sqrt(re * re + im * im));
        if (fabs(lp_fc_db - (-3.0103)) > 0.005) {
            printf("  FAIL: BW LP fc magnitude error: %f dB\n", lp_fc_db);
            return false;
        }
    }

    /* Linkwitz-Riley 4th order: -6.0206 dB at fc */
    dsp_cascade_t lr4_lp;
    if (dsp_design_xo(DSP_FAMILY_LINKWITZ_RILEY, DSP_MODE_LOWPASS, 4U, fc, fs, &lr4_lp) != DSP_OK) return false;
    double re = 0.0, im = 0.0;
    if (dsp_sos_eval(lr4_lp.sos, lr4_lp.n, fs, fc, &re, &im) != DSP_OK) return false;
    const double lr4_fc_db = 20.0 * log10(sqrt(re * re + im * im));
    if (fabs(lr4_fc_db - (-6.0206)) > 0.005) {
        printf("  FAIL: LR4 fc magnitude error: %f dB\n", lr4_fc_db);
        return false;
    }

    printf("  PASS: DC, Nyquist, and cutoff evaluations mathematically verified\n");
    return true;
}

/* 4. Invalid pointers, invalid frequency and invalid section-count handling */
static bool test_validation(void)
{
    printf("Running error handling and parameter validation checks...\n");
    const dsp_sos_t valid_sos = { .b0 = 1.0f };
    double re = 0.0, im = 0.0;
    double freqs[10];

    /* dsp_sos_eval pointer checks */
    if (dsp_sos_eval(NULL, 1U, 48000.0, 1000.0, &re, &im) != DSP_E_PARAM) return false;
    if (dsp_sos_eval(&valid_sos, 1U, 48000.0, 1000.0, NULL, &im) != DSP_E_PARAM) return false;
    if (dsp_sos_eval(&valid_sos, 1U, 48000.0, 1000.0, &re, NULL) != DSP_E_PARAM) return false;

    /* dsp_sos_eval section count checks */
    if (dsp_sos_eval(&valid_sos, 0U, 48000.0, 1000.0, &re, &im) != DSP_E_PARAM) return false;
    if (dsp_sos_eval(&valid_sos, 97U, 48000.0, 1000.0, &re, &im) != DSP_E_PARAM) return false;

    /* dsp_sos_eval sample rate checks */
    if (dsp_sos_eval(&valid_sos, 1U, 0.0, 1000.0, &re, &im) != DSP_E_PARAM) return false;
    if (dsp_sos_eval(&valid_sos, 1U, -48000.0, 1000.0, &re, &im) != DSP_E_PARAM) return false;

    /* dsp_sos_eval frequency range checks */
    if (dsp_sos_eval(&valid_sos, 1U, 48000.0, -1.0, &re, &im) != DSP_E_RANGE) return false;
    if (dsp_sos_eval(&valid_sos, 1U, 48000.0, 24000.1, &re, &im) != DSP_E_RANGE) return false;

    /* Coefficients with NaN */
    const dsp_sos_t nan_sos = { .b0 = (float)NAN };
    if (dsp_sos_eval(&nan_sos, 1U, 48000.0, 1000.0, &re, &im) != DSP_E_UNSTABLE) return false;

    /* dsp_log_grid parameter validation */
    if (dsp_log_grid(10.0, 20000.0, NULL, 10U) != DSP_E_PARAM) return false;
    if (dsp_log_grid(10.0, 20000.0, freqs, 0U) != DSP_E_PARAM) return false;
    if (dsp_log_grid(10.0, 20000.0, freqs, 1U) != DSP_E_PARAM) return false;
    if (dsp_log_grid(0.0, 20000.0, freqs, 10U) != DSP_E_PARAM) return false;
    if (dsp_log_grid(-10.0, 20000.0, freqs, 10U) != DSP_E_PARAM) return false;
    if (dsp_log_grid(20000.0, 10.0, freqs, 10U) != DSP_E_PARAM) return false;
    if (dsp_log_grid(1000.0, 1000.0, freqs, 10U) != DSP_E_PARAM) return false;

    printf("  PASS: all invalid parameter tests returned appropriate error codes\n");
    return true;
}

/* 5. Log-grid endpoints, monotonicity and expected frequency coverage */
static bool test_log_grid(void)
{
    printf("Running logarithmic frequency-grid helper checks...\n");
    static double freqs[DSP_GRID_POINTS_DEFAULT];
    const size_t n = DSP_GRID_POINTS_DEFAULT;

    if (dsp_log_grid(10.0, 20000.0, freqs, n) != DSP_OK) {
        printf("  FAIL: dsp_log_grid failed for default grid\n");
        return false;
    }

    /* Check endpoints */
    if (freqs[0] != 10.0 || freqs[n - 1U] != 20000.0) {
        printf("  FAIL: endpoints mismatch: freqs[0]=%f, freqs[%zu]=%f\n", freqs[0], n - 1U, freqs[n - 1U]);
        return false;
    }

    /* Check monotonicity and constant step ratio */
    const double expected_ratio = pow(20000.0 / 10.0, 1.0 / (double)(n - 1U));
    for (size_t i = 0U; i + 1U < n; ++i) {
        if (freqs[i + 1U] <= freqs[i]) {
            printf("  FAIL: non-increasing frequency at index %zu: %f <= %f\n", i, freqs[i + 1U], freqs[i]);
            return false;
        }
        const double step_ratio = freqs[i + 1U] / freqs[i];
        if (fabs(step_ratio - expected_ratio) > 1e-12) {
            printf("  FAIL: step ratio error at index %zu: got %e, exp %e\n", i, step_ratio, expected_ratio);
            return false;
        }
    }

    /* Test small grid */
    double small_grid[5];
    if (dsp_log_grid(100.0, 1000.0, small_grid, 5U) != DSP_OK) return false;
    if (small_grid[0] != 100.0 || small_grid[4] != 1000.0) return false;
    for (size_t i = 0U; i < 4U; ++i) {
        if (small_grid[i + 1U] <= small_grid[i]) return false;
    }

    printf("  PASS: log grid endpoints exact, strictly monotonic, ratio constant (< 1e-12)\n");
    return true;
}

/* 6. Agreement with SciPy scipy.signal.sosfreqz across 3 filter families */
static bool test_scipy_golden(void)
{
    printf("Running SciPy golden vector comparison (BW, LR, RBJ EQ)...\n");
    double max_mag_err = 0.0;
    double max_phase_err = 0.0;

    /* Crossover filters: Butterworth and Linkwitz-Riley */
    for (size_t i = 0U; i < GOLDEN_XO_COUNT; ++i) {
        const golden_xo_entry_t *g = &GOLDEN_XO_ENTRIES[i];
        for (size_t k = 0U; k < GOLDEN_GRID_COUNT; ++k) {
            const double f = GOLDEN_GRID_HZ[k];
            double re = 0.0, im = 0.0;
            if (dsp_sos_eval(g->expected_sos, g->n_sections, g->fs_hz, f, &re, &im) != DSP_OK) {
                printf("  FAIL: dsp_sos_eval failed for XO entry %zu at %f Hz\n", i, f);
                return false;
            }

            const double mag = sqrt(re * re + im * im);
            const double exp_mag_db = g->expected_mag_db[k];
            const double exp_phase = g->expected_phase_deg[k];

            /* Handle zero / near-zero response explicitly */
            if (exp_mag_db < -100.0 || mag < 1e-6) {
                if (mag > 1e-5) {
                    printf("  FAIL: expected deep stopband rejection but got mag=%e (XO %zu at %f Hz)\n", mag, i, f);
                    return false;
                }
                continue;
            }

            const double mag_db = 20.0 * log10(mag);
            const double phase_deg = atan2(im, re) * (180.0 / M_PI);

            const double mag_err = fabs(mag_db - exp_mag_db);
            const double phase_err = fabs(wrap_deg(phase_deg - exp_phase));

            if (mag_err > max_mag_err) max_mag_err = mag_err;
            if (phase_err > max_phase_err) max_phase_err = phase_err;

            if (mag_err > 0.001) {
                printf("  FAIL: XO %zu at %f Hz mag error %f dB > 0.001 dB\n", i, f, mag_err);
                return false;
            }
            if (phase_err > 0.01) {
                printf("  FAIL: XO %zu at %f Hz phase error %f deg > 0.01 deg\n", i, f, phase_err);
                return false;
            }
        }
    }

    /* RBJ EQ filters: peq, shelf, allpass, notch */
    for (size_t i = 0U; i < GOLDEN_EQ_COUNT; ++i) {
        const golden_eq_entry_t *g = &GOLDEN_EQ_ENTRIES[i];
        for (size_t k = 0U; k < GOLDEN_GRID_COUNT; ++k) {
            const double f = GOLDEN_GRID_HZ[k];
            double re = 0.0, im = 0.0;
            if (dsp_sos_eval(&g->expected_sos, 1U, g->fs_hz, f, &re, &im) != DSP_OK) {
                printf("  FAIL: dsp_sos_eval failed for EQ entry %zu at %f Hz\n", i, f);
                return false;
            }

            const double mag = sqrt(re * re + im * im);
            const double exp_mag_db = g->expected_mag_db[k];
            const double exp_phase = g->expected_phase_deg[k];

            /* Handle zero / near-zero response explicitly (e.g. notch zero) */
            if (exp_mag_db < -100.0 || mag < 1e-6) {
                if (mag > 1e-5) {
                    printf("  FAIL: expected notch zero/rejection but got mag=%e (EQ %zu at %f Hz)\n", mag, i, f);
                    return false;
                }
                continue;
            }

            const double mag_db = 20.0 * log10(mag);
            const double phase_deg = atan2(im, re) * (180.0 / M_PI);

            const double mag_err = fabs(mag_db - exp_mag_db);
            const double phase_err = fabs(wrap_deg(phase_deg - exp_phase));

            if (mag_err > max_mag_err) max_mag_err = mag_err;
            if (phase_err > max_phase_err) max_phase_err = phase_err;

            if (mag_err > 0.001) {
                printf("  FAIL: EQ %zu (%s) at %f Hz mag error %f dB > 0.001 dB\n", i, g->type, f, mag_err);
                return false;
            }
            if (phase_err > 0.01) {
                printf("  FAIL: EQ %zu (%s) at %f Hz phase error %f deg > 0.01 deg\n", i, g->type, f, phase_err);
                return false;
            }
        }
    }

    printf("  PASS: all 3 filter families match SciPy sosfreqz (max mag err: %e dB, max phase err: %e deg)\n",
           max_mag_err, max_phase_err);
    return true;
}

int main(void)
{
    printf("=== dspblocks test_eval_analytic ===\n");
    if (!test_single_biquad() ||
        !test_cascade_product() ||
        !test_dc_and_representative() ||
        !test_validation() ||
        !test_log_grid() ||
        !test_scipy_golden()) {
        printf("FAILED\n");
        return 1;
    }
    printf("ALL TESTS PASSED\n");
    return 0;
}
