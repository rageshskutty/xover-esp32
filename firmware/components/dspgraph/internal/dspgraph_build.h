#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "cJSON.h"
#include "dspblocks.h"

#define B_MAX_IN 8U
#define B_ID_LEN 32U
#define B_FS_HZ 48000.0

typedef enum {
    K_INPUT = 0,
    K_OUTPUT,
    K_CASCADE,
    K_GAIN,
    K_DELAY,
    K_POLARITY,
    K_MIX
} bkind_t;

/* One compiled node: designed in double precision, stored as float32. */
typedef struct {
    char id[B_ID_LEN];
    bkind_t kind;
    dsp_cascade_t casc;
    float gain;
    float gains[B_MAX_IN];
    uint32_t delay;
    bool invert;
} bnode_t;

/* Core 0 only. Designs one validated node. `vars` is the config's "vars"
 * object (may be NULL). Returns DSP_OK, or the design error when a parameter
 * is missing, not finite, out of range or unstable; *out is then undefined. */
dsp_err_t dspg_compile_node(const cJSON *vars, const cJSON *node, bnode_t *out);
