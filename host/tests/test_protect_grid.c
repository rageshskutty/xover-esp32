#include "dspgraph.h"
#include "dspgraph_protect.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static dspg_protect_t rec(const char *id, float lo, float hi)
{
    dspg_protect_t r = {0};
    (void)snprintf(r.out_id, sizeof(r.out_id), "%s", id);
    r.max_gain_db = 0.0f;
    r.stop_below_hz = lo;
    r.stop_above_hz = hi;
    r.atten_below_db = lo == 0.0f ? 0.0f : 20.0f;
    r.atten_above_db = hi == 0.0f ? 0.0f : 20.0f;
    return r;
}

int main(void)
{
    double f[1070];
    size_t n = 99U;
    dspg_protect_t r[] = {rec("o", 0.0f, 0.0f)};
    CHECK(dspg_protect_grid(r, 1U, f, 1070U, &n) == 0 && n == 1054U);
    CHECK(f[0] == 10.0 && f[n - 1U] == 20000.0);
    for (size_t i = 1U; i < n; i++) CHECK(f[i - 1U] < f[i]);

    r[0] = rec("o", 100.0f, 2000.0f);
    CHECK(dspg_protect_grid(r, 1U, f, 1070U, &n) == 0 && n == 1056U);
    {
        int has_100 = 0, has_2000 = 0;
        for (size_t i = 0U; i < n; i++) {
            if (f[i] == 100.0) has_100 = 1;
            if (f[i] == 2000.0) has_2000 = 1;
        }
        CHECK(has_100 && has_2000);
    }

    r[0] = rec("o", 10.0f, 20000.0f);
    CHECK(dspg_protect_grid(r, 1U, f, 1070U, &n) == 0 && n == 1054U);
    CHECK(f[0] == 10.0 && f[n - 1U] == 20000.0);

    /* Four distinct enabled edges create 1058 unique values; capacity is 1057. */
    {
        dspg_protect_t four[] = {rec("o", 100.0f, 2000.0f), rec("p", 500.0f, 8000.0f)};
        double before[1070];
        for (size_t i = 0U; i < 1070U; i++) f[i] = 12345.0;
        memcpy(before, f, sizeof(f));
        n = 7U;
        CHECK(dspg_protect_grid(four, 2U, f, 1057U, &n) == -1);
        CHECK(n == 0U && memcmp(f, before, sizeof(f)) == 0);
    }
    puts("PASS: protection grid contract");
    return 0;
}