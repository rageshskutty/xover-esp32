#include "pcm5122_regs.h"

#include <stddef.h>
#include <stdint.h>

int pcm5122_tdm_writes(
    uint32_t offset_bck,
    pcm5122_reg_write_t out[PCM5122_TDM_WRITE_COUNT]
)
{
    if (out == NULL || offset_bck > PCM5122_TDM_OFFSET_MAX) {
        return -1;
    }

    out[0].reg = PCM5122_REG_PAGE_SELECT;
    out[0].val = 0x00U;

    out[1].reg = PCM5122_REG_AUDIO_FORMAT;
    out[1].val = (uint8_t)PCM5122_AUDIO_FORMAT_TDM_32;

    out[2].reg = PCM5122_REG_AUDIO_OFFSET;
    out[2].val = (uint8_t)offset_bck;

    return 0;
}
