#include "dsp_s7_ramp.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>

/* Comparisons are false for NaN, so NaN and infinities fall outside [0, 1]. */
static bool gain_in_range(float gain)
{
    return gain >= 0.0f && gain <= 1.0f;
}

int dsp_s7_ramp_frames(float current, float target, uint32_t *frames)
{
    if (frames == NULL || !gain_in_range(current) || !gain_in_range(target)) {
        return DSP_S7_E_ARG;
    }
    if (current == target) {
        *frames = 0U;
        return DSP_S7_OK;
    }
    const float delta = fabsf(target - current);
    const uint32_t need = (uint32_t)ceilf(delta / DSP_S7_MAX_STEP);
    const uint32_t minimum = (target > current) ? DSP_S7_MIN_UP_FRAMES : DSP_S7_MIN_DOWN_FRAMES;
    *frames = (need > minimum) ? need : minimum;
    return DSP_S7_OK;
}
