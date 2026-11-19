#include "dspblocks.h"

#include <math.h>
#include <stdio.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static int at_gain(float b0, double f_hz, double expected_db)
{
    dsp_sos_t s = {0};
    double re = 0.0, im = 0.0;
    s.b0 = b0;
    if (dsp_sos_eval(&s, 1U, 48000.0, f_hz, &re, &im) != DSP_OK) return 0;
    if (!isfinite(re) || !isfinite(im)) return 0;
    return fabs(20.0 * log10(hypot(re, im)) - expected_db) <= 1e-5;
}

int main(void)
{
    const double f[] = {10.0, 1000.0, 20000.0};
    for (size_t i = 0U; i < sizeof(f) / sizeof(f[0]); i++) {
        CHECK(at_gain(1.0f, f[i], 0.0));
        CHECK(at_gain(0.5f, f[i], -6.020599913));
    }
    puts("PASS: SOS response vectors");
    return 0;
}