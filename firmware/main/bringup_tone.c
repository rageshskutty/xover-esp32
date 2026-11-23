#include "bringup_tone.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#define BRINGUP_TONE_TWO_PI (2.0f * 3.14159265358979323846f)
#define BRINGUP_TONE_FULL_SCALE 2147483647.0f
#define BRINGUP_TONE_DB_SCALE 20.0f
#define BRINGUP_TONE_BASE_10 10.0f

const uint32_t bringup_tone_freq_hz[BRINGUP_TONE_CHANNELS] = {
    400U, 700U, 1100U, 1700U, 2300U, 3100U
};

int bringup_tone_fill(float level_dbfs, uint32_t start_frame, size_t n_frames,
                      int32_t *const channels[BRINGUP_TONE_CHANNELS])
{
    /* 1. Return -1 if level_dbfs is not finite, above -30.0 or below -120.0 */
    if (!isfinite(level_dbfs) ||
        level_dbfs > BRINGUP_TONE_MAX_DBFS ||
        level_dbfs < BRINGUP_TONE_MIN_DBFS) {
        return -1;
    }

    /* 2. If n_frames == 0 return 0 */
    if (n_frames == 0U) {
        return 0;
    }

    /* 3. Return -1 if channels or any channels[i] is NULL */
    if (channels == NULL) {
        return -1;
    }
    for (size_t ch = 0U; ch < BRINGUP_TONE_CHANNELS; ch++) {
        if (channels[ch] == NULL) {
            return -1;
        }
    }

    /* 4. Generate sine tones for each channel */
    const float amp = powf(BRINGUP_TONE_BASE_10, level_dbfs / BRINGUP_TONE_DB_SCALE) *
                      BRINGUP_TONE_FULL_SCALE;
    const float fs = (float)BRINGUP_TONE_FS_HZ;

    for (size_t ch = 0U; ch < BRINGUP_TONE_CHANNELS; ch++) {
        const uint64_t freq = (uint64_t)bringup_tone_freq_hz[ch];
        int32_t *out = channels[ch];

        for (size_t i = 0U; i < n_frames; i++) {
            const uint64_t n = (uint64_t)start_frame + (uint64_t)i;
            const uint32_t idx = (uint32_t)((n * freq) % (uint64_t)BRINGUP_TONE_FS_HZ);
            const float phase = (BRINGUP_TONE_TWO_PI * (float)idx) / fs;
            out[i] = (int32_t)lrintf(amp * sinf(phase));
        }
    }

    return 0;
}
