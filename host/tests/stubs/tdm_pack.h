#pragma once
/* Stub of the T17 contract (docs/engineering/prompts/T17.md). Delete when the
 * real firmware/components/tdmout/internal/tdm_pack.h lands. */
#include <stddef.h>
#include <stdint.h>

int tdm_pack_8slot(const int32_t *const channels[6], size_t n_frames,
                   int32_t *interleaved, size_t out_words);
