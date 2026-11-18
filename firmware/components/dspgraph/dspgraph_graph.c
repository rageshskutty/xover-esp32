#include "dspgraph_graph.h"

#include <stdbool.h>
#include <string.h>

#include "dspgraph_validate.h"

#define G_MAX_NODES 64U
#define G_MAX_EDGES 512U /* 64 nodes x 8 mix inputs; more always breaks a fan-in rule */
#define G_MAX_SECTIONS 96U
#define G_N_SLOTS 8U
#define G_FREQ_MIN_HZ 10.0
#define G_FREQ_MAX_HZ 21600.0
#define G_LR_PAIR 2U
#define G_NONE 0xFFU

typedef struct {
    const cJSON *node;
    uint8_t in_deg;
    uint8_t out_deg;
    uint8_t kahn_deg;
} gnode_t;

static const char *str_of(const cJSON *n, const char *key)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(n, key);
    return cJSON_IsString(v) ? v->valuestring : "";
}

static bool is_type(const gnode_t *g, const char *type)
{
    return strcmp(str_of(g->node, "type"), type) == 0;
}

static uint32_t sections_of(const cJSON *node)
{
    const char *t = str_of(node, "type");
    const cJSON *order = cJSON_GetObjectItemCaseSensitive(node, "order");
    if (strcmp(t, "crossover") == 0 && cJSON_IsNumber(order) && order->valuedouble >= 1.0) {
        uint32_t n = (uint32_t)order->valuedouble;
        return (strcmp(str_of(node, "family"), "Linkwitz-Riley") == 0) ? n / G_LR_PAIR
                                                                         : (n + 1U) / G_LR_PAIR;
    }
    if (strcmp(t, "peq") == 0 || strcmp(t, "shelf") == 0 || strcmp(t, "allpass") == 0 ||
        strcmp(t, "notch") == 0 || strcmp(t, "biquad") == 0) {
        return 1U;
    }
    return 0U;
}

static uint8_t find_id(const gnode_t *g, uint8_t n, const char *id)
{
    for (uint8_t i = 0U; i < n; i++) {
        if (strcmp(str_of(g[i].node, "id"), id) == 0) {
            return i;
        }
    }
    return G_NONE;
}

static void check_limits(const cJSON *vars, const cJSON *limits, dspg_report_t *r)
{
    for (const cJSON *l = (limits != NULL) ? limits->child : NULL; l != NULL; l = l->next) {
        const cJSON *v = cJSON_GetObjectItemCaseSensitive(vars, l->string);
        const cJSON *lo = cJSON_GetObjectItemCaseSensitive(l, "min");
        const cJSON *hi = cJSON_GetObjectItemCaseSensitive(l, "max");
        if (v == NULL) {
            dspg_report_add(r, "E_VAR_UNDEFINED", "limits", "limit for an undeclared variable");
        } else if (lo->valuedouble >= hi->valuedouble) {
            dspg_report_add(r, "E_LIMIT", "limits", "min must be below max");
        } else if (v->valuedouble < lo->valuedouble || v->valuedouble > hi->valuedouble) {
            dspg_report_add(r, "E_LIMIT", "vars", "variable outside its limits");
        }
    }
}

static void check_freq_var(const cJSON *vars, const gnode_t *g, dspg_report_t *r)
{
    const cJSON *f = cJSON_GetObjectItemCaseSensitive(g->node, "frequency_hz");
    const cJSON *v;
    if (!cJSON_IsString(f)) {
        return;
    }
    v = cJSON_GetObjectItemCaseSensitive(vars, &f->valuestring[1]);
    if (!cJSON_IsNumber(v)) {
        dspg_report_add(r, "E_VAR_UNDEFINED", "nodes", "frequency_hz uses an undeclared variable");
    } else if (v->valuedouble < G_FREQ_MIN_HZ || v->valuedouble > G_FREQ_MAX_HZ) {
        dspg_report_add(r, "E_NODE_PARAM", "nodes", "frequency_hz variable outside 10..21600");
    }
}

static bool has_cycle(const gnode_t *g, uint8_t n, const uint8_t (*e)[2], size_t n_edges)
{
    gnode_t work[G_MAX_NODES];
    uint8_t queue[G_MAX_NODES];
    size_t head = 0U;
    size_t tail = 0U;
    for (uint8_t i = 0U; i < n; i++) {
        work[i] = g[i];
        work[i].kahn_deg = g[i].in_deg;
        if (work[i].kahn_deg == 0U) {
            queue[tail++] = i;
        }
    }
    while (head < tail) {
        uint8_t x = queue[head++];
        for (size_t k = 0U; k < n_edges; k++) {
            if (e[k][0] == x && --work[e[k][1]].kahn_deg == 0U) {
                queue[tail++] = e[k][1];
            }
        }
    }
    return tail != n;
}

static void check_node(const cJSON *vars, const gnode_t *g, dspg_report_t *r)
{
    const cJSON *gains = cJSON_GetObjectItemCaseSensitive(g->node, "gains");
    bool is_out = is_type(g, "output");
    check_freq_var(vars, g, r);
    if (is_type(g, "input")) {
        if (g->in_deg != 0U) {
            dspg_report_add(r, "E_GRAPH_FANIN", "edges", "input has an incoming edge");
        }
    } else if (is_type(g, "mix")) {
        if (g->in_deg < 2U || g->in_deg != (uint8_t)cJSON_GetArraySize(gains)) {
            dspg_report_add(r, "E_GRAPH_FANIN", "edges", "mix needs one gain per incoming edge");
        }
    } else if (g->in_deg != 1U) {
        dspg_report_add(r, "E_GRAPH_FANIN", "edges", "node needs exactly one incoming edge");
    }
    if (is_out && g->out_deg != 0U) {
        dspg_report_add(r, "E_GRAPH_DEAD", "edges", "output has an outgoing edge");
    } else if (!is_out && g->out_deg == 0U) {
        dspg_report_add(r, "E_GRAPH_DEAD", "edges", "node has no outgoing edge");
    }
}

static void check_slots(const gnode_t *g, uint8_t n, dspg_report_t *r)
{
    uint32_t slots = 0U;
    uint32_t chans = 0U;
    bool any_output = false;
    for (uint8_t i = 0U; i < n; i++) {
        const cJSON *s = cJSON_GetObjectItemCaseSensitive(g[i].node, "slot");
        const cJSON *c = cJSON_GetObjectItemCaseSensitive(g[i].node, "channel");
        if (is_type(&g[i], "output") && cJSON_IsNumber(s)) {
            uint32_t bit = 1U << (uint32_t)s->valuedouble;
            any_output = true;
            if ((slots & bit) != 0U) {
                dspg_report_add(r, "E_SLOT", "nodes", "duplicate output slot");
            }
            slots |= bit;
        }
        if (is_type(&g[i], "input") && cJSON_IsNumber(c)) {
            uint32_t bit = 1U << (uint32_t)c->valuedouble;
            if ((chans & bit) != 0U) {
                dspg_report_add(r, "E_SLOT", "nodes", "duplicate input channel");
            }
            chans |= bit;
        }
    }
    if (!any_output) {
        dspg_report_add(r, "E_GRAPH_DEAD", "nodes", "no output node");
    }
}

int dspg_validate_graph(const cJSON *root, dspg_report_t *report)
{
    const cJSON *nodes = cJSON_GetObjectItemCaseSensitive(root, "nodes");
    const cJSON *edges = cJSON_GetObjectItemCaseSensitive(root, "edges");
    const cJSON *vars = cJSON_GetObjectItemCaseSensitive(root, "vars");
    static const cJSON empty_vars;
    gnode_t g[G_MAX_NODES];
    uint8_t e[G_MAX_EDGES][2];
    uint8_t n = 0U;
    size_t n_edges = 0U;
    size_t n_sections = 0U;
    bool edges_ok = true;
    uint8_t before = report->n;

    memset(g, 0, sizeof(g));
    for (const cJSON *c = nodes->child; c != NULL && n < G_MAX_NODES; c = c->next) {
        if (find_id(g, n, str_of(c, "id")) != G_NONE) {
            dspg_report_add(report, "E_GRAPH_REF", "nodes", "duplicate node id");
        }
        g[n++].node = c;
    }
    check_limits((vars != NULL) ? vars : &empty_vars, cJSON_GetObjectItemCaseSensitive(root, "limits"), report);
    for (const cJSON *c = edges->child; c != NULL; c = c->next) {
        uint8_t a = find_id(g, n, c->child->valuestring);
        uint8_t b = find_id(g, n, c->child->next->valuestring);
        if (a == G_NONE || b == G_NONE || a == b) {
            dspg_report_add(report, "E_GRAPH_REF", "edges", "edge uses an unknown id or loops");
            edges_ok = false;
            continue;
        }
        if (g[a].out_deg < UINT8_MAX) {
            g[a].out_deg++;
        }
        if (g[b].in_deg < UINT8_MAX) {
            g[b].in_deg++;
        }
        if (n_edges < G_MAX_EDGES) {
            e[n_edges][0] = a;
            e[n_edges][1] = b;
        } else {
            edges_ok = false;
        }
        n_edges++;
    }
    if (edges_ok && has_cycle(g, n, (const uint8_t (*)[2])e, n_edges)) {
        dspg_report_add(report, "E_GRAPH_CYCLE", "edges", "graph is not a DAG");
    }
    for (uint8_t i = 0U; i < n; i++) {
        check_node((vars != NULL) ? vars : &empty_vars, &g[i], report);
        n_sections += sections_of(g[i].node);
    }
    check_slots(g, n, report);
    if (n_sections > G_MAX_SECTIONS) {
        dspg_report_add(report, "E_RESOURCE", "nodes", "more than 96 biquad sections");
    }
    return (report->n == before) ? 0 : -1;
}
