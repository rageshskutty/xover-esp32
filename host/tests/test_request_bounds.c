#include "xoweb_bounds.h"
#include "dspgraph.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static int ok(size_t b, size_t d, size_t n, size_t s)
{
    return xoweb_request_bounds_valid(b, d, n, s) == 1;
}

static int invalid_config_rejected(void)
{
    static const char bad[] =
        "{\"version\":1,\"sample_rate_hz\":48000,\"nodes\":["
        "{\"id\":\"g\",\"type\":\"gain\",\"db\":13}],\"edges\":[]}";
    dspg_report_t report = {0};
    if (dspg_validate(bad, sizeof(bad) - 1U, &report) != -1) return 0;
    for (uint8_t i = 0U; i < report.n; i++)
        if (strcmp(report.items[i].code, "E_NODE_PARAM") == 0) return 1;
    return 0;
}

int main(void)
{
    CHECK(ok(1U, 1U, 0U, 0U));
    CHECK(ok(32768U, 8U, 64U, 32U));
    CHECK(!ok(0U, 1U, 0U, 0U));
    CHECK(!ok(32769U, 1U, 0U, 0U));
    CHECK(!ok(1U, 0U, 0U, 0U));
    CHECK(!ok(1U, 9U, 0U, 0U));
    CHECK(!ok(1U, 1U, 65U, 0U));
    CHECK(!ok(1U, 1U, 0U, 33U));
    CHECK(!ok(SIZE_MAX, 1U, 0U, 0U));
    CHECK(!ok(1U, SIZE_MAX, 0U, 0U));
    CHECK(!ok(1U, 1U, 0U, SIZE_MAX));
    CHECK(invalid_config_rejected());
    puts("PASS: request bounds");
    return 0;
}