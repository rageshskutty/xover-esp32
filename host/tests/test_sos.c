#include "dspblocks.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define TOLERANCE 1e-6f

static bool test_analytic_impulse_response(void)
{
    printf("Running test_analytic_impulse_response...\n");

    /* Known stable biquad:
     * H(z) = (0.5 + 0.25 z^-1 + 0.125 z^-2) / (1 - 0.5 z^-1 + 0.25 z^-2)
     * Direct Form II Transposed:
     * y[n]  = b0 * x[n] + z1[n-1]
     * z1[n] = b1 * x[n] - a1 * y[n] + z2[n-1]
     * z2[n] = b2 * x[n] - a2 * y[n]
     */
    const dsp_sos_t sos = {
        .b0 = 0.5f,
        .b1 = 0.25f,
        .b2 = 0.125f,
        .a1 = -0.5f,
        .a2 = 0.25f
    };

    dsp_state_t state = { .z1 = 0.0f, .z2 = 0.0f };

    /* Analytic hand-calculated impulse response:
     * n=0: y=0.5
     * n=1: y=0.5
     * n=2: y=0.25
     * n=3: y=0.0
     * n=4: y=-0.0625
     * n=5: y=-0.03125
     * n=6: y=0.0
     * n=7: y=0.0078125
     */
    const float expected[8] = {
        0.5f,
        0.5f,
        0.25f,
        0.0f,
        -0.0625f,
        -0.03125f,
        0.0f,
        0.0078125f
    };

    float in[8] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    float out[8] = { 0.0f };

    dsp_sos_process(&sos, &state, 1U, in, out, 8U);

    for (size_t i = 0U; i < 8U; ++i) {
        float diff = fabsf(out[i] - expected[i]);
        if (diff > TOLERANCE) {
            fprintf(stderr, "FAIL: step %zu: expected %f, got %f (diff %e > %e)\n",
                    i, (double)expected[i], (double)out[i], (double)diff, (double)TOLERANCE);
            return false;
        }
    }

    printf("  PASS: impulse response matches analytic values within %e\n", (double)TOLERANCE);
    return true;
}

static bool test_inplace_vs_outofplace(void)
{
    printf("Running test_inplace_vs_outofplace...\n");

    const dsp_sos_t cascade[2] = {
        { .b0 = 0.5f, .b1 = 0.25f, .b2 = 0.125f, .a1 = -0.5f, .a2 = 0.25f },
        { .b0 = 0.2f, .b1 = -0.1f, .b2 = 0.05f, .a1 = 0.3f, .a2 = -0.15f }
    };

    dsp_state_t state_oop[2] = {
        { .z1 = 0.0f, .z2 = 0.0f },
        { .z1 = 0.0f, .z2 = 0.0f }
    };
    dsp_state_t state_inp[2] = {
        { .z1 = 0.0f, .z2 = 0.0f },
        { .z1 = 0.0f, .z2 = 0.0f }
    };

    #define TEST_FRAMES 64U
    float in[TEST_FRAMES];
    float out_oop[TEST_FRAMES];
    float out_inp[TEST_FRAMES];

    /* Deterministic test signal */
    for (size_t i = 0U; i < TEST_FRAMES; ++i) {
        in[i] = ((float)(i % 11U) - 5.0f) / 10.0f;
        out_inp[i] = in[i];
    }

    dsp_sos_process(cascade, state_oop, 2U, in, out_oop, TEST_FRAMES);
    dsp_sos_process(cascade, state_inp, 2U, out_inp, out_inp, TEST_FRAMES);

    for (size_t i = 0U; i < TEST_FRAMES; ++i) {
        float diff = fabsf(out_oop[i] - out_inp[i]);
        if (diff > TOLERANCE) {
            fprintf(stderr, "FAIL: frame %zu: oop=%f, inp=%f (diff %e)\n",
                    i, (double)out_oop[i], (double)out_inp[i], (double)diff);
            return false;
        }
    }

    for (size_t sec = 0U; sec < 2U; ++sec) {
        float dz1 = fabsf(state_oop[sec].z1 - state_inp[sec].z1);
        float dz2 = fabsf(state_oop[sec].z2 - state_inp[sec].z2);
        if (dz1 > TOLERANCE || dz2 > TOLERANCE) {
            fprintf(stderr, "FAIL: final state section %zu mismatch: dz1=%e, dz2=%e\n",
                    sec, (double)dz1, (double)dz2);
            return false;
        }
    }

    printf("  PASS: in-place matches out-of-place identical\n");
    return true;
}

static bool test_block_continuity(void)
{
    printf("Running test_block_continuity...\n");

    const dsp_sos_t sos = {
        .b0 = 0.5f, .b1 = 0.25f, .b2 = 0.125f, .a1 = -0.5f, .a2 = 0.25f
    };

    dsp_state_t state_all = { .z1 = 0.0f, .z2 = 0.0f };
    dsp_state_t state_chunked = { .z1 = 0.0f, .z2 = 0.0f };

    #define CONT_FRAMES 128U
    float in[CONT_FRAMES];
    float out_all[CONT_FRAMES];
    float out_chunked[CONT_FRAMES];

    for (size_t i = 0U; i < CONT_FRAMES; ++i) {
        in[i] = ((float)(i % 13U) - 6.0f) / 10.0f;
    }

    /* Process all at once */
    dsp_sos_process(&sos, &state_all, 1U, in, out_all, CONT_FRAMES);

    /* Process in arbitrary chunk sizes: 17, 31, 16, 64 */
    const size_t chunks[] = { 17U, 31U, 16U, 64U };
    size_t offset = 0U;
    for (size_t c = 0U; c < 4U; ++c) {
        size_t n = chunks[c];
        dsp_sos_process(&sos, &state_chunked, 1U, &in[offset], &out_chunked[offset], n);
        offset += n;
    }

    for (size_t i = 0U; i < CONT_FRAMES; ++i) {
        float diff = fabsf(out_all[i] - out_chunked[i]);
        if (diff > TOLERANCE) {
            fprintf(stderr, "FAIL: frame %zu: all=%f, chunked=%f (diff %e)\n",
                    i, (double)out_all[i], (double)out_chunked[i], (double)diff);
            return false;
        }
    }

    printf("  PASS: chunked processing preserves state across block boundaries\n");
    return true;
}

static bool test_zero_sections_passthrough(void)
{
    printf("Running test_zero_sections_passthrough...\n");

    float in[8] = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f };
    float out[8] = { 0.0f };

    dsp_sos_process(NULL, NULL, 0U, in, out, 8U);

    for (size_t i = 0U; i < 8U; ++i) {
        if (out[i] != in[i]) {
            fprintf(stderr, "FAIL: passthrough out[%zu]=%f != in[%zu]=%f\n",
                    i, (double)out[i], i, (double)in[i]);
            return false;
        }
    }

    printf("  PASS: zero sections passes input through to output\n");
    return true;
}

int main(void)
{
    printf("=== dspblocks test_sos ===\n");

    bool ok = true;
    ok = ok && test_analytic_impulse_response();
    ok = ok && test_inplace_vs_outofplace();
    ok = ok && test_block_continuity();
    ok = ok && test_zero_sections_passthrough();

    if (!ok) {
        printf("FAILED\n");
        return 1;
    }

    printf("ALL TESTS PASSED\n");
    return 0;
}
