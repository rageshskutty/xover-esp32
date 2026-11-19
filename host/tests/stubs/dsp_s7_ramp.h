#pragma once
/* Stub of the T18 contract (docs/engineering/prompts/T18.md). Delete when the
 * real firmware/components/dspblocks/internal/dsp_s7_ramp.h lands. */
#include <stdint.h>

int dsp_s7_ramp_frames(float current, float target, uint32_t *frames);
