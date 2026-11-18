#pragma once

#include "cJSON.h"
#include "dspgraph.h"

/* Graph-level semantic rules (references, cycles, fan-in, slots, variables,
 * limits, section cap). Precondition: dspg_validate_tree found no structural
 * issue. Returns 0 when no issue was added. No heap, no recursion. */
int dspg_validate_graph(const cJSON *root, dspg_report_t *report);
