#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_psram.h"
#include "esp_idf_version.h"
#include "esp_system.h"
#include "esp_cpu.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "esp_dsp.h"
#include "biquad_ref.h"
#include "bench_util.h"

#define STR_(x) #x
#define STR(x) STR_(x)

// dsps_biquad_f32_aes3 is implemented in the esp-dsp component but is not
// declared in the public dsps_biquad.h header of esp-dsp 1.8.2.  The symbol
// exists in the component's assembly sources.  Declaring it here allows the
// benchmark to call it explicitly without modifying the component.
extern esp_err_t dsps_biquad_f32_aes3(const float *input, float *output,
                                       int len, float *coef, float *w);

#define BENCH_WARMUP_BLOCKS   20
#define BENCH_TIMED_BLOCKS    2000
#define BENCH_YIELD_INTERVAL  64
#define BENCH_PAD_FLOATS      4
#define BENCH_ALIGNMENT       16
#define BENCH_MAX_CHANNELS    6
#define BENCH_MAX_BLOCK       256
#define BENCH_MAX_STAGES      16
#define BENCH_SAMPLE_RATE     48000.0f
#define BENCH_LR_FC_HZ        1000.0f
#define BENCH_LR_Q            0.70710678118f

typedef enum {
    IMPL_AE32,
    IMPL_AES3,
    IMPL_ANSI,
    IMPL_REF_C,
} impl_t;

static const char *impl_name(impl_t impl)
{
    switch (impl) {
        case IMPL_AE32:  return "esp_dsp_ae32";
        case IMPL_AES3:  return "esp_dsp_aes3";
        case IMPL_ANSI:  return "esp_dsp_ansi";
        case IMPL_REF_C: return "ref_c";
        default:         return "unknown";
    }
}

typedef struct {
    float *in;
    float *work;
    float *coeffs;
    float *state;
    int block;
    int stages;
    bool in_psram;
} channel_buf_t;

typedef struct {
    channel_buf_t ch[BENCH_MAX_CHANNELS];
    int num_channels;
    int block;
    int stages;
} test_bufs_t;

static void *alloc_aligned(size_t size, bool psram)
{
    const uint32_t caps = psram
        ? MALLOC_CAP_SPIRAM
        : (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    return heap_caps_aligned_alloc(BENCH_ALIGNMENT, size, caps);
}

static bool bufs_init(test_bufs_t *b, int channels, int block, int stages,
                      bool psram)
{
    memset(b, 0, sizeof(*b));
    b->num_channels = channels;
    b->block = block;
    b->stages = stages;

    const size_t buf_floats = (size_t)(block + BENCH_PAD_FLOATS);
    const size_t buf_bytes = buf_floats * sizeof(float);

    for (int c = 0; c < channels; c++) {
        channel_buf_t *ch = &b->ch[c];
        ch->block = block;
        ch->stages = stages;
        ch->in_psram = psram;

        ch->in = (float *)alloc_aligned(buf_bytes, psram);
        ch->work = (float *)alloc_aligned(buf_bytes, psram);
        ch->coeffs = (float *)alloc_aligned((size_t)stages * 5 * sizeof(float),
                                           psram);
        ch->state = (float *)alloc_aligned((size_t)stages * 2 * sizeof(float),
                                          psram);

        if (!ch->in || !ch->work || !ch->coeffs || !ch->state) {
            return false;
        }

        memset(ch->in, 0, buf_bytes);
        memset(ch->work, 0, buf_bytes);
        memset(ch->state, 0, (size_t)stages * 2 * sizeof(float));
    }

    return true;
}

static void bufs_free(test_bufs_t *b)
{
    for (int c = 0; c < b->num_channels; c++) {
        heap_caps_free(b->ch[c].in);
        heap_caps_free(b->ch[c].work);
        heap_caps_free(b->ch[c].coeffs);
        heap_caps_free(b->ch[c].state);
    }
    memset(b, 0, sizeof(*b));
}

static uint32_t s_rng_state = 0x12345678u;

static float xorshift_float(void)
{
    uint32_t x = s_rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_rng_state = x;
    return ((float)(int32_t)x) * (1.0f / 2147483648.0f);
}

static void fill_signal(float *buf, int len, float sine_hz, bool noise)
{
    s_rng_state = 0x12345678u;
    for (int i = 0; i < len; i++) {
        const float t = (float)i / BENCH_SAMPLE_RATE;
        const float sine = sinf(2.0f * BIQUAD_PI * sine_hz * t);
        const float n = noise ? 0.01f * xorshift_float() : 0.0f;
        buf[i] = 0.5f * sine + n;
    }
}

static void gen_sweep_coeffs(channel_buf_t *ch, int stages)
{
    static const float freqs[BENCH_MAX_STAGES] = {
        200.0f, 400.0f, 800.0f, 1600.0f,
        3200.0f, 6400.0f, 12800.0f, 16000.0f,
        100.0f, 300.0f, 600.0f, 1200.0f,
        2400.0f, 4800.0f, 9600.0f, 14000.0f,
    };
    static const float gains[BENCH_MAX_STAGES] = {
        1.0f, -1.0f, 1.0f, -1.0f,
        1.0f, -1.0f, 1.0f, -1.0f,
        1.0f, -1.0f, 1.0f, -1.0f,
        1.0f, -1.0f, 1.0f, -1.0f,
    };

    for (int s = 0; s < stages; s++) {
        biquad_rbj_peaking(ch->coeffs + s * 5, freqs[s],
                           BENCH_SAMPLE_RATE, 1.0f, gains[s]);
    }
}

static void gen_realistic_coeffs(channel_buf_t *ch, int channel_idx,
                                 int eq_stages)
{
    if ((channel_idx % 2) == 0) {
        biquad_rbj_lpf(ch->coeffs + 0 * 5, BENCH_LR_FC_HZ,
                       BENCH_SAMPLE_RATE, BENCH_LR_Q);
        biquad_rbj_lpf(ch->coeffs + 1 * 5, BENCH_LR_FC_HZ,
                       BENCH_SAMPLE_RATE, BENCH_LR_Q);
    } else {
        biquad_rbj_hpf(ch->coeffs + 0 * 5, BENCH_LR_FC_HZ,
                       BENCH_SAMPLE_RATE, BENCH_LR_Q);
        biquad_rbj_hpf(ch->coeffs + 1 * 5, BENCH_LR_FC_HZ,
                       BENCH_SAMPLE_RATE, BENCH_LR_Q);
    }

    static const float eq_freqs[8] = {
        100.0f, 500.0f, 2000.0f, 5000.0f,
        8000.0f, 12000.0f, 15000.0f, 18000.0f,
    };
    static const float eq_gains[8] = {
        3.0f, -2.0f, 4.0f, -3.0f,
        2.0f, -4.0f, 3.0f, -2.0f,
    };

    for (int e = 0; e < eq_stages; e++) {
        biquad_rbj_peaking(ch->coeffs + (2 + e) * 5, eq_freqs[e],
                           BENCH_SAMPLE_RATE, 1.0f, eq_gains[e]);
    }
}

static esp_err_t run_kernel(impl_t impl,
                            const float *in, float *out, int len,
                            float *coef, float *w)
{
    switch (impl) {
        case IMPL_AE32:
            return dsps_biquad_f32_ae32(in, out, len, coef, w);
        case IMPL_AES3:
            return dsps_biquad_f32_aes3(in, out, len, coef, w);
        case IMPL_ANSI:
            return dsps_biquad_f32_ansi(in, out, len, coef, w);
        case IMPL_REF_C:
            biquad_ref_process(in, out, coef, w, (size_t)len);
            return ESP_OK;
        default:
            return ESP_ERR_INVALID_ARG;
    }
}

static esp_err_t run_cascade(impl_t impl, channel_buf_t *ch)
{
    const float *src = ch->in;
    float *dst = ch->work;

    for (int s = 0; s < ch->stages; s++) {
        const esp_err_t err = run_kernel(impl, src, dst, ch->block,
                                         ch->coeffs + s * 5,
                                         ch->state + s * 2);
        if (err != ESP_OK) {
            return err;
        }
        src = ch->work;
    }

    return ESP_OK;
}

static bool output_is_finite(const test_bufs_t *b)
{
    for (int c = 0; c < b->num_channels; c++) {
        for (int i = 0; i < b->block; i++) {
            if (!isfinite(b->ch[c].work[i])) {
                return false;
            }
        }
    }
    return true;
}

static bool time_test(impl_t impl,
                      test_bufs_t *b,
                      const char *scenario,
                      const char *mem_label,
                      int channels,
                      int block,
                      int stages)
{
    uint32_t *samples = (uint32_t *)heap_caps_malloc(
        (size_t)BENCH_TIMED_BLOCKS * sizeof(uint32_t),
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!samples) {
        ESP_LOGE("bench", "Failed to allocate samples array");
        return false;
    }

    for (int c = 0; c < channels; c++) {
        memset(b->ch[c].state, 0, (size_t)stages * 2 * sizeof(float));
    }

    for (int w = 0; w < BENCH_WARMUP_BLOCKS; w++) {
        for (int c = 0; c < channels; c++) {
            const esp_err_t err = run_cascade(impl, &b->ch[c]);
            if (err != ESP_OK) {
                ESP_LOGE("bench", "Warm-up kernel error %d", err);
                heap_caps_free(samples);
                return false;
            }
        }
    }

    for (int i = 0; i < BENCH_TIMED_BLOCKS; i++) {
        const uint32_t start = esp_cpu_get_cycle_count();

        for (int c = 0; c < channels; c++) {
            const esp_err_t err = run_cascade(impl, &b->ch[c]);
            if (err != ESP_OK) {
                ESP_LOGE("bench", "Timed kernel error %d", err);
                heap_caps_free(samples);
                return false;
            }
        }

        const uint32_t end = esp_cpu_get_cycle_count();
        samples[i] = end - start;

        __asm__ volatile("" ::: "memory");

        if (((i + 1) % BENCH_YIELD_INTERVAL) == 0
            && (i + 1) < BENCH_TIMED_BLOCKS) {
            vTaskDelay(1);

            for (int c = 0; c < channels; c++) {
                run_cascade(impl, &b->ch[c]);
            }
        }
    }

    const bool finite = output_is_finite(b);

    bench_sort_u32(samples, BENCH_TIMED_BLOCKS);
    bench_stats_t stats;
    bench_compute_stats(samples, BENCH_TIMED_BLOCKS, &stats);

    const double budget_us = (double)block / (double)BENCH_SAMPLE_RATE * 1e6;
    bench_print_csv_row(scenario, impl_name(impl), mem_label, channels, block,
                        stages, &stats, budget_us,
                        (double)CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);

    printf("  [%s] %s %s %dch %dsmp %dbiq "
           "avg=%" PRIu32 "cyc (%.1f%%) p99=%" PRIu32 "cyc\n",
           scenario, impl_name(impl), mem_label, channels, block, stages,
           stats.avg, 100.0 * (double)stats.avg
               / ((double)CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ * budget_us),
           stats.p99);

    heap_caps_free(samples);
    return finite;
}

static float biquad_mag_at_freq(const float *c, float freq, float fs)
{
    const double w = 2.0 * BIQUAD_PI * (double)freq / (double)fs;
    const double cw = cos(w);
    const double sw = sin(w);
    const double c2w = cos(2.0 * w);
    const double s2w = sin(2.0 * w);

    const double num_re = (double)c[0] + (double)c[1] * cw + (double)c[2] * c2w;
    const double num_im = -((double)c[1] * sw + (double)c[2] * s2w);
    const double den_re = 1.0 + (double)c[3] * cw + (double)c[4] * c2w;
    const double den_im = -((double)c[3] * sw + (double)c[4] * s2w);

    const double num_mag = sqrt(num_re * num_re + num_im * num_im);
    const double den_mag = sqrt(den_re * den_re + den_im * den_im);
    return (float)(num_mag / den_mag);
}

static bool check_coefficients(void)
{
    float esp_lpf[5], esp_hpf[5], ref_lpf[5], ref_hpf[5];

    const esp_err_t e1 = dsps_biquad_gen_lpf_f32(esp_lpf,
        BENCH_LR_FC_HZ / BENCH_SAMPLE_RATE, BENCH_LR_Q);
    const esp_err_t e2 = dsps_biquad_gen_hpf_f32(esp_hpf,
        BENCH_LR_FC_HZ / BENCH_SAMPLE_RATE, BENCH_LR_Q);

    biquad_rbj_lpf(ref_lpf, BENCH_LR_FC_HZ, BENCH_SAMPLE_RATE, BENCH_LR_Q);
    biquad_rbj_hpf(ref_hpf, BENCH_LR_FC_HZ, BENCH_SAMPLE_RATE, BENCH_LR_Q);

    if (e1 != ESP_OK || e2 != ESP_OK) {
        printf("CHECK coefficients: generator error -> FAIL\n");
        return false;
    }

    float max_diff = 0.0f;
    for (int i = 0; i < 5; i++) {
        const float d1 = fabsf(esp_lpf[i] - ref_lpf[i]);
        const float d2 = fabsf(esp_hpf[i] - ref_hpf[i]);
        if (d1 > max_diff) max_diff = d1;
        if (d2 > max_diff) max_diff = d2;
    }

    const bool pass = (max_diff < 1e-5f);
    printf("CHECK coefficients LPF/HPF: max_diff=%.10f -> %s\n",
           max_diff, pass ? "PASS" : "FAIL");
    return pass;
}

static bool check_single_stage_match(void)
{
    const int N = 256;
    float in[N], out_ref[N];
    float coeffs[5];
    float state[2] = {0};

    fill_signal(in, N, 1000.0f, false);
    biquad_rbj_lpf(coeffs, BENCH_LR_FC_HZ, BENCH_SAMPLE_RATE, BENCH_LR_Q);

    biquad_ref_process(in, out_ref, coeffs, state, N);

    const impl_t impls[] = {IMPL_AE32, IMPL_AES3, IMPL_ANSI};
    bool all_pass = true;

    for (size_t k = 0; k < sizeof(impls) / sizeof(impls[0]); k++) {
        float out[N];
        float st[2] = {0};

        const esp_err_t err = run_kernel(impls[k], in, out, N, coeffs, st);
        if (err != ESP_OK) {
            printf("CHECK single_stage %s: kernel error -> FAIL\n",
                   impl_name(impls[k]));
            all_pass = false;
            continue;
        }

        float max_diff = 0.0f;
        for (int i = 0; i < N; i++) {
            const float d = fabsf(out[i] - out_ref[i]);
            if (d > max_diff) max_diff = d;
        }

        const bool pass = (max_diff < 1e-5f);
        printf("CHECK single_stage %s: max_diff=%.10f -> %s\n",
               impl_name(impls[k]), max_diff, pass ? "PASS" : "FAIL");
        all_pass = all_pass && pass;
    }

    return all_pass;
}

static bool check_cascade_match(void)
{
    const int N = 256;
    const int stages = 10;

    test_bufs_t ref;
    if (!bufs_init(&ref, 1, N, stages, false)) {
        printf("CHECK cascade: allocation failed -> FAIL\n");
        return false;
    }

    fill_signal(ref.ch[0].in, N, 1000.0f, false);
    gen_realistic_coeffs(&ref.ch[0], 0, 8);

    run_cascade(IMPL_REF_C, &ref.ch[0]);

    float out_ref[N];
    memcpy(out_ref, ref.ch[0].work, (size_t)N * sizeof(float));

    const impl_t impls[] = {IMPL_AE32, IMPL_AES3, IMPL_ANSI};
    bool all_pass = true;

    for (size_t k = 0; k < sizeof(impls) / sizeof(impls[0]); k++) {
        test_bufs_t test;
        if (!bufs_init(&test, 1, N, stages, false)) {
            printf("CHECK cascade %s: allocation failed -> FAIL\n",
                   impl_name(impls[k]));
            all_pass = false;
            continue;
        }

        memcpy(test.ch[0].in, ref.ch[0].in, (size_t)N * sizeof(float));
        memcpy(test.ch[0].coeffs, ref.ch[0].coeffs,
               (size_t)stages * 5 * sizeof(float));

        run_cascade(impls[k], &test.ch[0]);

        float max_diff = 0.0f;
        for (int i = 0; i < N; i++) {
            const float d = fabsf(test.ch[0].work[i] - out_ref[i]);
            if (d > max_diff) max_diff = d;
        }

        const bool pass = (max_diff < 1e-4f);
        printf("CHECK cascade %s: max_diff=%.10f -> %s\n",
               impl_name(impls[k]), max_diff, pass ? "PASS" : "FAIL");
        all_pass = all_pass && pass;

        bufs_free(&test);
    }

    bufs_free(&ref);
    return all_pass;
}

static bool check_lpf_attenuation(void)
{
    const int settle = 576;
    const int window = 480;
    const int total = settle + window;

    float *in = (float *)heap_caps_malloc((size_t)total * sizeof(float),
                                          MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    float *out = (float *)heap_caps_malloc((size_t)total * sizeof(float),
                                           MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!in || !out) {
        heap_caps_free(in);
        heap_caps_free(out);
        printf("CHECK lpf_attenuation: allocation failed -> FAIL\n");
        return false;
    }

    fill_signal(in, total, 10000.0f, false);

    float coeffs[5];
    float state[2] = {0};
    biquad_rbj_lpf(coeffs, BENCH_LR_FC_HZ, BENCH_SAMPLE_RATE, BENCH_LR_Q);
    biquad_ref_process(in, out, coeffs, state, (size_t)total);

    double sum_in = 0.0;
    double sum_out = 0.0;
    for (int i = settle; i < total; i++) {
        sum_in += (double)in[i] * (double)in[i];
        sum_out += (double)out[i] * (double)out[i];
    }

    const double rms_in = sqrt(sum_in / (double)window);
    const double rms_out = sqrt(sum_out / (double)window);
    const double attenuation_db = 20.0 * log10(rms_in / rms_out);

    const double f = 10000.0;
    const double fc = (double)BENCH_LR_FC_HZ;
    const double fs = (double)BENCH_SAMPLE_RATE;
    const double ratio = tan(BIQUAD_PI * f / fs) / tan(BIQUAD_PI * fc / fs);
    const double analytic_db = 10.0 * log10(1.0 + pow(ratio, 4.0));

    const double tol_db = 0.5;
    const bool pass = fabs(attenuation_db - analytic_db) < tol_db;

    printf("CHECK lpf_attenuation: measured=%.2f dB analytic=%.2f dB -> %s\n",
           attenuation_db, analytic_db, pass ? "PASS" : "FAIL");

    heap_caps_free(in);
    heap_caps_free(out);
    return pass;
}

static bool run_correctness_checks(void)
{
    printf("=== Correctness checks ===\n");

    const bool c1 = check_coefficients();
    const bool c2 = check_single_stage_match();
    const bool c3 = check_cascade_match();
    const bool c4 = check_lpf_attenuation();

    const bool ok = c1 && c2 && c3 && c4;
    printf("=== Correctness %s ===\n\n", ok ? "PASSED" : "FAILED");
    return ok;
}

static void print_system_info(void)
{
    esp_chip_info_t info;
    esp_chip_info(&info);

    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);

    printf("\n=== Biquad Benchmark ===\n");
    printf("Chip:        ESP32-S3 (model=%d, rev=%d, cores=%d)\n",
           info.model, info.revision, info.cores);
    printf("CPU freq:    %d MHz\n", CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);
    printf("Flash:       %" PRIu32 " MB\n",
           flash_size / (1024u * 1024u));
    printf("PSRAM:       %s (%zu MB)\n",
           esp_psram_is_initialized() ? "yes" : "no",
           esp_psram_is_initialized()
               ? esp_psram_get_size() / (1024u * 1024u)
               : (size_t)0);
    printf("ESP-IDF:     %s\n", esp_get_idf_version());
    printf("DSPS_BIQUAD_F32 expands to: %s\n", STR(dsps_biquad_f32));
    printf("========================\n\n");
}

static bool run_matrix(void)
{
    const int blocks[] = {32, 64, 128, 256};
    const int stages[] = {1, 2, 4, 8, 12, 16};
    const int channels_6 = 6;
    const int channels_2 = 2;

    bool all_ok = true;

    for (size_t bi = 0; bi < sizeof(blocks) / sizeof(blocks[0]); bi++) {
        for (size_t si = 0; si < sizeof(stages) / sizeof(stages[0]); si++) {
            test_bufs_t b;
            if (!bufs_init(&b, channels_6, blocks[bi], stages[si], false)) {
                bufs_free(&b);
                return false;
            }

            for (int c = 0; c < channels_6; c++) {
                gen_sweep_coeffs(&b.ch[c], stages[si]);
                fill_signal(b.ch[c].in, blocks[bi], 1000.0f, true);
            }

            const impl_t impls[] = {IMPL_AE32, IMPL_AES3, IMPL_ANSI,
                                    IMPL_REF_C};
            for (size_t k = 0; k < sizeof(impls) / sizeof(impls[0]); k++) {
                if (!time_test(impls[k], &b, "sweep", "internal",
                               channels_6, blocks[bi], stages[si])) {
                    all_ok = false;
                }
            }

            bufs_free(&b);
        }
    }

    for (size_t bi = 0; bi < sizeof(blocks) / sizeof(blocks[0]); bi++) {
        for (size_t si = 0; si < sizeof(stages) / sizeof(stages[0]); si++) {
            test_bufs_t b;
            if (!bufs_init(&b, channels_2, blocks[bi], stages[si], false)) {
                bufs_free(&b);
                return false;
            }

            for (int c = 0; c < channels_2; c++) {
                gen_sweep_coeffs(&b.ch[c], stages[si]);
                fill_signal(b.ch[c].in, blocks[bi], 1000.0f, true);
            }

            const impl_t impls[] = {IMPL_AE32, IMPL_AES3, IMPL_ANSI,
                                    IMPL_REF_C};
            for (size_t k = 0; k < sizeof(impls) / sizeof(impls[0]); k++) {
                if (!time_test(impls[k], &b, "sweep", "internal",
                               channels_2, blocks[bi], stages[si])) {
                    all_ok = false;
                }
            }

            bufs_free(&b);
        }
    }

    if (esp_psram_is_initialized() && esp_psram_get_size() > 0) {
        test_bufs_t b;
        if (bufs_init(&b, channels_6, 256, 8, true)) {
            for (int c = 0; c < channels_6; c++) {
                gen_sweep_coeffs(&b.ch[c], 8);
                fill_signal(b.ch[c].in, 256, 1000.0f, true);
            }

            const impl_t impls[] = {IMPL_AE32, IMPL_AES3, IMPL_ANSI,
                                    IMPL_REF_C};
            for (size_t k = 0; k < sizeof(impls) / sizeof(impls[0]); k++) {
                if (!time_test(impls[k], &b, "psram_sweep", "psram",
                               channels_6, 256, 8)) {
                    all_ok = false;
                }
            }

            bufs_free(&b);
        } else {
            printf("  PSRAM allocation failed; skipping psram_sweep.\n");
        }
    } else {
        printf("  PSRAM unavailable; skipping psram_sweep.\n");
    }

    {
        const int eq_stages[] = {0, 4, 8};
        for (size_t ei = 0; ei < sizeof(eq_stages) / sizeof(eq_stages[0]);
             ei++) {
            const int total_stages = 2 + eq_stages[ei];

            test_bufs_t b;
            if (!bufs_init(&b, channels_6, 256, total_stages, false)) {
                bufs_free(&b);
                return false;
            }

            for (int c = 0; c < channels_6; c++) {
                gen_realistic_coeffs(&b.ch[c], c, eq_stages[ei]);
                fill_signal(b.ch[c].in, 256, 1000.0f, true);
            }

            const impl_t impls[] = {IMPL_AE32, IMPL_AES3, IMPL_ANSI,
                                    IMPL_REF_C};
            for (size_t k = 0; k < sizeof(impls) / sizeof(impls[0]); k++) {
                if (!time_test(impls[k], &b, "lr4_eq", "internal",
                               channels_6, 256, total_stages)) {
                    all_ok = false;
                }
            }

            bufs_free(&b);
        }
    }

    return all_ok;
}

static void bench_task(void *arg)
{
    (void)arg;

    print_system_info();

    if (!run_correctness_checks()) {
        printf("BENCH_STATUS,CHECKS_FAILED\n");
        printf("BENCH_DONE\n");
        vTaskDelete(NULL);
        return;
    }

    bench_print_csv_header();

    const bool ok = run_matrix();
    printf("BENCH_STATUS,%s\n", ok ? "OK" : "ERRORS");
    printf("BENCH_DONE\n");

    vTaskDelete(NULL);
}

void app_main(void)
{
    xTaskCreatePinnedToCore(bench_task, "bench", 8192, NULL,
                            configMAX_PRIORITIES - 3, NULL, 1);
}