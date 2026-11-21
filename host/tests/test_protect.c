#include "dspgraph.h"
#include "dspgraph_protect.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); return 1; \
} } while (0)

static dspg_protect_t record(const char *id, float ceiling)
{
    dspg_protect_t r = {0};
    (void)snprintf(r.out_id, sizeof(r.out_id), "%s", id);
    r.max_gain_db = ceiling;
    return r;
}

static int zero_failure(const dspg_protect_failure_t *f)
{
    static const dspg_protect_failure_t zero = {0};
    return memcmp(f, &zero, sizeof(zero)) == 0;
}

static int rejected(const dspg_protect_t *records, size_t nr,
                    const char *const *ids, size_t no,
                    const double *freq, const double *const *rows, size_t nf)
{
    dspg_protect_failure_t f;
    memset(&f, 0xA5, sizeof(f));
    CHECK(dspg_protect_check(records, nr, ids, no, freq, rows, nf, &f) == -1);
    CHECK(zero_failure(&f));
    return 0;
}

int main(void)
{
    const char *const one_id[] = {"o"};
    const double freq[] = {100.0, 1000.0};
    const double pass[] = {0.0, 0.0};
    const double quiet[] = {-6.020599913, -6.020599913};
    const double *const pass_rows[] = {pass};
    const double *const quiet_rows[] = {quiet};
    dspg_protect_t one = record("o", 0.0f);
    dspg_protect_failure_t f;

    /* Basic pass, equality, success-zeroing, and real violation contents. */
    memset(&f, 0xA5, sizeof(f));
    CHECK(dspg_protect_check(&one, 1U, one_id, 1U, freq, pass_rows, 2U, &f) == 0);
    CHECK(zero_failure(&f));
    one.max_gain_db = -6.020599913f;
    CHECK(dspg_protect_check(&one, 1U, one_id, 1U, freq, quiet_rows, 2U, &f) == 0);
    one.max_gain_db = -1.0f;
    CHECK(dspg_protect_check(&one, 1U, one_id, 1U, freq, pass_rows, 2U, &f) == -1);
    CHECK(strcmp(f.out_id, "o") == 0 && f.frequency_hz == 100.0 && f.response_db == 0.0);

    /* Empty dimensions and nulls fail closed with a zero failure record. */
    CHECK(rejected(&one, 1U, one_id, 0U, freq, pass_rows, 2U) == 0);
    CHECK(rejected(&one, 1U, one_id, 1U, freq, pass_rows, 0U) == 0);
    CHECK(rejected(NULL, 1U, one_id, 1U, freq, pass_rows, 2U) == 0);
    CHECK(rejected(&one, 1U, NULL, 1U, freq, pass_rows, 2U) == 0);
    CHECK(rejected(&one, 1U, one_id, 1U, NULL, pass_rows, 2U) == 0);
    CHECK(rejected(&one, 1U, one_id, 1U, freq, NULL, 2U) == 0);
    CHECK(dspg_protect_check(&one, 1U, one_id, 1U, freq, pass_rows, 2U, NULL) == -1);

    /* Non-finite sample is malformed, even when an earlier sample violates. */
    {
        const double bad[] = {1.0, INFINITY};
        const double *const rows[] = {bad};
        one.max_gain_db = -1.0f;
        CHECK(rejected(&one, 1U, one_id, 1U, freq, rows, 2U) == 0);
    }

    /* Validate every record before comparing any output curve. */
    {
        const char *const ids[] = {"o", "p"};
        const double bad_first[] = {10.0};
        const double *const rows[] = {bad_first, pass};
        dspg_protect_t records[] = {record("o", -1.0f), record("p", 0.0f)};
        records[1].stop_below_hz = 9.0f; /* invalid edge */
        CHECK(rejected(records, 2U, ids, 2U, freq, rows, 1U) == 0);
    }

    /* Equal counts do not prove a bijection; duplicate output ID must reject. */
    {
        const char *const ids[] = {"o", "o"};
        const double *const rows[] = {pass, pass};
        dspg_protect_t records[] = {record("o", 0.0f), record("p", 0.0f)};
        CHECK(rejected(records, 2U, ids, 2U, freq, rows, 2U) == 0);
    }

    /* Missing, duplicate, unknown, and unterminated IDs are malformed. */
    {
        const char *const ids[] = {"o", "p"};
        const double *const rows[] = {pass, pass};
        dspg_protect_t missing[] = {record("o", 0.0f)};
        dspg_protect_t duplicate[] = {record("o", 0.0f), record("o", 0.0f)};
        dspg_protect_t unknown[] = {record("o", 0.0f), record("x", 0.0f)};
        dspg_protect_t unterminated = {0};
        memset(unterminated.out_id, 'x', sizeof(unterminated.out_id));
        CHECK(rejected(missing, 1U, ids, 2U, freq, rows, 2U) == 0);
        CHECK(rejected(duplicate, 2U, ids, 2U, freq, rows, 2U) == 0);
        CHECK(rejected(unknown, 2U, ids, 2U, freq, rows, 2U) == 0);
        CHECK(rejected(&unterminated, 1U, one_id, 1U, freq, pass_rows, 2U) == 0);
    }

    /* Frequency ordering and non-finite values are validated before compare. */
    {
        const double unordered[] = {100.0, 100.0};
        const double nonfinite[] = {NAN, 1000.0};
        one = record("o", 0.0f);
        CHECK(rejected(&one, 1U, one_id, 1U, unordered, pass_rows, 2U) == 0);
        CHECK(rejected(&one, 1U, one_id, 1U, nonfinite, pass_rows, 2U) == 0);
    }

    puts("PASS: protection comparator contract");
    return 0;
}
