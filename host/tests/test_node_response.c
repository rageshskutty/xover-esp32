#include "dspgraph_build.h"
#include "dspgraph_response.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static int magnitude_is(const bnode_t *n, double expected_db)
{
    double re = 0.0, im = 0.0;
    CHECK(dspg_node_response(n, 48000.0, 1000.0, &re, &im) == 0);
    CHECK(fabs(20.0 * log10(hypot(re, im)) - expected_db) <= 0.001);
    return 0;
}

int main(void)
{
    bnode_t n;
    double re = 7.0, im = 8.0;
    memset(&n, 0, sizeof(n)); n.kind = K_INPUT;
    CHECK(magnitude_is(&n, 0.0) == 0);
    n.kind = K_OUTPUT; CHECK(magnitude_is(&n, 0.0) == 0);
    n.kind = K_GAIN; n.gain = 0.5f; CHECK(magnitude_is(&n, -6.020599913) == 0);
    n.kind = K_POLARITY; n.invert = true;
    CHECK(dspg_node_response(&n, 48000.0, 1000.0, &re, &im) == 0 && re == -1.0 && im == 0.0);
    n.kind = K_DELAY; n.delay = 12U;
    CHECK(dspg_node_response(&n, 48000.0, 1000.0, &re, &im) == 0);
    CHECK(fabs(re) < 1e-12 && fabs(im + 1.0) < 1e-12);
    n.kind = K_CASCADE; n.casc.n = 1U;
    /* Exact SOS coefficients from rbj_eq.json: peq, 1000 Hz, Q=.7, -6 dB. */
    n.casc.sos[0].b0 = 0.9419532704755859f;
    n.casc.sos[0].b1 = -1.7521412913270318f;
    n.casc.sos[0].b2 = 0.8253071789851647f;
    n.casc.sos[0].a1 = -1.7521412913270318f;
    n.casc.sos[0].a2 = 0.7672604494607507f;
    CHECK(magnitude_is(&n, -6.000003627305285) == 0);
    n.kind = K_MIX; re = 7.0; im = 8.0;
    CHECK(dspg_node_response(&n, 48000.0, 1000.0, &re, &im) == -1 && re == 7.0 && im == 8.0);
    CHECK(dspg_node_response(NULL, 48000.0, 1000.0, &re, &im) == -1);
    CHECK(dspg_node_response(&n, 48000.0, NAN, &re, &im) == -1);
    puts("PASS: compiled node response contract");
    return 0;
}