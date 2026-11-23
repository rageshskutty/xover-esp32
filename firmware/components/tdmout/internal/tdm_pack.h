#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Number of 32-bit words in one TDM frame (8 slots x 32 bit). */
#define TDM_PACK_SLOTS_PER_FRAME 8U
/* Number of planar audio channels carried in slots 0 to 5. */
#define TDM_PACK_AUDIO_CHANNELS 6U

/*
 * Pack six planar channels into frame-major TDM frames.
 * Slot i (0..5) takes channels[i]; slots 6 and 7 are zero. No scaling or byte swap.
 * Returns 0 on success (including n_frames == 0) and -1 on any invalid argument;
 * on -1 the output buffer is not modified.
 */
int tdm_pack_8slot(const int32_t *const channels[TDM_PACK_AUDIO_CHANNELS], size_t n_frames,
                   int32_t *interleaved, size_t out_words);

#ifdef __cplusplus
}
#endif
