/* G2: float32 response and execution accuracy against a closed-form double
 * reference. The reference never touches the library's coefficients, so
 * coefficient quantisation is measured rather than inherited. */
#include "dspblocks.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define FS_HZ 48000.0
#define GATE_DB 0.01
/* Tones whose ideal level is below this are not float32-accurate by nature
 * (stopband noise floor, G4 covers it), so execution is checked above it only. */
#define EXEC_FLOOR_DB (-60.0)
#define F_MIN_HZ 20.0
#define F_MAX_HZ 20000.0
#define GRID_POINTS 400U
#define BLOCK_FRAMES 64U
#define SETTLE_S 2U
#define MEASURE_S 2U
#define TONE_AMP 0.5
#define PI_RAD 3.141592653589793
#define TWO_PI (2.0 * PI_RAD)

typedef struct { dsp_family_t fam; unsigned order; } shape_t;

static const shape_t SHAPES[] = {
    { DSP_FAMILY_BUTTERWORTH, 2U }, { DSP_FAMILY_BUTTERWORTH, 4U },
    { DSP_FAMILY_LINKWITZ_RILEY, 4U }, { DSP_FAMILY_LINKWITZ_RILEY, 8U },
};
static const double FC_HZ[] = { 70.0, 80.0, 120.0, 300.0, 2500.0 };
static const double TONE_HZ[] = { 20.0, 30.0, 50.0, 100.0, 1000.0 };
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

/* Bilinear-transformed Butterworth/LR magnitude in dB, exact in double.
 * BW N: |H|^2 = 1/(1+W^2N). LR N: |H| = 1/(1+W^N). W = tan(pi f/fs)/tan(pi fc/fs). */
static double ref_db(const shape_t *sh, dsp_mode_t mode, double fc, double f)
{
    double w = tan(PI_RAD * f / FS_HZ) / tan(PI_RAD * fc / FS_HZ);
    if (mode == DSP_MODE_HIGHPASS) w = 1.0 / w;
    if (sh->fam == DSP_FAMILY_BUTTERWORTH) {
        return -10.0 * log10(1.0 + pow(w, 2.0 * (double)sh->order));
    }
    return -20.0 * log10(1.0 + pow(w, (double)sh->order));
}

static double tone_level_db(const dsp_cascade_t *c, double f)
{
    dsp_state_t st[DSP_MAX_SOS];
    memset(st, 0, sizeof st);
    const size_t total = (size_t)(SETTLE_S + MEASURE_S) * (size_t)FS_HZ;
    const size_t settle = (size_t)SETTLE_S * (size_t)FS_HZ;
    double acc_c = 0.0, acc_s = 0.0;
    float in[BLOCK_FRAMES], out[BLOCK_FRAMES];
    for (size_t n0 = 0U; n0 < total; n0 += BLOCK_FRAMES) {
        for (size_t i = 0U; i < BLOCK_FRAMES; ++i) {
            in[i] = (float)(TONE_AMP * sin(TWO_PI * f * (double)(n0 + i) / FS_HZ));
        }
        dsp_sos_process(c->sos, st, c->n, in, out, BLOCK_FRAMES);
        for (size_t i = 0U; i < BLOCK_FRAMES; ++i) {
            if (n0 + i >= settle) {
                const double ph = TWO_PI * f * (double)(n0 + i) / FS_HZ;
                acc_c += (double)out[i] * cos(ph);
                acc_s += (double)out[i] * sin(ph);
            }
        }
    }
    const double amp = 2.0 * sqrt(acc_c * acc_c + acc_s * acc_s) / (double)(total - settle);
    return 20.0 * log10(amp / TONE_AMP);
}

int main(void)
{
    double worst_resp = 0.0, worst_exec = 0.0;
    int fails = 0;
    double grid[GRID_POINTS];
    if (dsp_log_grid(F_MIN_HZ, F_MAX_HZ, grid, GRID_POINTS) != DSP_OK) return 2;

    for (size_t s = 0U; s < COUNT(SHAPES); ++s)
    for (int m = 0; m < 2; ++m)
    for (size_t k = 0U; k < COUNT(FC_HZ); ++k) {
        const dsp_mode_t mode = (m == 0) ? DSP_MODE_LOWPASS : DSP_MODE_HIGHPASS;
        dsp_cascade_t c;
        if (dsp_design_xo(SHAPES[s].fam, mode, SHAPES[s].order, FC_HZ[k], FS_HZ, &c) != DSP_OK) return 2;

        double worst = 0.0, at = 0.0;
        for (size_t i = 0U; i < GRID_POINTS; ++i) {
            double re, im;
            if (dsp_sos_eval(c.sos, c.n, FS_HZ, grid[i], &re, &im) != DSP_OK) return 2;
            const double e = fabs(10.0 * log10(re * re + im * im) - ref_db(&SHAPES[s], mode, FC_HZ[k], grid[i]));
            if (e > worst) { worst = e; at = grid[i]; }
        }
        if (worst > worst_resp) worst_resp = worst;
        if (worst > GATE_DB) {
            printf("FAIL response %s%u %s fc=%.0f: %.4f dB at %.1f Hz (gate %.2f)\n",
                   SHAPES[s].fam == DSP_FAMILY_BUTTERWORTH ? "BW" : "LR", SHAPES[s].order,
                   m == 0 ? "LP" : "HP", FC_HZ[k], worst, at, GATE_DB);
            ++fails;
        }

        for (size_t t = 0U; t < COUNT(TONE_HZ); ++t) {
            const double want = ref_db(&SHAPES[s], mode, FC_HZ[k], TONE_HZ[t]);
            if (want < EXEC_FLOOR_DB) continue;
            const double e = fabs(tone_level_db(&c, TONE_HZ[t]) - want);
            if (e > worst_exec) worst_exec = e;
            if (e > GATE_DB) {
                printf("FAIL execution %s%u %s fc=%.0f tone %.0f Hz: %.4f dB (gate %.2f)\n",
                       SHAPES[s].fam == DSP_FAMILY_BUTTERWORTH ? "BW" : "LR", SHAPES[s].order,
                       m == 0 ? "LP" : "HP", FC_HZ[k], TONE_HZ[t], e, GATE_DB);
                ++fails;
            }
        }
    }
    printf("worst response error %.4f dB, worst execution error %.4f dB, gate %.2f dB, failures %d\n",
           worst_resp, worst_exec, GATE_DB, fails);
    return fails == 0 ? 0 : 1;
}
