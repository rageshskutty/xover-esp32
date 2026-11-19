#include "dspgraph_protect.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static dspg_protect_t rec(const char *id)
{
    dspg_protect_t r = {0};
    (void)snprintf(r.out_id, sizeof(r.out_id), "%s", id);
    r.max_gain_db = 0.0f;
    return r;
}

int main(void)
{
    const char *const ids[] = {"o"};
    dspg_protect_t r = rec("o");
    CHECK(dspg_protect_record_valid(&r, 1U, ids, 1U) == 1);

    r.max_gain_db = -120.0f;
    CHECK(dspg_protect_record_valid(&r, 1U, ids, 1U) == 1);
    r.max_gain_db = 24.0f;
    CHECK(dspg_protect_record_valid(&r, 1U, ids, 1U) == 1);
    r.max_gain_db = -120.01f;
    CHECK(dspg_protect_record_valid(&r, 1U, ids, 1U) == 0);
    r = rec("o"); r.max_gain_db = 24.01f;
    CHECK(dspg_protect_record_valid(&r, 1U, ids, 1U) == 0);
    /* Both bands disabled, lower only, upper only, and both enabled. */
    CHECK(dspg_protect_record_valid(&r, 1U, ids, 1U) == 1);
    r.stop_below_hz = 100.0f; r.atten_below_db = 20.0f;
    CHECK(dspg_protect_record_valid(&r, 1U, ids, 1U) == 1);
    r = rec("o"); r.stop_above_hz = 100.0f; r.atten_above_db = 20.0f;
    CHECK(dspg_protect_record_valid(&r, 1U, ids, 1U) == 1);
    r.stop_below_hz = 100.0f; r.atten_below_db = 20.0f;
    CHECK(dspg_protect_record_valid(&r, 1U, ids, 1U) == 1);
    r = rec("o"); r.stop_below_hz = 10.0f; r.atten_below_db = 0.0f;
    CHECK(dspg_protect_record_valid(&r, 1U, ids, 1U) == 1);
    r.stop_below_hz = 20000.0f; r.atten_below_db = 120.0f;
    CHECK(dspg_protect_record_valid(&r, 1U, ids, 1U) == 1);
    r.stop_below_hz = 9.99f;
    CHECK(dspg_protect_record_valid(&r, 1U, ids, 1U) == 0);
    r = rec("o"); r.stop_above_hz = 20000.0f; r.atten_above_db = 120.0f;
    CHECK(dspg_protect_record_valid(&r, 1U, ids, 1U) == 1);
    r.stop_above_hz = 20000.01f;
    CHECK(dspg_protect_record_valid(&r, 1U, ids, 1U) == 0);

    r = rec("o"); r.atten_below_db = 0.01f;
    CHECK(dspg_protect_record_valid(&r, 1U, ids, 1U) == 0);
    r = rec("o"); r.atten_above_db = 0.01f;
    CHECK(dspg_protect_record_valid(&r, 1U, ids, 1U) == 0);
    r = rec("o"); r.stop_below_hz = 100.0f; r.atten_below_db = -0.01f;
    CHECK(dspg_protect_record_valid(&r, 1U, ids, 1U) == 0);
    r.atten_below_db = 120.01f;
    CHECK(dspg_protect_record_valid(&r, 1U, ids, 1U) == 0);
    r = rec("o"); r.stop_above_hz = 100.0f; r.atten_above_db = 120.01f;
    CHECK(dspg_protect_record_valid(&r, 1U, ids, 1U) == 0);
    r.max_gain_db = NAN;
    CHECK(dspg_protect_record_valid(&r, 1U, ids, 1U) == 0);

    r = rec("");
    CHECK(dspg_protect_record_valid(&r, 1U, ids, 1U) == 0);
    memset(r.out_id, 'x', sizeof(r.out_id));
    CHECK(dspg_protect_record_valid(&r, 1U, ids, 1U) == 0);
    {
        char long_id[32];
        const char *const long_ids[] = {long_id};
        memset(long_id, 'a', 31U); long_id[31] = '\0';
        r = rec(long_id);
        CHECK(dspg_protect_record_valid(&r, 1U, long_ids, 1U) == 1);
    }

    {
        const char *const dup_ids[] = {"o", "o"};
        const char *const two_ids[] = {"o", "p"};
        dspg_protect_t one[] = {rec("o")};
        dspg_protect_t dup[] = {rec("o"), rec("o")};
        dspg_protect_t unknown[] = {rec("o"), rec("x")};
        CHECK(dspg_protect_record_valid(one, 1U, two_ids, 2U) == 0);
        CHECK(dspg_protect_record_valid(dup, 2U, two_ids, 2U) == 0);
        CHECK(dspg_protect_record_valid(unknown, 2U, two_ids, 2U) == 0);
        CHECK(dspg_protect_record_valid(dup, 2U, dup_ids, 2U) == 0);
        CHECK(dspg_protect_record_valid(NULL, 1U, ids, 1U) == 0);
        CHECK(dspg_protect_record_valid(&r, 1U, NULL, 1U) == 0);
        CHECK(dspg_protect_record_valid(&r, 0U, ids, 1U) == 0);
    }
    puts("PASS: protection record validator contract");
    return 0;
}