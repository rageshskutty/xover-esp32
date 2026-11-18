#include "dspgraph_validate.h"

#include "dspblocks.h"

#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define CONFIG_VERSION 1.0
#define SAMPLE_RATE_HZ 48000.0
#define MAX_NODES 64
#define MAX_ID_LEN 31U
#define FREQ_MIN_HZ 10.0
#define FREQ_MAX_HZ 21600.0
#define Q_MIN 0.1
#define Q_MAX 20.0
#define EQ_DB_MIN (-24.0)
#define EQ_DB_MAX 12.0
#define GAIN_DB_MIN (-60.0)
#define GAIN_DB_MAX 12.0
#define DELAY_US_MAX 20000.0
#define ORDER_MIN 1.0
#define ORDER_MAX 8.0
#define INTEGER_LIMIT 1.0e9
#define MIX_MIN_GAINS 2
#define MIX_MAX_GAINS 8
#define BIQUAD_B_LEN 3
#define BIQUAD_A_LEN 2
#define EDGE_LEN 2
#define LABEL_LEN 32U

#define E_SCHEMA "E_SCHEMA"
#define E_NODE_PARAM "E_NODE_PARAM"
#define E_RESOURCE "E_RESOURCE"
#define E_UNSUPPORTED "E_UNSUPPORTED"

typedef enum {
    NK_ID, NK_TYPE, NK_FREQ, NK_FAMILY, NK_MODE, NK_ORDER, NK_CHANNEL, NK_SLOT,
    NK_GAINS, NK_DB, NK_Q, NK_SHAPE, NK_INVERT, NK_B, NK_A, NK_FS, NK_DELAY, NK_COUNT
} node_key_t;

static const char *const KEY_NAME[NK_COUNT] = {
    "id", "type", "frequency_hz", "family", "mode", "order", "channel", "slot",
    "gains", "db", "q", "shape", "invert", "b", "a", "sample_rate_hz", "delay_us"
};

#define BIT(k) (1U << (unsigned)(k))
#define COMMON (BIT(NK_ID) | BIT(NK_TYPE))
#define FREQ_Q (COMMON | BIT(NK_FREQ) | BIT(NK_Q))

/* Every key a type allows is also required (configs/schema.json). */
typedef struct {
    const char *name;
    uint32_t keys;
} node_spec_t;

static const node_spec_t SPECS[] = {
    {"input", COMMON | BIT(NK_CHANNEL)},
    {"output", COMMON | BIT(NK_SLOT)},
    {"crossover", COMMON | BIT(NK_FREQ) | BIT(NK_FAMILY) | BIT(NK_MODE) | BIT(NK_ORDER)},
    {"peq", FREQ_Q | BIT(NK_DB)},
    {"shelf", COMMON | BIT(NK_FREQ) | BIT(NK_DB) | BIT(NK_SHAPE)},
    {"allpass", FREQ_Q},
    {"notch", FREQ_Q},
    {"biquad", COMMON | BIT(NK_B) | BIT(NK_A) | BIT(NK_FS)},
    {"gain", COMMON | BIT(NK_DB)},
    {"delay", COMMON | BIT(NK_DELAY)},
    {"polarity", COMMON | BIT(NK_INVERT)},
    {"mix", COMMON | BIT(NK_GAINS)},
};
#define SPEC_COUNT (sizeof(SPECS) / sizeof(SPECS[0]))

static const char *const TOP_KEYS[] = {"version", "sample_rate_hz", "vars", "limits", "nodes", "edges"};
static const char *const FAMILIES[] = {"Butterworth", "Linkwitz-Riley", "Bessel"};
static const char *const MODES[] = {"lowpass", "highpass"};
static const char *const SHAPES[] = {"low", "high"};
#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

void dspg_report_add(dspg_report_t *report, const char *code, const char *path, const char *msg)
{
    dspg_issue_t *it;
    if (report->n >= DSPG_MAX_ISSUES) {
        return;
    }
    it = &report->items[report->n];
    report->n++;
    (void)snprintf(it->code, sizeof(it->code), "%s", code);
    (void)snprintf(it->path, sizeof(it->path), "%s", path);
    (void)snprintf(it->msg, sizeof(it->msg), "%s", msg);
}

static bool in_set(const cJSON *v, const char *const *set, size_t n)
{
    if (!cJSON_IsString(v)) {
        return false;
    }
    for (size_t i = 0U; i < n; i++) {
        if (strcmp(v->valuestring, set[i]) == 0) {
            return true;
        }
    }
    return false;
}

static bool is_top_key(const char *s)
{
    for (size_t i = 0U; i < COUNT_OF(TOP_KEYS); i++) {
        if (strcmp(s, TOP_KEYS[i]) == 0) {
            return true;
        }
    }
    return false;
}

static bool is_var_ref(const char *s)
{
    if (s[0] != '@' || !(isalpha((unsigned char)s[1]) || s[1] == '_')) {
        return false;
    }
    for (const char *p = &s[2]; *p != '\0'; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_') {
            return false;
        }
    }
    return true;
}

static bool is_integer(const cJSON *v)
{
    return cJSON_IsNumber(v) && fabs(v->valuedouble) <= INTEGER_LIMIT &&
           v->valuedouble == floor(v->valuedouble);
}

static bool is_number_array(const cJSON *v, int min_len, int max_len)
{
    int n;
    if (!cJSON_IsArray(v)) {
        return false;
    }
    n = cJSON_GetArraySize(v);
    if (n < min_len || n > max_len) {
        return false;
    }
    for (const cJSON *e = v->child; e != NULL; e = e->next) {
        if (!cJSON_IsNumber(e)) {
            return false;
        }
    }
    return true;
}

static void check_number(dspg_report_t *r, const char *path, const cJSON *v, double lo, double hi)
{
    if (!cJSON_IsNumber(v)) {
        dspg_report_add(r, E_SCHEMA, path, "must be a number");
    } else if (v->valuedouble < lo || v->valuedouble > hi) {
        dspg_report_add(r, E_NODE_PARAM, path, "value out of range");
    }
}

static void check_integer(dspg_report_t *r, const char *path, const cJSON *v, double lo, double hi)
{
    if (!is_integer(v)) {
        dspg_report_add(r, E_SCHEMA, path, "must be an integer");
    } else if (v->valuedouble < lo || v->valuedouble > hi) {
        dspg_report_add(r, E_NODE_PARAM, path, "value out of range");
    }
}

static void check_enum(dspg_report_t *r, const char *path, const cJSON *v,
                       const char *const *set, size_t n)
{
    if (!in_set(v, set, n)) {
        dspg_report_add(r, E_SCHEMA, path, "not an allowed value");
    }
}

static void check_order(dspg_report_t *r, const char *path, const cJSON *v, const cJSON *node)
{
    const cJSON *family = cJSON_GetObjectItemCaseSensitive(node, "family");
    check_integer(r, path, v, ORDER_MIN, ORDER_MAX);
    if (is_integer(v) && cJSON_IsString(family) &&
        strcmp(family->valuestring, "Linkwitz-Riley") == 0 &&
        v->valuedouble >= ORDER_MIN && v->valuedouble <= ORDER_MAX &&
        fmod(v->valuedouble, 2.0) != 0.0) {
        dspg_report_add(r, E_NODE_PARAM, path, "Linkwitz-Riley order must be even");
    }
}

static void check_key(dspg_report_t *r, const char *path, node_key_t k, const cJSON *v,
                      const char *type, const cJSON *node)
{
    switch (k) {
    case NK_FREQ:
        if (cJSON_IsString(v)) {
            if (!is_var_ref(v->valuestring)) {
                dspg_report_add(r, E_SCHEMA, path, "must be a number or @var");
            }
        } else {
            check_number(r, path, v, FREQ_MIN_HZ, FREQ_MAX_HZ);
        }
        break;
    case NK_FAMILY:
        check_enum(r, path, v, FAMILIES, COUNT_OF(FAMILIES));
        if (cJSON_IsString(v) && strcmp(v->valuestring, "Bessel") == 0) {
            dspg_report_add(r, E_UNSUPPORTED, path, "Bessel is not supported yet");
        }
        break;
    case NK_MODE:
        check_enum(r, path, v, MODES, COUNT_OF(MODES));
        break;
    case NK_SHAPE:
        check_enum(r, path, v, SHAPES, COUNT_OF(SHAPES));
        break;
    case NK_ORDER:
        check_order(r, path, v, node);
        break;
    case NK_CHANNEL:
    case NK_SLOT:
        if (!is_integer(v)) {
            dspg_report_add(r, E_SCHEMA, path, "must be an integer");
        }
        break;
    case NK_GAINS:
        if (!is_number_array(v, MIX_MIN_GAINS, MIX_MAX_GAINS)) {
            dspg_report_add(r, E_SCHEMA, path, "must be 2..8 numbers");
        }
        break;
    case NK_DB:
        if (strcmp(type, "gain") == 0) {
            check_number(r, path, v, GAIN_DB_MIN, GAIN_DB_MAX);
        } else {
            check_number(r, path, v, EQ_DB_MIN, EQ_DB_MAX);
        }
        break;
    case NK_Q:
        check_number(r, path, v, Q_MIN, Q_MAX);
        break;
    case NK_INVERT:
        if (!cJSON_IsBool(v)) {
            dspg_report_add(r, E_SCHEMA, path, "must be a boolean");
        }
        break;
    case NK_B:
        if (!is_number_array(v, BIQUAD_B_LEN, BIQUAD_B_LEN)) {
            dspg_report_add(r, E_SCHEMA, path, "must be 3 numbers");
        }
        break;
    case NK_A:
        if (!is_number_array(v, BIQUAD_A_LEN, BIQUAD_A_LEN)) {
            dspg_report_add(r, E_SCHEMA, path, "must be 2 numbers");
        }
        break;
    case NK_FS:
        if (!cJSON_IsNumber(v) || v->valuedouble != SAMPLE_RATE_HZ) {
            dspg_report_add(r, E_SCHEMA, path, "sample_rate_hz must be 48000");
        }
        break;
    case NK_DELAY:
        check_number(r, path, v, 0.0, DELAY_US_MAX);
        break;
    case NK_ID:
    case NK_TYPE:
    case NK_COUNT:
        break;
    }
}

static int find_key(const char *name)
{
    for (int k = 0; k < (int)NK_COUNT; k++) {
        if (strcmp(name, KEY_NAME[k]) == 0) {
            return k;
        }
    }
    return -1;
}

static bool biquad_coeff(const cJSON *arr, int index, float *out)
{
    const cJSON *e = cJSON_GetArrayItem(arr, index);
    if (!cJSON_IsNumber(e) || !isfinite(e->valuedouble) || fabs(e->valuedouble) > (double)FLT_MAX) {
        return false;
    }
    *out = (float)e->valuedouble;
    return true;
}

/* Same test the engine applies (dsp_check_stable), on the float32 values the
 * DSP will run, so the validator and the engine cannot disagree. */
static void check_biquad_stable(dspg_report_t *r, unsigned index, const cJSON *node)
{
    const cJSON *b = cJSON_GetObjectItemCaseSensitive(node, "b");
    const cJSON *a = cJSON_GetObjectItemCaseSensitive(node, "a");
    char path[sizeof(((dspg_issue_t *)0)->path)];
    dsp_sos_t s = {0};

    (void)snprintf(path, sizeof(path), "nodes[%u].a", index);
    if (!biquad_coeff(b, 0, &s.b0) || !biquad_coeff(b, 1, &s.b1) || !biquad_coeff(b, 2, &s.b2) ||
        !biquad_coeff(a, 0, &s.a1) || !biquad_coeff(a, 1, &s.a2)) {
        dspg_report_add(r, E_NODE_PARAM, path, "biquad coefficients must be finite float32 values");
    } else if (dsp_check_stable(&s) != DSP_OK) {
        dspg_report_add(r, E_NODE_PARAM, path, "biquad poles are not inside the unit circle");
    }
}

static void validate_node(dspg_report_t *r, unsigned index, const cJSON *node)
{
    char base[LABEL_LEN];
    char path[sizeof(((dspg_issue_t *)0)->path)];
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(node, "id");
    const cJSON *type = cJSON_GetObjectItemCaseSensitive(node, "type");
    const node_spec_t *spec = NULL;
    const uint8_t issues_before = r->n;

    (void)snprintf(base, sizeof(base), "nodes[%u]", index);
    if (!cJSON_IsObject(node)) {
        dspg_report_add(r, E_SCHEMA, base, "node must be an object");
        return;
    }
    if (!cJSON_IsString(id) || id->valuestring[0] == '\0' || strlen(id->valuestring) > MAX_ID_LEN) {
        dspg_report_add(r, E_SCHEMA, base, "id must be a string of 1..31 characters");
    }
    for (size_t i = 0U; i < SPEC_COUNT && cJSON_IsString(type); i++) {
        if (strcmp(type->valuestring, SPECS[i].name) == 0) {
            spec = &SPECS[i];
        }
    }
    if (spec == NULL) {
        dspg_report_add(r, E_SCHEMA, base, "unknown or missing node type");
        return;
    }
    for (const cJSON *e = node->child; e != NULL; e = e->next) {
        int k = (e->string != NULL) ? find_key(e->string) : -1;
        if (k < 0 || (spec->keys & BIT(k)) == 0U) {
            (void)snprintf(path, sizeof(path), "%s", base);
            dspg_report_add(r, E_SCHEMA, path, "property not allowed for this node type");
            continue;
        }
        (void)snprintf(path, sizeof(path), "%s.%s", base, KEY_NAME[k]);
        check_key(r, path, (node_key_t)k, e, spec->name, node);
    }
    for (int k = 0; k < (int)NK_COUNT; k++) {
        if ((spec->keys & BIT(k)) != 0U &&
            cJSON_GetObjectItemCaseSensitive(node, KEY_NAME[k]) == NULL) {
            (void)snprintf(path, sizeof(path), "%s.%s", base, KEY_NAME[k]);
            dspg_report_add(r, E_NODE_PARAM, path, "required parameter missing");
        }
    }
    if (strcmp(spec->name, "biquad") == 0 && r->n == issues_before) {
        check_biquad_stable(r, index, node);
    }
}

static void validate_top_level(dspg_report_t *r, const cJSON *root)
{
    const cJSON *v;
    for (const cJSON *e = root->child; e != NULL; e = e->next) {
        if (e->string == NULL || !is_top_key(e->string)) {
            dspg_report_add(r, E_SCHEMA, "", "unknown top-level key");
        }
    }
    v = cJSON_GetObjectItemCaseSensitive(root, "version");
    if (!cJSON_IsNumber(v) || v->valuedouble != CONFIG_VERSION) {
        dspg_report_add(r, E_SCHEMA, "version", "version must be 1");
    }
    v = cJSON_GetObjectItemCaseSensitive(root, "sample_rate_hz");
    if (!cJSON_IsNumber(v) || v->valuedouble != SAMPLE_RATE_HZ) {
        dspg_report_add(r, E_SCHEMA, "sample_rate_hz", "sample_rate_hz must be 48000");
    }
}

int dspg_validate_tree(const cJSON *root, dspg_report_t *report)
{
    const cJSON *nodes;
    const cJSON *edges;
    const cJSON *vars;
    const cJSON *limits;
    unsigned index = 0U;

    if (!cJSON_IsObject(root)) {
        dspg_report_add(report, E_SCHEMA, "", "config must be an object");
        return -1;
    }
    validate_top_level(report, root);
    vars = cJSON_GetObjectItemCaseSensitive(root, "vars");
    if (vars != NULL) {
        for (const cJSON *e = cJSON_IsObject(vars) ? vars->child : NULL; e != NULL; e = e->next) {
            if (!cJSON_IsNumber(e)) {
                dspg_report_add(report, E_SCHEMA, "vars", "variables must be numbers");
            }
        }
        if (!cJSON_IsObject(vars)) {
            dspg_report_add(report, E_SCHEMA, "vars", "vars must be an object");
        }
    }
    limits = cJSON_GetObjectItemCaseSensitive(root, "limits");
    if (limits != NULL) {
        if (!cJSON_IsObject(limits)) {
            dspg_report_add(report, E_SCHEMA, "limits", "limits must be an object");
        }
        for (const cJSON *e = cJSON_IsObject(limits) ? limits->child : NULL; e != NULL; e = e->next) {
            const cJSON *lo = cJSON_GetObjectItemCaseSensitive(e, "min");
            const cJSON *hi = cJSON_GetObjectItemCaseSensitive(e, "max");
            if (!cJSON_IsObject(e) || !cJSON_IsNumber(lo) || !cJSON_IsNumber(hi) ||
                cJSON_GetArraySize(e) != 2) {
                dspg_report_add(report, E_SCHEMA, "limits", "each limit needs numeric min and max only");
            }
        }
    }
    nodes = cJSON_GetObjectItemCaseSensitive(root, "nodes");
    if (!cJSON_IsArray(nodes)) {
        dspg_report_add(report, E_SCHEMA, "nodes", "nodes must be an array");
    } else if (cJSON_GetArraySize(nodes) > MAX_NODES) {
        dspg_report_add(report, E_RESOURCE, "nodes", "more than 64 nodes");
    } else {
        for (const cJSON *n = nodes->child; n != NULL; n = n->next) {
            validate_node(report, index, n);
            index++;
        }
    }
    edges = cJSON_GetObjectItemCaseSensitive(root, "edges");
    if (!cJSON_IsArray(edges)) {
        dspg_report_add(report, E_SCHEMA, "edges", "edges must be an array");
    } else {
        for (const cJSON *e = edges->child; e != NULL; e = e->next) {
            if (!cJSON_IsArray(e) || cJSON_GetArraySize(e) != EDGE_LEN ||
                !cJSON_IsString(e->child) || !cJSON_IsString(e->child->next)) {
                dspg_report_add(report, E_SCHEMA, "edges", "each edge must be two ids");
            }
        }
    }
    return (report->n == 0U) ? 0 : -1;
}
