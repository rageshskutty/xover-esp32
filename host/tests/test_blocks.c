#include "dspblocks.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "FAIL: %s at %d\n", #cond, __LINE__); return false; } } while (0)

static bool test_delay(void)
{
    float buf[DSP_MAX_DELAY_SAMPLES], in[16] = {1.0f}, out[16] = {0.0f}, inout[8] = {2.5f};
    dsp_delay_t d;
    CHECK(dsp_delay_init(&d, buf, DSP_MAX_DELAY_SAMPLES, 5U) == DSP_OK);
    CHECK(dsp_delay_us_to_samples(20000.0, 48000.0) == 960U && dsp_delay_us_to_samples(0.0, 48000.0) == 0U);

    /* M4: delay conversion at 0, 1, 959, 960, 961 samples worth of microseconds */
    CHECK(dsp_delay_us_to_samples(0.0, 48000.0) == 0U);
    CHECK(dsp_delay_us_to_samples(20.833333333333332, 48000.0) == 1U);
    CHECK(dsp_delay_us_to_samples(19979.166666666668, 48000.0) == 959U);
    CHECK(dsp_delay_us_to_samples(20000.0, 48000.0) == 960U);
    CHECK(dsp_delay_us_to_samples(20020.833333333332, 48000.0) == 960U); /* clamps to 960 */
    CHECK(dsp_delay_us_to_samples(4294967296.0, 48000.0) == 960U);        /* UINT32-scale clamped */
    CHECK(dsp_delay_us_to_samples(1e300, 48000.0) == 960U);               /* huge double clamped */
    CHECK(dsp_delay_us_to_samples(-1.0, 48000.0) == 0U);
    CHECK(dsp_delay_us_to_samples(NAN, 48000.0) == 0U);
    CHECK(dsp_delay_us_to_samples(INFINITY, 48000.0) == 0U);

    dsp_delay_process(&d, in, out, 16U);
    for (size_t i = 0U; i < 5U; ++i) CHECK(out[i] == 0.0f);
    CHECK(out[5] == 1.0f);
    for (size_t i = 6U; i < 16U; ++i) CHECK(out[i] == 0.0f);

    /* In-place delay & reconfig */
    CHECK(dsp_delay_init(&d, buf, DSP_MAX_DELAY_SAMPLES, 3U) == DSP_OK);
    dsp_delay_process(&d, inout, inout, 8U);
    for (size_t i = 0U; i < 3U; ++i) CHECK(inout[i] == 0.0f);
    CHECK(inout[3] == 2.5f);
    CHECK(dsp_delay_set_delay(&d, 10U) == DSP_OK && dsp_delay_set_delay(&d, 1000U) == DSP_E_RANGE);
    CHECK(dsp_delay_init(NULL, buf, 10U, 1U) == DSP_E_PARAM && dsp_delay_init(&d, NULL, 10U, 1U) == DSP_E_PARAM);
    return true;
}

static bool test_gain_ramp(void)
{
    dsp_gain_t g;
    dsp_gain_init(&g, 0.0f);
    dsp_gain_set_target(&g, 1.0f, 960U); /* 20 ms ramp up */
    float in[64], out[64], prev = 0.0f;
    for (size_t i = 0U; i < 64U; ++i) in[i] = 1.0f;

    for (size_t b = 0U; b < 15U; ++b) {
        dsp_gain_process(&g, in, out, 64U);
        for (size_t i = 0U; i < 64U; ++i) { CHECK(out[i] >= prev); prev = out[i]; }
    }
    CHECK(g.current == 1.0f);

    /* 5 ms ramp down */
    dsp_gain_set_target(&g, 0.0f, 240U);
    for (size_t b = 0U; b < 5U; ++b) {
        dsp_gain_process(&g, in, out, 64U);
        for (size_t i = 0U; i < 64U; ++i) { CHECK(out[i] <= prev); prev = out[i]; }
    }
    CHECK(g.current == 0.0f);
    CHECK(fabsf(dsp_db_to_linear(0.0f) - 1.0f) < 1e-6f && fabsf(dsp_db_to_linear(-60.0f) - 0.001f) < 1e-6f);
    CHECK(dsp_db_to_linear(-130.0f) == 0.0f && fabsf(dsp_linear_to_db(1.0f)) < 1e-5f);
    return true;
}

static bool test_polarity_and_mix(void)
{
    float in[4] = {1.0f, -2.0f, 3.0f, -4.0f}, out[4] = {0.0f}, ch0[4] = {1.0f, 2.0f, 3.0f, 4.0f}, ch1[4] = {2.0f, 4.0f, 6.0f, 8.0f};
    dsp_polarity_process(true, in, out, 4U);
    CHECK(out[0] == -1.0f && out[1] == 2.0f && out[2] == -3.0f && out[3] == 4.0f);
    dsp_polarity_process(false, in, out, 4U);
    CHECK(out[0] == 1.0f && out[1] == -2.0f);

    const float *inputs[2] = {ch0, ch1}, gains[2] = {0.5f, 0.25f};
    dsp_mix_process(inputs, gains, 2U, out, 4U);
    CHECK(fabsf(out[0] - 1.0f) < 1e-6f && fabsf(out[1] - 2.0f) < 1e-6f);
    dsp_mix_process(inputs, gains, 2U, ch0, 4U);
    CHECK(fabsf(ch0[0] - 1.0f) < 1e-6f);
    return true;
}

static bool test_nan_inf_safety(void)
{
    float nan_val = (float)NAN, inf_val = (float)INFINITY;
    CHECK(!dsp_is_nan_or_inf(0.0f) && dsp_is_nan_or_inf(nan_val) && dsp_is_nan_or_inf(inf_val));

    float buf[4] = {1.0f, nan_val, 2.0f, 3.0f};
    CHECK(dsp_check_nan_inf(buf, 4U) && !dsp_sanitize_block(buf, 4U));
    for (size_t i = 0U; i < 4U; ++i) CHECK(buf[i] == 0.0f); /* Muted on fault */

    dsp_gain_t g; dsp_gain_init(&g, 1.0f);
    float in[4] = {nan_val, 1.0f, 2.0f, 3.0f}, out[4];
    dsp_gain_process(&g, in, out, 4U);
    CHECK(!dsp_sanitize_block(out, 4U));
    for (size_t i = 0U; i < 4U; ++i) CHECK(out[i] == 0.0f);

    dsp_sos_t s = {1.0f, 0.0f, 0.0f, -0.5f, 0.0f, 0.0f, 0.0f}; dsp_state_t st = {0.0f, 0.0f};
    dsp_sos_process(&s, &st, 1U, in, out, 4U);
    CHECK(!dsp_sanitize_block(out, 4U) && st.z1 == 0.0f && st.z2 == 0.0f);
    return true;
}

static bool test_denormal_10s_tail(void)
{
    dsp_cascade_t cas;
    CHECK(dsp_design_xo(DSP_FAMILY_BUTTERWORTH, DSP_MODE_LOWPASS, 4U, 100.0, 48000.0, &cas) == DSP_OK);
    dsp_state_t st[DSP_MAX_SOS] = {{0.0f, 0.0f}};
    float in[64] = {1.0f}, out[64] = {0.0f};

    for (size_t b = 0U; b < 7500U; ++b) {
        dsp_sos_process(cas.sos, st, cas.n, in, out, 64U);
        CHECK(dsp_sanitize_block(out, 64U));
        for (size_t i = 0U; i < 64U; ++i) {
            union { float f; uint32_t u; } p; p.f = out[i];
            CHECK(!((p.u & 0x7F800000U) == 0U && (p.u & 0x007FFFFFU) != 0U));
            CHECK(!dsp_is_nan_or_inf(p.f));
        }
        if (b == 0U) in[0] = 0.0f;
    }
    CHECK(out[63] == 0.0f);
    for (size_t sec = 0U; sec < cas.n; ++sec) CHECK(st[sec].z1 == 0.0f && st[sec].z2 == 0.0f);
    return true;
}

int main(void)
{
    bool ok = test_delay() && test_gain_ramp() && test_polarity_and_mix() &&
              test_nan_inf_safety() && test_denormal_10s_tail();
    if (!ok) {
        fprintf(stderr, "test_blocks FAILED\n");
        return 1;
    }
    printf("All T05 block tests passed.\n");
    return 0;
}
