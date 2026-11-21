#include "dspgraph_validate.h"
#include "dspgraph_json.h"
#include "dspgraph_graph.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

#define PATH_MAX_LEN 256U
#define MANY_NODES 12

static const char *const REJECTED_HERE[] = {
    "bessel_unsupported", "crossover_too_low", "gain_too_high", "lr_odd_order",
    "peq_q_too_high", "recognized_wrong_node_property", "sample_rate_96k",
    "unknown_node_property", "unknown_node_type", "biquad_unstable", "cycle", "duplicate_slot",
    "fanin_without_mix", "too_many_sections", "undefined_var", "var_outside_limits",
};

static char file_buf[DSPG_JSON_MAX_BYTES];

static size_t load(const char *dir, const char *name, char *out, size_t cap)
{
    char path[PATH_MAX_LEN];
    FILE *f;
    size_t n;
    size_t dir_len = strlen(dir);
    size_t name_len = strlen(name);
    size_t name_start = dir_len + 1U;
    size_t suffix_start = name_start + name_len;
    if (dir_len >= sizeof(path) || name_start >= sizeof(path) ||
        name_len >= sizeof(path) - name_start ||
        sizeof(".json") > sizeof(path) - suffix_start) {
        return 0U;
    }
    memcpy(path, dir, dir_len);
    path[dir_len] = '/';
    memcpy(path + name_start, name, name_len);
    memcpy(path + suffix_start, ".json", sizeof(".json"));
#if defined(_MSC_VER)
    if (fopen_s(&f, path, "rb") != 0) {
        f = NULL;
    }
#else
    f = fopen(path, "rb");
#endif
    if (f == NULL) {
        return 0U;
    }
    n = fread(out, 1U, cap, f);
    (void)fclose(f);
    return n;
}

/* Parses a fixture, strips "_expect", validates the tree. Returns expect text. */
static int run_fixture(const char *name, int invalid, dspg_report_t *rep, char *expect)
{
    char dir[PATH_MAX_LEN];
    size_t n;
    cJSON *root;
    const cJSON *e;
    dspg_parse_issue_t pi;
    (void)snprintf(dir, sizeof(dir), "%s/%s", XOVER_CONFIG_DIR, invalid ? "invalid" : ".");
    n = load(dir, name, file_buf, sizeof(file_buf));
    if (n == 0U) {
        return -1;
    }
    root = dspg_parse_bounded_json(file_buf, n, &pi);
    if (root == NULL) {
        return -1;
    }
    e = cJSON_GetObjectItemCaseSensitive(root, "_expect");
    expect[0] = '\0';
    if (cJSON_IsString(e)) {
        (void)snprintf(expect, 32U, "%s", e->valuestring);
        cJSON_DeleteItemFromObjectCaseSensitive(root, "_expect");
    }
    memset(rep, 0, sizeof(*rep));
    if (dspg_validate_tree(root, rep) == 0) {
        (void)dspg_validate_graph(root, rep);
    }
    cJSON_Delete(root);
    return 0;
}

static int has_code(const dspg_report_t *rep, const char *code)
{
    for (uint8_t i = 0U; i < rep->n; i++) {
        if (strcmp(rep->items[i].code, code) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Validates in -> biquad(b, a) -> out. Returns 0 valid, -1 rejected with E_NODE_PARAM, -2 other. */
static int biquad_result(const char *b, const char *a)
{
    char json[512];
    dspg_report_t rep;
    int n = snprintf(json, sizeof(json),
                     "{\"version\":1,\"sample_rate_hz\":48000,\"nodes\":["
                     "{\"id\":\"i\",\"type\":\"input\",\"channel\":0},"
                     "{\"id\":\"q\",\"type\":\"biquad\",\"b\":[%s],\"a\":[%s],\"sample_rate_hz\":48000},"
                     "{\"id\":\"o\",\"type\":\"output\",\"slot\":0}],"
                     "\"edges\":[[\"i\",\"q\"],[\"q\",\"o\"]]}", b, a);
    if (n <= 0 || (size_t)n >= sizeof(json)) {
        return -2;
    }
    if (dspg_validate(json, (size_t)n, &rep) == 0) {
        return 0;
    }
    return (rep.n == 1U && strcmp(rep.items[0].code, "E_NODE_PARAM") == 0) ? -1 : -2;
}

static int expect_code(const char *json, const char *code)
{
    dspg_report_t rep;
    return dspg_validate(json, strlen(json), &rep) == -1 && has_code(&rep, code);
}

int main(void)
{
    dspg_report_t rep;
    char expect[32];
    char many[1024];
    size_t used = 0U;

    CHECK(run_fixture("legacy/example_2way_sub", 0, &rep, expect) == 0);
    CHECK(rep.n == 0U);

    for (size_t i = 0U; i < sizeof(REJECTED_HERE) / sizeof(REJECTED_HERE[0]); i++) {
        CHECK(run_fixture(REJECTED_HERE[i], 1, &rep, expect) == 0);
        CHECK(expect[0] != '\0');
        if (!has_code(&rep, expect)) {
            fprintf(stderr, "%s: expected %s, got %u issue(s)\n", REJECTED_HERE[i], expect, (unsigned)rep.n);
            return 1;
        }
    }
    CHECK(expect_code("{\"version\":1,\"sample_rate_hz\":48000,\"nodes\":[{\"id\":\"i\",\"type\":\"input\",\"channel\":0},{\"id\":\"o\",\"type\":\"output\",\"slot\":0}],\"edges\":[[\"i\",\"o\"],[\"i\",\"zz\"]]}", "E_GRAPH_REF"));
    CHECK(expect_code("{\"version\":1,\"sample_rate_hz\":48000,\"nodes\":[{\"id\":\"i\",\"type\":\"input\",\"channel\":0},{\"id\":\"i\",\"type\":\"output\",\"slot\":0}],\"edges\":[]}", "E_GRAPH_REF"));
    CHECK(expect_code("{\"version\":1,\"sample_rate_hz\":48000,\"nodes\":[{\"id\":\"i\",\"type\":\"input\",\"channel\":0},{\"id\":\"g\",\"type\":\"gain\",\"db\":0}],\"edges\":[[\"i\",\"g\"]]}", "E_GRAPH_DEAD"));
    CHECK(expect_code("{\"version\":1,\"sample_rate_hz\":48000,\"vars\":{\"f\":30000},\"nodes\":[{\"id\":\"i\",\"type\":\"input\",\"channel\":0},"
                      "{\"id\":\"x\",\"type\":\"crossover\",\"mode\":\"lowpass\",\"family\":\"Butterworth\",\"order\":2,\"frequency_hz\":\"@f\"},"
                      "{\"id\":\"o\",\"type\":\"output\",\"slot\":0}],\"edges\":[[\"i\",\"x\"],[\"x\",\"o\"]]}", "E_NODE_PARAM"));

    /* T06a5: biquad stability is judged on the float32 values the DSP runs. */
    CHECK(biquad_result("1,0,0", "-1.9,0.95") == 0);            /* stable */
    CHECK(biquad_result("1,0,0", "-1.99,0.9901") == 0);         /* pole just inside */
    CHECK(biquad_result("1,0,0", "-1.9,1.2") == -1);            /* |a2| > 1 */
    CHECK(biquad_result("1,0,0", "0,1") == -1);                 /* |a2| == 1 */
    CHECK(biquad_result("1,0,0", "2,1") == -1);                 /* pole on z = 1 */
    CHECK(biquad_result("1,0,0", "-2,0.99") == -1);             /* 1 + a1 + a2 < 0 */
    CHECK(biquad_result("1,0,0", "2.5,0.5") == -1);            /* 1 - a1 + a2 < 0 */
    CHECK(biquad_result("1e39,0,0", "0,0") == -1);              /* beyond float32 */
    CHECK(biquad_result("1,0,0", "1e999,0") == -1);             /* infinite */

    /* dspg_validate: syntax errors surface as E_JSON, never as valid. */
    CHECK(dspg_validate("{", 1U, &rep) == -1);
    CHECK(rep.n == 1U && strcmp(rep.items[0].code, "E_JSON") == 0);
    CHECK(dspg_validate(NULL, 0U, NULL) == -1);

    /* A violating config stays rejected through the public entry point (S6). */
    {
        static const char bad_gain[] =
            "{\"version\":1,\"sample_rate_hz\":48000,\"nodes\":["
            "{\"id\":\"g\",\"type\":\"gain\",\"db\":13}],\"edges\":[]}";
        CHECK(dspg_validate(bad_gain, sizeof(bad_gain) - 1U, &rep) == -1);
        CHECK(has_code(&rep, "E_NODE_PARAM"));
    }

    /* Report is bounded at DSPG_MAX_ISSUES. */
    used = (size_t)snprintf(many, sizeof(many),
                            "{\"version\":1,\"sample_rate_hz\":48000,\"nodes\":[");
    for (int i = 0; i < MANY_NODES; i++) {
        used += (size_t)snprintf(many + used, sizeof(many) - used,
                                 "%s{\"id\":\"n%d\",\"type\":\"bogus\"}", i == 0 ? "" : ",", i);
    }
    used += (size_t)snprintf(many + used, sizeof(many) - used, "],\"edges\":[]}");
    CHECK(dspg_validate(many, used, &rep) == -1);
    CHECK(rep.n == DSPG_MAX_ISSUES);

    puts("PASS: graph validator (schema-equivalent, node parameter and biquad stability checks)");
    return 0;
}
