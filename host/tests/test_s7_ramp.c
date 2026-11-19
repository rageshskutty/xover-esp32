#include "dsp_s7_ramp.h"

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

int main(void)
{
    uint32_t frames = 55U;
    CHECK(check(0.0f, 1.0f, 1000U) == 0);
    CHECK(check(1.0f, 0.0f, 1000U) == 0);
    CHECK(check(0.5f, 0.75f, 960U) == 0);
    CHECK(check(0.5f, 0.1f, 400U) == 0);
    CHECK(check(0.1f, 0.099f, 240U) == 0);
    CHECK(check(0.5f, 0.5f, 0U) == 0);
    CHECK(dsp_s7_ramp_frames(NAN, 0.0f, &frames) == -1 && frames == 55U);
    CHECK(dsp_s7_ramp_frames(0.0f, 1.1f, &frames) == -1 && frames == 55U);
    CHECK(dsp_s7_ramp_frames(0.0f, 1.0f, NULL) == -1);
    puts("PASS: S7 ramp duration");
    return 0;
}