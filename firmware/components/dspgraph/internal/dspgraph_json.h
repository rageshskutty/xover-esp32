#pragma once

#include <stddef.h>
#include "cJSON.h"

#define DSPG_JSON_MAX_BYTES 32768U
#define DSPG_JSON_MAX_DEPTH 8U

typedef struct {
    char code[20];
    char path[48];
    char msg[96];
} dspg_parse_issue_t;

/* Returns a cJSON root for bounded, syntactically valid JSON; caller deletes it. */
cJSON *dspg_parse_bounded_json(const char *json, size_t len, dspg_parse_issue_t *issue);
