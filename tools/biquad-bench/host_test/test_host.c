#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#include "../main/biquad_ref.h"
#include "../main/bench_util.h"

#include "../main/biquad_ref.c"
#include "../main/bench_util.c"

static int g_failures = 0;

static void expect_true(bool cond, const char *msg)
{
    if (cond) {
        printf("PASS: %s\n", msg);
    } else {
        printf("FAIL: %s\n", msg);
        g_failures++;
    }
}

static float mag_at_freq(const float *c, double freq, double fs)
{
    const double w = 2.0 * BIQUAD_PI * freq / fs;
    const double cw = cos(w);
    const double sw = sin(w);
    const double c2w = cos(2.0 * w);
    const double s2w = sin(2.0 * w);

    const double num_re = (double)c[0] + (double)c[1] * cw
                          + (double)c[2] * c2w;
    const double num_im = -((double)c[1] * sw + (double)c[2] * s2w);
    const double den_re = 1.0 + (double)c[3] * cw + (double)c[4] * c2w;
    const double den_im = -((double)c[3] * sw + (double)c[4] * s2w);

    const double num_mag = sqrt(num_re * num_re + num_im * num_im);
    const double den_mag = sqrt(den_re * den_re + den_im * den_im);
    return (float)(num_mag / den_mag);
}

static void df2_process(const float *in, float *out, const float *c,
                        float *state, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        const float x = in[i];
        const float y = c[0] * x + state[0];
        state[0] = c[1] * x - c[3] * y + state[1];
        state[1] = c[2] * x - c[4] * y;
        out[i] = y;
    }
}

static void test_lpf_gain_at_fc(void)
{
    const double fs = 48000.0;
    const double fc = 1000.0;
    const double q = 0.70710678118;
    float c[5];
    biquad_rbj_lpf(c, (float)fc, (float)fs, (float)q);

    const float mag = mag_at_freq(c, fc, fs);
    const double diff = fabs((double)mag - q);
    expect_true(diff < 1e-4, "LPF gain at fc equals Q");
}

static void test_lpf_dc_and_nyquist(void)
{
    const double fs = 48000.0;
    const double fc = 1000.0;
    const double q = 0.70710678118;
    float c[5];
    biquad_rbj_lpf(c, (float)fc, (float)fs, (float)q);

    const float dc = mag_at_freq(c, 0.0, fs);
    const float nyq = mag_at_freq(c, fs / 2.0, fs);

    expect_true(fabs((double)dc - 1.0) < 1e-5, "LPF DC gain is 1");
    expect_true(fabs((double)nyq) < 1e-5, "LPF Nyquist gain is 0");
}

static void test_hpf_mirrored(void)
{
    const double fs = 48000.0;
    const double fc = 1000.0;
    const double q = 0.70710678118;
    float c[5];
    biquad_rbj_hpf(c, (float)fc, (float)fs, (float)q);

    const float dc = mag_at_freq(c, 0.0, fs);
    const float nyq = mag_at_freq(c, fs / 2.0, fs);

    expect_true(fabs((double)dc) < 1e-5, "HPF DC gain is 0");
    expect_true(fabs((double)nyq - 1.0) < 1e-5, "HPF Nyquist gain is 1");
}

static void test_peaking_gain_at_fc(void)
{
    const double fs = 48000.0;
    const double fc = 1000.0;
    const double q = 1.0;
    const double gain_db = 6.0;
    float c[5];
    biquad_rbj_peaking(c, (float)fc, (float)fs, (float)q, (float)gain_db);

    const float mag = mag_at_freq(c, fc, fs);
    const double db = 20.0 * log10((double)mag);

    expect_true(fabs(db - gain_db) < 0.01,
                "Peaking gain at fc equals requested dB");
}

static void test_cascade_matches_df2(void)
{
    const size_t N = 256;
    const size_t stages = 8;
    float in[N];
    float out_tdf2[N];
    float out_df2[N];
    float coeffs[8 * 5];
    float state_tdf2[8 * 2] = {0};
    float state_df2[8 * 2] = {0};

    for (size_t i = 0; i < N; i++) {
        in[i] = sinf(2.0f * BIQUAD_PI * 1000.0f * (float)i / 48000.0f);
    }

    for (size_t s = 0; s < stages; s++) {
        biquad_rbj_peaking(coeffs + s * 5,
                           (float)(200 + s * 400), 48000.0f, 1.0f, 3.0f);
    }

    biquad_ref_cascade(in, out_tdf2, coeffs, state_tdf2, stages, N);

    memcpy(out_df2, in, N * sizeof(float));
    for (size_t s = 0; s < stages; s++) {
        df2_process(out_df2, out_df2, coeffs + s * 5, state_df2 + s * 2, N);
    }

    float max_diff = 0.0f;
    for (size_t i = 0; i < N; i++) {
        const float d = fabsf(out_tdf2[i] - out_df2[i]);
        if (d > max_diff) max_diff = d;
    }

    expect_true(max_diff < 1e-5f, "Cascade TD-FII equals DF-II within 1e-5");
}

static void test_stats(void)
{
    const size_t N = 100;
    uint32_t a[100];
    for (size_t i = 0; i < N; i++) {
        a[i] = (uint32_t)(i + 1);
    }

    bench_sort_u32(a, N);
    bench_stats_t st;
    bench_compute_stats(a, N, &st);

    expect_true(st.min == 1, "stats min");
    expect_true(st.max == 100, "stats max");
    expect_true(st.avg == 50, "stats avg");
    expect_true(st.p99 == 99, "stats p99 nearest-rank");
}

static void test_csv_format(void)
{
    bench_stats_t st = {.min = 100, .max = 200, .avg = 150, .p99 = 190};
    FILE *fp = tmpfile();
    if (!fp) {
        printf("FAIL: tmpfile\n");
        g_failures++;
        return;
    }

    fflush(stdout);
    const int saved = dup(fileno(stdout));
    dup2(fileno(fp), fileno(stdout));

    bench_print_csv_header();
    bench_print_csv_row("sweep", "ref_c", "internal", 6, 256, 8,
                        &st, 5333.333, 240.0);

    fflush(stdout);
    dup2(saved, fileno(stdout));
    close(saved);

    rewind(fp);
    char line[512];
    bool header_ok = false;
    bool row_ok = false;

    if (fgets(line, sizeof(line), fp)) {
        header_ok = (strncmp(line, "CSV,scenario,impl,mem,channels,block,",
                             strlen("CSV,scenario,impl,mem,channels,block,"))
                     == 0);
    }
    if (fgets(line, sizeof(line), fp)) {
        row_ok = (strncmp(line, "CSV,sweep,ref_c,internal,6,256,8,",
                          strlen("CSV,sweep,ref_c,internal,6,256,8,")) == 0);
    }

    fclose(fp);

    expect_true(header_ok, "CSV header format");
    expect_true(row_ok, "CSV row format");
}

int main(void)
{
    printf("=== Host tests ===\n");

    test_lpf_gain_at_fc();
    test_lpf_dc_and_nyquist();
    test_hpf_mirrored();
    test_peaking_gain_at_fc();
    test_cascade_matches_df2();
    test_stats();
    test_csv_format();

    printf("=== %s (%d failure%s) ===\n",
           g_failures == 0 ? "PASSED" : "FAILED",
           g_failures, g_failures == 1 ? "" : "s");

    return g_failures == 0 ? 0 : 1;
}