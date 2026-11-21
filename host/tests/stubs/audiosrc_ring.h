#pragma once
/* Stub of the T16 contract (docs/engineering/prompts/T16.md). Delete when the
 * real firmware/components/audiosrc/internal/audiosrc_ring.h lands. */
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

typedef struct { int32_t left, right; } audiosrc_frame_t;
typedef struct {
    audiosrc_frame_t *storage;
    size_t capacity;
    atomic_size_t head, tail;
} audiosrc_ring_t;
int audiosrc_ring_init(audiosrc_ring_t *, audiosrc_frame_t *storage, size_t capacity);
size_t audiosrc_ring_write(audiosrc_ring_t *, const audiosrc_frame_t *, size_t n);
size_t audiosrc_ring_read(audiosrc_ring_t *, audiosrc_frame_t *, size_t n);
size_t audiosrc_ring_count(const audiosrc_ring_t *);
