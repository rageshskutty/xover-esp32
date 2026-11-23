#include "tdm_pack.h"

int tdm_pack_8slot(const int32_t *const channels[TDM_PACK_AUDIO_CHANNELS], size_t n_frames,
                   int32_t *interleaved, size_t out_words)
{
    if (n_frames == 0U) {
        return 0;
    }
    if (channels == NULL || interleaved == NULL) {
        return -1;
    }
    for (size_t ch = 0U; ch < TDM_PACK_AUDIO_CHANNELS; ch++) {
        if (channels[ch] == NULL) {
            return -1;
        }
    }
    if (n_frames > SIZE_MAX / TDM_PACK_SLOTS_PER_FRAME) {
        return -1;
    }
    if (out_words < n_frames * TDM_PACK_SLOTS_PER_FRAME) {
        return -1;
    }
    for (size_t f = 0U; f < n_frames; f++) {
        int32_t *frame = &interleaved[f * TDM_PACK_SLOTS_PER_FRAME];
        for (size_t ch = 0U; ch < TDM_PACK_AUDIO_CHANNELS; ch++) {
            frame[ch] = channels[ch][f];
        }
        frame[TDM_PACK_AUDIO_CHANNELS] = 0;
        frame[TDM_PACK_AUDIO_CHANNELS + 1U] = 0;
    }
    return 0;
}
