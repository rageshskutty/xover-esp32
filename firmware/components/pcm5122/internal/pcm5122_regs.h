#ifndef PCM5122_REGS_H
#define PCM5122_REGS_H

#include <stdint.h>

#define PCM5122_REG_PAGE_SELECT 0x00U
#define PCM5122_REG_AUDIO_FORMAT 0x28U
#define PCM5122_REG_AUDIO_OFFSET 0x29U

#define PCM5122_AFMT_TDM_DSP 0x01U
#define PCM5122_ALEN_32BIT 0x03U

#define PCM5122_AUDIO_FORMAT_TDM_32 \
    ((PCM5122_AFMT_TDM_DSP << 4) | PCM5122_ALEN_32BIT)

#define PCM5122_TDM_WRITE_COUNT 3U
#define PCM5122_TDM_OFFSET_MAX 255U

typedef struct {
    uint8_t reg;
    uint8_t val;
} pcm5122_reg_write_t;

int pcm5122_tdm_writes(
    uint32_t offset_bck,
    pcm5122_reg_write_t out[PCM5122_TDM_WRITE_COUNT]
);

#endif /* PCM5122_REGS_H */
