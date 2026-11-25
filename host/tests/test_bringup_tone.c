#include "bringup_tone.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

#define N 48000U
#define PI 3.14159265358979323846

static int32_t buf[BRINGUP_TONE_CHANNELS][N];
static int32_t buf2[BRINGUP_TONE_CHANNELS][N];

static void ptrs(int32_t (*b)[N], int32_t *p[BRINGUP_TONE_CHANNELS], size_t off)
{
    for (size_t ch = 0U; ch < BRINGUP_TONE_CHANNELS; ch++) p[ch] = &b[ch][off];
}

/* Amplitude of the component at f_hz over N samples (whole cycles), in LSB. */
static double tone_amp(const int32_t *x, uint32_t f_hz)
{
    double re = 0.0, im = 0.0;
    for (uint32_t n = 0U; n < N; n++) {
        const double ph = 2.0 * PI * (double)((uint64_t)n * f_hz % N) / (double)N;
        re += (double)x[n] * cos(ph);
        im += (double)x[n] * sin(ph);
    }
    return 2.0 * sqrt(re * re + im * im) / (double)N;
}

int main(void)
{
    int32_t *p[BRINGUP_TONE_CHANNELS];
    static const uint32_t want_hz[BRINGUP_TONE_CHANNELS] = {400U, 700U, 1100U, 1700U, 2300U, 3100U};
    const double want_amp = pow(10.0, -30.0 / 20.0) * 2147483647.0;

    for (size_t ch = 0U; ch < BRINGUP_TONE_CHANNELS; ch++) CHECK(bringup_tone_freq_hz[ch] == want_hz[ch]);

    /* One second at -30 dBFS: each channel has the right level and no other tone. */
    ptrs(buf, p, 0U);
    CHECK(bringup_tone_fill(-30.0f, BRINGUP_TONE_MAX_DBFS, 0U, N, p) == 0);
    for (size_t ch = 0U; ch < BRINGUP_TONE_CHANNELS; ch++) {
        const double own = tone_amp(buf[ch], want_hz[ch]);
        CHECK(fabs(20.0 * log10(own / want_amp)) <= 0.001);
        for (size_t other = 0U; other < BRINGUP_TONE_CHANNELS; other++) {
            if (other == ch) continue;
            CHECK(tone_amp(buf[ch], want_hz[other]) <= own * 1e-5);
        }
        for (uint32_t n = 0U; n < N; n++) CHECK(fabs((double)buf[ch][n]) <= want_amp * (1.0 + 1e-6));
    }

    /* Splitting a block into calls gives bit-identical samples. */
    ptrs(buf2, p, 0U);
    CHECK(bringup_tone_fill(-30.0f, BRINGUP_TONE_MAX_DBFS, 0U, 100U, p) == 0);
    ptrs(buf2, p, 100U);
    CHECK(bringup_tone_fill(-30.0f, BRINGUP_TONE_MAX_DBFS, 100U, 19900U, p) == 0);
    ptrs(buf2, p, 20000U);
    CHECK(bringup_tone_fill(-30.0f, BRINGUP_TONE_MAX_DBFS, 20000U, 28000U, p) == 0);
    CHECK(memcmp(buf, buf2, sizeof buf) == 0);

    /* The signal repeats every 48000 frames; start_frame is not wrapped before use. */
    ptrs(buf2, p, 0U);
    CHECK(bringup_tone_fill(-30.0f, BRINGUP_TONE_MAX_DBFS, 480000U, N, p) == 0);
    CHECK(memcmp(buf, buf2, sizeof buf) == 0);
    ptrs(buf2, p, 0U);
    CHECK(bringup_tone_fill(-30.0f, BRINGUP_TONE_MAX_DBFS, UINT32_MAX - 1U, 4U, p) == 0);
    {
        int32_t ref[BRINGUP_TONE_CHANNELS][4];
        int32_t *rp[BRINGUP_TONE_CHANNELS];
        for (size_t ch = 0U; ch < BRINGUP_TONE_CHANNELS; ch++) rp[ch] = ref[ch];
        CHECK(bringup_tone_fill(-30.0f, BRINGUP_TONE_MAX_DBFS, 23294U, 4U, rp) == 0); /* (2^32 - 2) mod 48000 */
        for (size_t ch = 0U; ch < BRINGUP_TONE_CHANNELS; ch++)
            CHECK(memcmp(ref[ch], buf2[ch], 4U * sizeof(int32_t)) == 0);
    }

    /* A lower level scales the amplitude: -60 dBFS is 30 dB below -30 dBFS. */
    ptrs(buf2, p, 0U);
    CHECK(bringup_tone_fill(-60.0f, BRINGUP_TONE_MAX_DBFS, 0U, N, p) == 0);
    CHECK(fabs(20.0 * log10(tone_amp(buf2[0], 400U) / tone_amp(buf[0], 400U)) + 30.0) <= 0.001);

    /* Level gate: above -30 dBFS, below -120 dBFS and non-finite levels are refused, output untouched. */
    memset(buf2, 0x5A, sizeof buf2);
    ptrs(buf2, p, 0U);
    CHECK(bringup_tone_fill(-29.9f, BRINGUP_TONE_MAX_DBFS, 0U, 8U, p) == -1);
    CHECK(bringup_tone_fill(0.0f, BRINGUP_TONE_MAX_DBFS, 0U, 8U, p) == -1);
    CHECK(bringup_tone_fill(-120.1f, BRINGUP_TONE_MAX_DBFS, 0U, 8U, p) == -1);
    CHECK(bringup_tone_fill(NAN, BRINGUP_TONE_MAX_DBFS, 0U, 8U, p) == -1);
    CHECK(bringup_tone_fill(INFINITY, BRINGUP_TONE_MAX_DBFS, 0U, 8U, p) == -1);
    CHECK(bringup_tone_fill(-INFINITY, BRINGUP_TONE_MAX_DBFS, 0U, 8U, p) == -1);
    CHECK(bringup_tone_fill(-29.9f, BRINGUP_TONE_MAX_DBFS, 0U, 0U, p) == -1);
    for (size_t ch = 0U; ch < BRINGUP_TONE_CHANNELS; ch++)
        for (uint32_t n = 0U; n < 8U; n++) CHECK(buf2[ch][n] == 0x5A5A5A5A);
    CHECK(bringup_tone_fill(-120.0f, BRINGUP_TONE_MAX_DBFS, 0U, 8U, p) == 0);

    /* Caller cap: the effective cap is min(BRINGUP_TONE_MAX_DBFS, cap_dbfs); the hard limit always wins. */
    memset(buf2, 0x5A, sizeof buf2);
    ptrs(buf2, p, 0U);
    CHECK(bringup_tone_fill(-39.0f, -40.0f, 0U, 8U, p) == -1);
    for (size_t ch = 0U; ch < BRINGUP_TONE_CHANNELS; ch++)
        for (uint32_t n = 0U; n < 8U; n++) CHECK(buf2[ch][n] == 0x5A5A5A5A);
    CHECK(bringup_tone_fill(-40.0f, -40.0f, 0U, 8U, p) == 0);
    CHECK(bringup_tone_fill(-30.0f, -20.0f, 0U, 8U, p) == 0);
    CHECK(bringup_tone_fill(-29.0f, -20.0f, 0U, 8U, p) == -1);
    CHECK(bringup_tone_fill(-120.0f, -120.0f, 0U, 8U, p) == 0);
    CHECK(bringup_tone_fill(-120.0f, -121.0f, 0U, 8U, p) == -1);
    CHECK(bringup_tone_fill(-60.0f, NAN, 0U, 8U, p) == -1);
    CHECK(bringup_tone_fill(-60.0f, INFINITY, 0U, 8U, p) == -1);
    ptrs(buf2, p, 0U);
    CHECK(bringup_tone_fill(-40.0f, -40.0f, 0U, N, p) == 0);
    CHECK(fabs(tone_amp(buf2[0], 400U) / (pow(10.0, -40.0 / 20.0) * 2147483647.0) - 1.0) <= 0.01);

    /* Zero frames succeed without touching pointers; bad pointers fail. */
    CHECK(bringup_tone_fill(-30.0f, BRINGUP_TONE_MAX_DBFS, 0U, 0U, NULL) == 0);
    CHECK(bringup_tone_fill(-30.0f, BRINGUP_TONE_MAX_DBFS, 0U, 8U, NULL) == -1);
    ptrs(buf2, p, 0U);
    p[3] = NULL;
    CHECK(bringup_tone_fill(-30.0f, BRINGUP_TONE_MAX_DBFS, 0U, 8U, p) == -1);

    puts("PASS: bring-up tone generator");
    return 0;
}
