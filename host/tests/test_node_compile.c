#include <math.h>
#include <stdio.h>
#include <string.h>

#include "dspgraph_build.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

#define FS_HZ 48000.0
#define EXPECTED_DELAY_SAMPLES 48U
#define LONG_ID "\"id\":\"0123456789012345678901234567890123\""

static dsp_err_t compile(const char *vars, const char *node, bnode_t *out)
{
    cJSON *v = (vars != NULL) ? cJSON_Parse(vars) : NULL;
    cJSON *n = cJSON_Parse(node);
    dsp_err_t rc = (n != NULL) ? dspg_compile_node(v, n, out) : DSP_E_PARAM;
    cJSON_Delete(v);
    cJSON_Delete(n);
    return rc;
}

static int same(const dsp_cascade_t *a, const dsp_cascade_t *b)
{
    return a->n == b->n && memcmp(a->sos, b->sos, a->n * sizeof(a->sos[0])) == 0;
}

static int test_filters(void)
{
    bnode_t b;
    dsp_cascade_t want;
    dsp_sos_t s;

    CHECK(compile(NULL, "{\"id\":\"x\",\"type\":\"crossover\",\"mode\":\"lowpass\","
                  "\"family\":\"Linkwitz-Riley\",\"order\":4,\"frequency_hz\":2400}", &b) == DSP_OK);
    CHECK(dsp_design_xo(DSP_FAMILY_LINKWITZ_RILEY, DSP_MODE_LOWPASS, 4U, 2400.0, FS_HZ, &want) == DSP_OK);
    CHECK(b.kind == K_CASCADE && same(&b.casc, &want) && strcmp(b.id, "x") == 0);

    /* @var is resolved from "vars" */
    CHECK(compile("{\"f\":80}", "{\"id\":\"x\",\"type\":\"crossover\",\"mode\":\"highpass\","
                  "\"family\":\"Butterworth\",\"order\":3,\"frequency_hz\":\"@f\"}", &b) == DSP_OK);
    CHECK(dsp_design_xo(DSP_FAMILY_BUTTERWORTH, DSP_MODE_HIGHPASS, 3U, 80.0, FS_HZ, &want) == DSP_OK);
    CHECK(same(&b.casc, &want) && b.casc.n == 2U);

    CHECK(compile(NULL, "{\"id\":\"x\",\"type\":\"peq\",\"frequency_hz\":1000,\"q\":2,\"db\":-3}", &b) == DSP_OK);
    CHECK(dsp_design_peq(1000.0, 2.0, -3.0, FS_HZ, &s) == DSP_OK);
    CHECK(b.casc.n == 1U && memcmp(&b.casc.sos[0], &s, sizeof(s)) == 0);
    CHECK(compile(NULL, "{\"id\":\"x\",\"type\":\"shelf\",\"frequency_hz\":200,\"db\":4,\"shape\":\"high\"}", &b) == DSP_OK);
    CHECK(dsp_design_shelf(DSP_SHELF_HIGH, 200.0, 4.0, FS_HZ, &s) == DSP_OK);
    CHECK(memcmp(&b.casc.sos[0], &s, sizeof(s)) == 0);
    CHECK(compile(NULL, "{\"id\":\"x\",\"type\":\"allpass\",\"frequency_hz\":500,\"q\":0.7}", &b) == DSP_OK);
    CHECK(dsp_design_allpass(500.0, 0.7, FS_HZ, &s) == DSP_OK && memcmp(&b.casc.sos[0], &s, sizeof(s)) == 0);
    CHECK(compile(NULL, "{\"id\":\"x\",\"type\":\"notch\",\"frequency_hz\":500,\"q\":5}", &b) == DSP_OK);
    CHECK(dsp_design_notch(500.0, 5.0, FS_HZ, &s) == DSP_OK && memcmp(&b.casc.sos[0], &s, sizeof(s)) == 0);

    CHECK(compile(NULL, "{\"id\":\"x\",\"type\":\"biquad\",\"b\":[1,0,0],\"a\":[0,0],\"sample_rate_hz\":48000}", &b) == DSP_OK);
    CHECK(b.casc.n == 1U && b.casc.sos[0].b0 == 1.0f && b.casc.sos[0].da1 == 0.0f);
    return 0;
}

static int test_simple_nodes(void)
{
    bnode_t b;
    CHECK(compile(NULL, "{\"id\":\"i\",\"type\":\"input\",\"channel\":1}", &b) == DSP_OK && b.kind == K_INPUT);
    CHECK(compile(NULL, "{\"id\":\"o\",\"type\":\"output\",\"slot\":2}", &b) == DSP_OK && b.kind == K_OUTPUT);
    CHECK(compile(NULL, "{\"id\":\"g\",\"type\":\"gain\",\"db\":-6.020599913}", &b) == DSP_OK);
    CHECK(b.kind == K_GAIN && fabsf(b.gain - 0.5f) < 1e-6f);
    CHECK(compile(NULL, "{\"id\":\"d\",\"type\":\"delay\",\"delay_us\":1000}", &b) == DSP_OK);
    CHECK(b.kind == K_DELAY && b.delay == EXPECTED_DELAY_SAMPLES);
    CHECK(compile(NULL, "{\"id\":\"p\",\"type\":\"polarity\",\"invert\":true}", &b) == DSP_OK);
    CHECK(b.kind == K_POLARITY && b.invert);
    CHECK(compile(NULL, "{\"id\":\"m\",\"type\":\"mix\",\"gains\":[1.0,0.25,0.5]}", &b) == DSP_OK);
    CHECK(b.kind == K_MIX && b.gains[0] == 1.0f && b.gains[1] == 0.25f && b.gains[2] == 0.5f);
    return 0;
}

/* Violating nodes stay rejected (S6). */
static int test_rejections(void)
{
    bnode_t b;
    CHECK(compile(NULL, "{\"id\":\"x\",\"type\":\"crossover\",\"mode\":\"lowpass\",\"family\":\"Butterworth\","
                  "\"order\":2,\"frequency_hz\":\"@missing\"}", &b) != DSP_OK);
    CHECK(compile(NULL, "{\"id\":\"x\",\"type\":\"crossover\",\"mode\":\"lowpass\",\"family\":\"Butterworth\","
                  "\"order\":2,\"frequency_hz\":5}", &b) != DSP_OK);
    CHECK(compile(NULL, "{\"id\":\"x\",\"type\":\"crossover\",\"mode\":\"lowpass\",\"family\":\"Bessel\","
                  "\"order\":2,\"frequency_hz\":500}", &b) == DSP_E_UNSUPPORTED);
    CHECK(compile(NULL, "{\"id\":\"x\",\"type\":\"crossover\",\"mode\":\"bandpass\",\"family\":\"Butterworth\","
                  "\"order\":2,\"frequency_hz\":500}", &b) == DSP_E_PARAM);
    CHECK(compile(NULL, "{\"id\":\"x\",\"type\":\"shelf\",\"frequency_hz\":200,\"db\":4,\"shape\":\"mid\"}", &b) == DSP_E_PARAM);
    CHECK(compile(NULL, "{\"id\":\"x\",\"type\":\"biquad\",\"b\":[1,0,0],\"a\":[0,1.5],\"sample_rate_hz\":48000}", &b) == DSP_E_UNSTABLE);
    CHECK(compile(NULL, "{\"id\":\"x\",\"type\":\"biquad\",\"b\":[1,0],\"a\":[0,0],\"sample_rate_hz\":48000}", &b) != DSP_OK);
    CHECK(compile(NULL, "{\"id\":\"x\",\"type\":\"mix\",\"gains\":[1.0]}", &b) != DSP_OK);
    CHECK(compile(NULL, "{\"id\":\"x\",\"type\":\"bogus\"}", &b) != DSP_OK);
    CHECK(compile(NULL, "{" LONG_ID ",\"type\":\"input\",\"channel\":0}", &b) != DSP_OK);
    return 0;
}

int main(void)
{
    CHECK(test_filters() == 0);
    CHECK(test_simple_nodes() == 0);
    CHECK(test_rejections() == 0);
    printf("test_node_compile OK\n");
    return 0;
}
