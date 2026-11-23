#pragma once

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIOSRC_RING_OK 0
#define AUDIOSRC_RING_E_ARG (-1)
#define AUDIOSRC_RING_MIN_CAPACITY 2U
#define AUDIOSRC_RING_MAX_CAPACITY (SIZE_MAX / 2U)

typedef struct {
    int32_t left, right;
} audiosrc_frame_t;

/*
 * Bounded single-producer single-consumer ring of stereo frames.
 * head and tail are counters modulo 2 * capacity; the slot is counter % capacity,
 * so all `capacity` slots are usable and full is distinguishable from empty.
 * One thread may call write, another may call read. No locks, no allocation,
 * no blocking: both return immediately with the number of frames transferred.
 */
typedef struct {
    audiosrc_frame_t *storage;
    size_t capacity;
    atomic_size_t head, tail;
} audiosrc_ring_t;

/*
 * The caller owns storage for the lifetime of the ring.
 * Returns AUDIOSRC_RING_OK, or AUDIOSRC_RING_E_ARG for a null pointer or a capacity
 * outside [AUDIOSRC_RING_MIN_CAPACITY, AUDIOSRC_RING_MAX_CAPACITY]. On error nothing is modified.
 * Not thread safe: call before the producer and consumer start.
 */
int audiosrc_ring_init(audiosrc_ring_t *ring, audiosrc_frame_t *storage, size_t capacity);

/* Producer only. Copies up to n frames; returns the count stored. Invalid arguments return 0. */
size_t audiosrc_ring_write(audiosrc_ring_t *ring, const audiosrc_frame_t *frames, size_t n);

/* Consumer only. Copies up to n frames; returns the count read. Invalid arguments return 0. */
size_t audiosrc_ring_read(audiosrc_ring_t *ring, audiosrc_frame_t *frames, size_t n);

/* Frames currently queued. Exact when called from the producer or the consumer thread. */
size_t audiosrc_ring_count(const audiosrc_ring_t *ring);

#ifdef __cplusplus
}
#endif
