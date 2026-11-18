#include "dspblocks.h"
#include <float.h>
#include <math.h>
#include <stdio.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

int main(void)
{
    double re = 7.0, im = 8.0;
    const dsp_sos_t unity = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    CHECK(dsp_sos_eval(&unity, 1U, 48000.0, 1000.0, &re, &im) == DSP_OK);
    CHECK(re == 1.0 && im == 0.0);
    CHECK(dsp_sos_eval(&unity, 1U, NAN, 1000.0, &re, &im) == DSP_E_PARAM);
    CHECK(dsp_sos_eval(&unity, 1U, INFINITY, 1000.0, &re, &im) == DSP_E_PARAM);
    CHECK(dsp_sos_eval(&unity, 1U, 48000.0, NAN, &re, &im) == DSP_E_RANGE);
    CHECK(dsp_sos_eval(&unity, 1U, 48000.0, INFINITY, &re, &im) == DSP_E_RANGE);
    const dsp_sos_t nan_sos = {(float)NAN, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    const dsp_sos_t inf_sos = {1.0f, 0.0f, 0.0f, 0.0f, (float)INFINITY, 0.0f, 0.0f};
    CHECK(dsp_sos_eval(&nan_sos, 1U, 48000.0, 1000.0, &re, &im) == DSP_E_UNSTABLE);
    CHECK(dsp_sos_eval(&inf_sos, 1U, 48000.0, 1000.0, &re, &im) == DSP_E_UNSTABLE);
    CHECK(dsp_sos_eval(&unity, DSP_MAX_EVAL_SECTIONS + 1U, 48000.0, 1000.0, &re, &im) == DSP_E_PARAM);
    dsp_sos_t large[DSP_MAX_EVAL_SECTIONS];
    for (size_t i = 0U; i < DSP_MAX_EVAL_SECTIONS; ++i) large[i] = (dsp_sos_t){FLT_MAX, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    re = 7.0; im = 8.0;
    CHECK(dsp_sos_eval(large, DSP_MAX_EVAL_SECTIONS, 48000.0, 1000.0, &re, &im) == DSP_E_UNSTABLE);
    CHECK(re == 7.0 && im == 8.0);

    double grid[DSP_GRID_POINTS_DEFAULT];
    CHECK(dsp_log_grid(10.0, 20000.0, grid, DSP_GRID_POINTS_DEFAULT) == DSP_OK);
    CHECK(grid[0] == 10.0 && grid[DSP_GRID_POINTS_DEFAULT - 1U] == 20000.0);
    for (size_t i = 1U; i < DSP_GRID_POINTS_DEFAULT; ++i) CHECK(grid[i] > grid[i - 1U]);
    CHECK(dsp_log_grid(NAN, 20000.0, grid, 3U) == DSP_E_PARAM);
    CHECK(dsp_log_grid(10.0, INFINITY, grid, 3U) == DSP_E_PARAM);
    CHECK(dsp_log_grid(0.0, 20000.0, grid, 3U) == DSP_E_PARAM);
    CHECK(dsp_log_grid(10.0, 10.0, grid, 3U) == DSP_E_PARAM);
    CHECK(dsp_log_grid(10.0, 20000.0, grid, 1U) == DSP_E_PARAM);
    CHECK(dsp_log_grid(1.0, nextafter(1.0, 2.0), grid, 3U) == DSP_E_RANGE);

    double extremes[3];
    CHECK(dsp_log_grid(1e-300, 1e300, extremes, 3U) == DSP_OK);
    CHECK(extremes[0] == 1e-300 && extremes[1] > extremes[0] && extremes[2] == 1e300);
    puts("response evaluation and logarithmic grid checks passed");
    return 0;
}
