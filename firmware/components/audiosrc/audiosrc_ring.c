#include "audiosrc_ring.h"

#include <stdbool.h>

static bool ring_usable(const audiosrc_ring_t *ring)
{
    return ring != NULL && ring->storage != NULL &&
           ring->capacity >= AUDIOSRC_RING_MIN_CAPACITY &&
           ring->capacity <= AUDIOSRC_RING_MAX_CAPACITY;
}

/* Frames between tail and head. 2 * capacity cannot overflow under the capacity bound. */
static size_t counter_used(size_t head, size_t tail, size_t capacity)
{
    return (head >= tail) ? (head - tail) : ((2U * capacity) - (tail - head));
}

/* counter + n modulo 2 * capacity, for n <= capacity, without an intermediate overflow. */
static size_t counter_advance(size_t counter, size_t n, size_t capacity)
{
    const size_t span = 2U * capacity;
    return (counter >= (span - n)) ? (counter - (span - n)) : (counter + n);
}

int audiosrc_ring_init(audiosrc_ring_t *ring, audiosrc_frame_t *storage, size_t capacity)
{
    if (ring == NULL || storage == NULL ||
        capacity < AUDIOSRC_RING_MIN_CAPACITY || capacity > AUDIOSRC_RING_MAX_CAPACITY) {
        return AUDIOSRC_RING_E_ARG;
    }
    ring->storage = storage;
    ring->capacity = capacity;
    atomic_init(&ring->head, 0U);
    atomic_init(&ring->tail, 0U);
    return AUDIOSRC_RING_OK;
}

size_t audiosrc_ring_write(audiosrc_ring_t *ring, const audiosrc_frame_t *frames, size_t n)
{
    if (!ring_usable(ring) || frames == NULL || n == 0U) {
        return 0U;
    }
    const size_t head = atomic_load_explicit(&ring->head, memory_order_relaxed);
    const size_t tail = atomic_load_explicit(&ring->tail, memory_order_acquire);
    const size_t space = ring->capacity - counter_used(head, tail, ring->capacity);
    const size_t count = (n < space) ? n : space;
    size_t slot = head % ring->capacity;
    for (size_t i = 0U; i < count; ++i) {
        ring->storage[slot] = frames[i];
        slot = (slot + 1U == ring->capacity) ? 0U : (slot + 1U);
    }
    if (count > 0U) {
        atomic_store_explicit(&ring->head, counter_advance(head, count, ring->capacity),
                              memory_order_release);
    }
    return count;
}

size_t audiosrc_ring_read(audiosrc_ring_t *ring, audiosrc_frame_t *frames, size_t n)
{
    if (!ring_usable(ring) || frames == NULL || n == 0U) {
        return 0U;
    }
    const size_t tail = atomic_load_explicit(&ring->tail, memory_order_relaxed);
    const size_t head = atomic_load_explicit(&ring->head, memory_order_acquire);
    const size_t avail = counter_used(head, tail, ring->capacity);
    const size_t count = (n < avail) ? n : avail;
    size_t slot = tail % ring->capacity;
    for (size_t i = 0U; i < count; ++i) {
        frames[i] = ring->storage[slot];
        slot = (slot + 1U == ring->capacity) ? 0U : (slot + 1U);
    }
    if (count > 0U) {
        atomic_store_explicit(&ring->tail, counter_advance(tail, count, ring->capacity),
                              memory_order_release);
    }
    return count;
}

size_t audiosrc_ring_count(const audiosrc_ring_t *ring)
{
    if (!ring_usable(ring)) {
        return 0U;
    }
    const size_t tail = atomic_load_explicit(&ring->tail, memory_order_acquire);
    const size_t head = atomic_load_explicit(&ring->head, memory_order_acquire);
    return counter_used(head, tail, ring->capacity);
}
