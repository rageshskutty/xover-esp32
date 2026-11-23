#ifndef BRINGUP_TONE_H
#define BRINGUP_TONE_H

#include <stddef.h>
#include <stdint.h>

#define BRINGUP_TONE_CHANNELS 6U
#define BRINGUP_TONE_FS_HZ 48000U
#define BRINGUP_TONE_MAX_DBFS (-30.0f)
#define BRINGUP_TONE_MIN_DBFS (-120.0f)

extern const uint32_t bringup_tone_freq_hz[BRINGUP_TONE_CHANNELS];

int bringup_tone_fill(float level_dbfs, uint32_t start_frame, size_t n_frames,
                      int32_t *const channels[BRINGUP_TONE_CHANNELS]);

#endif /* BRINGUP_TONE_H */
