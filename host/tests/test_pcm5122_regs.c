#include "pcm5122_regs.h"
#include "board_config.h"

#include <stdint.h>
#include <stdio.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static int expect_writes(uint32_t offset)
{
    pcm5122_reg_write_t w[PCM5122_TDM_WRITE_COUNT];
    CHECK(pcm5122_tdm_writes(offset, w) == 0);
    CHECK(w[0].reg == 0x00U && w[0].val == 0x00U);
    CHECK(w[1].reg == 0x28U && w[1].val == 0x13U);
    CHECK(w[2].reg == 0x29U && w[2].val == (uint8_t)offset);
    return 0;
}

int main(void)
{
    CHECK(PCM5122_TDM_WRITE_COUNT == 3U);
    CHECK(PCM5122_AUDIO_FORMAT_TDM_32 == 0x13U);
    CHECK(expect_writes(0U) == 0);
    CHECK(expect_writes(1U) == 0);
    CHECK(expect_writes(64U) == 0);
    CHECK(expect_writes(128U) == 0);
    CHECK(expect_writes(255U) == 0);

    /* The offsets chosen in board_config.h produce the datasheet register values. */
    CHECK(expect_writes((uint32_t)XOVER_DAC0_TDM_OFFSET_BCK) == 0);
    CHECK(expect_writes((uint32_t)XOVER_DAC1_TDM_OFFSET_BCK) == 0);
    CHECK(expect_writes((uint32_t)XOVER_DAC2_TDM_OFFSET_BCK) == 0);

    {
        pcm5122_reg_write_t w[PCM5122_TDM_WRITE_COUNT];
        for (size_t i = 0U; i < PCM5122_TDM_WRITE_COUNT; i++) { w[i].reg = 0xEEU; w[i].val = 0xEEU; }
        CHECK(pcm5122_tdm_writes(256U, w) == -1);
        CHECK(pcm5122_tdm_writes(UINT32_MAX, w) == -1);
        for (size_t i = 0U; i < PCM5122_TDM_WRITE_COUNT; i++) CHECK(w[i].reg == 0xEEU && w[i].val == 0xEEU);
    }
    CHECK(pcm5122_tdm_writes(0U, NULL) == -1);
    puts("PASS: PCM5122 TDM register writes");
    return 0;
}
