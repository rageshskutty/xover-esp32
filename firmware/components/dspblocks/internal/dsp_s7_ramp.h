#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DSP_S7_OK 0
#define DSP_S7_E_ARG (-1)
/* AGENTS.md S7: linear per-sample gain step bound; a full-scale input step is then <= -60 dBFS. */
#define DSP_S7_MAX_STEP 0.001f
/* 20 ms up and 5 ms down at 48 kHz. */
#define DSP_S7_MIN_UP_FRAMES 960U
#define DSP_S7_MIN_DOWN_FRAMES 240U

/*
 * Number of frames for a linear gain ramp from current to target, both in [0, 1].
 * Pass the result unchanged to dsp_gain_set_target. Equal gains give 0 frames.
 * Returns DSP_S7_OK, or DSP_S7_E_ARG for a null output, a non-finite gain or a gain
 * outside [0, 1]; on error *frames is unchanged.
 *
 * This bounds the commanded step (delta / frames <= DSP_S7_MAX_STEP). The executor
 * dsp_gain_process accumulates in float32, so its observed per-sample change can be
 * slightly larger; this helper does not guarantee <= -60 dBFS at the output.
 * It does not decide whether a start, stop, swap or fault is safe to begin.
 */
int dsp_s7_ramp_frames(float current, float target, uint32_t *frames);

#ifdef __cplusplus
}
#endif
