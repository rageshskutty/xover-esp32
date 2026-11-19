#include "audiosrc_ring.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

#define THREAD_FRAMES 100000
#define THREAD_ATTEMPT_LIMIT ((size_t)THREAD_FRAMES * 100U)

typedef struct {
    audiosrc_ring_t *ring;
    atomic_bool failed;
} threaded_ring_t;

static void *threaded_writer(void *opaque)
{
    threaded_ring_t *ctx = opaque;
    size_t attempts = 0U;
    int32_t value = 1;
    while (value <= THREAD_FRAMES && attempts < THREAD_ATTEMPT_LIMIT) {
        const audiosrc_frame_t f = {value, -value};
        if (audiosrc_ring_write(ctx->ring, &f, 1U) == 1U) {
            ++value;
        } else {
            ++attempts;
        }
    }
    if (value <= THREAD_FRAMES) atomic_store(&ctx->failed, true);
    return NULL;
}

static void *threaded_reader(void *opaque)
{
    threaded_ring_t *ctx = opaque;
    size_t attempts = 0U;
    int32_t expected = 1;
    while (expected <= THREAD_FRAMES && attempts < THREAD_ATTEMPT_LIMIT) {
        audiosrc_frame_t f;
        if (audiosrc_ring_read(ctx->ring, &f, 1U) == 1U) {
            if (f.left != expected || f.right != -expected) {
                atomic_store(&ctx->failed, true);
                return NULL;
            }
            ++expected;
        } else {
            ++attempts;
        }
    }
    if (expected <= THREAD_FRAMES) atomic_store(&ctx->failed, true);
    return NULL;
}

static audiosrc_frame_t frame(int32_t n)
{
    audiosrc_frame_t f = {n, -n};
    return f;
}

int main(void)
{
    audiosrc_ring_t ring;
    audiosrc_frame_t storage[4], in[5], out[5];
    CHECK(audiosrc_ring_init(&ring, storage, 4U) == 0);
    CHECK(audiosrc_ring_count(&ring) == 0U);
    CHECK(audiosrc_ring_read(&ring, out, 1U) == 0U);

    for (int32_t i = 0; i < 3; i++) in[i] = frame(i + 1);
    CHECK(audiosrc_ring_write(&ring, in, 3U) == 3U);
    CHECK(audiosrc_ring_count(&ring) == 3U);
    CHECK(audiosrc_ring_read(&ring, out, 2U) == 2U);
    CHECK(out[0].left == 1 && out[1].left == 2);

    in[0] = frame(4); in[1] = frame(5); in[2] = frame(6);
    CHECK(audiosrc_ring_write(&ring, in, 3U) == 3U);
    CHECK(audiosrc_ring_count(&ring) == 4U);
    CHECK(audiosrc_ring_write(&ring, in, 1U) == 0U);
    CHECK(audiosrc_ring_read(&ring, out, 5U) == 4U);
    CHECK(out[0].left == 3 && out[1].left == 4 &&
          out[2].left == 5 && out[3].left == 6);
    CHECK(audiosrc_ring_count(&ring) == 0U);

    for (int32_t i = 0; i < 5; i++) in[i] = frame(i + 10);
    CHECK(audiosrc_ring_write(&ring, in, 5U) == 4U);
    CHECK(audiosrc_ring_read(&ring, out, 5U) == 4U);
    CHECK(out[0].right == -10 && out[3].right == -13);

    CHECK(audiosrc_ring_init(NULL, storage, 4U) == -1);
    CHECK(audiosrc_ring_init(&ring, NULL, 4U) == -1);
    CHECK(audiosrc_ring_init(&ring, storage, 1U) == -1);
    CHECK(audiosrc_ring_init(&ring, storage, SIZE_MAX / 2U + 1U) == -1);
    CHECK(audiosrc_ring_write(NULL, in, 1U) == 0U);
    CHECK(audiosrc_ring_read(&ring, NULL, 1U) == 0U);
    CHECK(audiosrc_ring_init(&ring, storage, 4U) == 0);
    atomic_store(&ring.head, 7U); atomic_store(&ring.tail, 7U);
    CHECK(audiosrc_ring_write(&ring, in, 3U) == 3U);
    CHECK(audiosrc_ring_count(&ring) == 3U);
    CHECK(audiosrc_ring_read(&ring, out, 3U) == 3U);
    CHECK(out[0].left == in[0].left && out[2].left == in[2].left);
    CHECK(audiosrc_ring_count(&ring) == 0U);

    {
        audiosrc_frame_t threaded_storage[64];
        audiosrc_ring_t threaded;
        threaded_ring_t ctx = {&threaded, ATOMIC_VAR_INIT(false)};
        pthread_t writer, reader;
        CHECK(audiosrc_ring_init(&threaded, threaded_storage, 64U) == 0);
        CHECK(pthread_create(&writer, NULL, threaded_writer, &ctx) == 0);
        CHECK(pthread_create(&reader, NULL, threaded_reader, &ctx) == 0);
        CHECK(pthread_join(writer, NULL) == 0);
        CHECK(pthread_join(reader, NULL) == 0);
        CHECK(!atomic_load(&ctx.failed));
        CHECK(audiosrc_ring_count(&threaded) == 0U);
    }
    puts("PASS: SPSC audio ring");
    return 0;
}

