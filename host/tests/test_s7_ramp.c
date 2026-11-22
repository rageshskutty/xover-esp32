#include "dsp_s7_ramp.h"
#include "dspblocks.h"

#include <math.h>
#include <stdio.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static int check(float from, float to, uint32_t expected)
{
    uint32_t frames = 99U;
    CHECK(dsp_s7_ramp_frames(from, to, &frames) == 0 && frames == expected);
    if (frames != 0U) CHECK(fabsf((to - from) / (float)frames) <= 0.001f);
    return 0;
}

/* The commanded step above is exact, but dsp_gain_process accumulates
 * cur +/- step in float32 and snaps to target on the last frame, so the
 * observed output delta can exceed the commanded step. Measure it with the
 * real executor. 0.0011f is 10 % headroom over 0.001f (-59.2 dBFS); the
 * worst accumulated error is about frames * 0.5 ulp(1.0f), roughly 6e-5. */
#define EXEC_DELTA_LIMIT 0.0011f

static int check_executed(float from, float to, uint32_t expected)
{
    uint32_t frames = 0U;
    float in[1024], out[1024];
    dsp_gain_t g;
    CHECK(dsp_s7_ramp_frames(from, to, &frames) == 0 && frames == expected);
    CHECK(frames > 0U && frames <= 1024U);
    for (uint32_t i = 0U; i < frames; i++) in[i] = 1.0f;
    dsp_gain_init(&g, from);
    dsp_gain_set_target(&g, to, frames);
    dsp_gain_process(&g, in, out, frames);
    float prev = from;
    for (uint32_t i = 0U; i < frames; i++) {
        CHECK(fabsf(out[i] - prev) <= EXEC_DELTA_LIMIT);
        prev = out[i];
    }
    CHECK(out[frames - 1U] == to);
    return 0;
}

int main(void)
{
    uint32_t frames = 55U;
    CHECK(check(0.0f, 1.0f, 1000U) == 0);
    CHECK(check(1.0f, 0.0f, 1000U) == 0);
    CHECK(check(0.5f, 0.75f, 960U) == 0);
    CHECK(check(0.5f, 0.1f, 400U) == 0);
    CHECK(check(0.1f, 0.099f, 240U) == 0);
    CHECK(check(0.24f, 0.0f, 240U) == 0);
    CHECK(check(0.25f, 0.0f, 250U) == 0);
    CHECK(check(nextafterf(0.25f, INFINITY), 0.0f, 251U) == 0);
    CHECK(check(0.5f, 0.5f, 0U) == 0);
    CHECK(check_executed(0.0f, 1.0f, 1000U) == 0);
    CHECK(check_executed(1.0f, 0.0f, 1000U) == 0);
    CHECK(check_executed(1.0f, 0.256032f, 744U) == 0); /* worst case seen in fuzzing */
    CHECK(check_executed(0.25f, 0.0f, 250U) == 0);
    CHECK(dsp_s7_ramp_frames(NAN, 0.0f, &frames) == -1 && frames == 55U);
    CHECK(dsp_s7_ramp_frames(0.0f, 1.1f, &frames) == -1 && frames == 55U);
    CHECK(dsp_s7_ramp_frames(0.0f, 1.0f, NULL) == -1);
    puts("PASS: S7 ramp duration");
    return 0;
}

