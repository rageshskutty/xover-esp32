#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DSPG_MAX_ISSUES 8U

typedef struct {
    char code[20];
    char path[48];
    char msg[96];
} dspg_issue_t;

typedef struct {
    dspg_issue_t items[DSPG_MAX_ISSUES];
    uint8_t n;
} dspg_report_t;

/* Core 0 only. Parses and validates a config body without building a graph.
 * Returns 0 when valid, -1 when rejected; the report lists up to
 * DSPG_MAX_ISSUES issues using the codes in docs/api.md section 1. */
int dspg_validate(const char *json, size_t len, dspg_report_t *report);

#ifdef __cplusplus
}
#endif
