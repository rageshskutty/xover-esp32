#pragma once

#include "cJSON.h"
#include "dspgraph.h"

/* Appends an issue; silently drops it once the report holds DSPG_MAX_ISSUES. */
void dspg_report_add(dspg_report_t *report, const char *code, const char *path, const char *msg);

/* Validates a parsed config tree. Returns 0 when no issue was added. */
int dspg_validate_tree(const cJSON *root, dspg_report_t *report);
