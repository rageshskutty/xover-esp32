#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "hifi_decoder.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

/* ---- fixtures ---------------------------------------------------------- */

static const uint8_t wav_basic[] = {
    'R','I','F','F', 44,0,0,0, 'W','A','V','E',
    'f','m','t',' ', 16,0,0,0,
    1,0, 1,0, 0x40,0x1F,0,0, 0x80,0x3E,0,0, 2,0, 16,0,
    'd','a','t','a', 8,0,0,0,
    1,0, 2,0, 3,0, 4,0
};

static const uint8_t wav_list[] = {
    'R','I','F','F', 62,0,0,0, 'W','A','V','E',
    'f','m','t',' ', 16,0,0,0,
    1,0, 1,0, 0x40,0x1F,0,0, 0x80,0x3E,0,0, 2,0, 16,0,
    'L','I','S','T', 9,0,0,0,
    'I','N','F','O','I','C','M','T','A', 0,
    'd','a','t','a', 8,0,0,0,
    1,0, 2,0, 3,0, 4,0
};

static const uint8_t aiff_basic[] = {
    'F','O','R','M', 0,0,0,54, 'A','I','F','F',
    'C','O','M','M', 0,0,0,18,
    0,1, 0,0,0,4, 0,16, 0x40,0x0B,0xFA,0,0,0,0,0,0,0,
    'S','S','N','D', 0,0,0,16,
    0,0,0,0, 0,0,0,0,
    0,1, 0,2, 0,3, 0,4
};

static const uint8_t aifc_fl32[] = {
    'F','O','R','M', 0,0,0,60, 'A','I','F','C',
    'C','O','M','M', 0,0,0,24,
    0,1, 0,0,0,2, 0,32, 0x40,0x0B,0xFA,0,0,0,0,0,0,0,
    'f','l','3','2', 0x00, 0x00,
    'S','S','N','D', 0,0,0,16,
    0,0,0,0, 0,0,0,0,
    0x3F,0x80,0,0, 0x40,0,0,0
};

static const uint8_t aiff_ssnd_first[] = {
    'F','O','R','M', 0,0,0,32, 'A','I','F','F',
    'S','S','N','D', 0,0,0,16,
    0,0,0,0, 0,0,0,0,
    0,1, 0,2, 0,3, 0,4
};

static const uint8_t wav_trunc_data[] = {
    'R','I','F','F', 40,0,0,0, 'W','A','V','E',
    'f','m','t',' ', 16,0,0,0,
    1,0, 1,0, 0x40,0x1F,0,0, 0x80,0x3E,0,0, 2,0, 16,0,
    'd','a','t','a', 8,0,0,0,
    1,0, 2,0
};

/* Streaming WAV: data chunk declares 0xFFFFFFFF size. */
static const uint8_t wav_streaming[] = {
    'R','I','F','F', 0xFF,0xFF,0xFF,0xFF, 'W','A','V','E',
    'f','m','t',' ', 16,0,0,0,
    1,0, 1,0, 0x40,0x1F,0,0, 0x80,0x3E,0,0, 2,0, 16,0,
    'd','a','t','a', 0xFF,0xFF,0xFF,0xFF,
    1,0, 2,0, 3,0, 4,0
};

/* ---- helpers ----------------------------------------------------------- */

typedef struct { uint8_t *buf; size_t cap, len; } sink_t;

static hifi_result_t feed(hifi_decoder_t *d,
                          const uint8_t *in, size_t n,
                          size_t chunk, sink_t *sink)
{
    size_t off = 0;
    hifi_result_t last = HIFI_NEED_MORE_DATA;
    while (off < n) {
        size_t piece = n - off;
        if (chunk && piece > chunk) piece = chunk;
        size_t used = 0, wrote = 0;
        hifi_result_t r = hifi_decoder_decode(d, in + off, piece,
                                              sink->buf + sink->len,
                                              sink->cap - sink->len,
                                              &used, &wrote);
        off += used;
        sink->len += wrote;
        last = r;
        if (r < 0) return r;
        if (r == HIFI_END_OF_STREAM) return r;
        if (used == 0 && wrote == 0) break;
    }
    return last;
}

static hifi_result_t eof(hifi_decoder_t *d, sink_t *sink)
{
    size_t used = 0, wrote = 0;
    hifi_result_t r = hifi_decoder_decode(d, NULL, 0,
                                          sink->buf + sink->len,
                                          sink->cap - sink->len,
                                          &used, &wrote);
    sink->len += wrote;
    return r;
}

/* ---- tests ------------------------------------------------------------- */

static void test_wav_basic(void)
{
    hifi_decoder_t d; hifi_decoder_init(&d);
    uint8_t out[64]; sink_t s = { out, sizeof out, 0 };
    hifi_result_t r = feed(&d, wav_basic, sizeof wav_basic, 0, &s);
    CHECK(r == HIFI_OK);
    CHECK(eof(&d, &s) == HIFI_END_OF_STREAM);
    CHECK(d.fmt.sample_format   == HIFI_FMT_S16LE);
    CHECK(d.fmt.sample_rate     == 8000);
    CHECK(d.fmt.channels        == 1);
    CHECK(d.fmt.bits_per_sample == 16);
    CHECK(d.fmt.data_bytes      == 8);
    CHECK(s.len == 8);
    static const uint8_t expect[] = {1,0, 2,0, 3,0, 4,0};
    CHECK(!memcmp(s.buf, expect, 8));
    printf("  ok: wav_basic\n");
}

static void test_wav_odd_list(void)
{
    hifi_decoder_t d; hifi_decoder_init(&d);
    uint8_t out[64]; sink_t s = { out, sizeof out, 0 };
    feed(&d, wav_list, sizeof wav_list, 0, &s);
    CHECK(eof(&d, &s) == HIFI_END_OF_STREAM);
    CHECK(s.len == 8);
    static const uint8_t expect[] = {1,0, 2,0, 3,0, 4,0};
    CHECK(!memcmp(s.buf, expect, 8));
    printf("  ok: wav_odd_list\n");
}

static void test_aiff_basic(void)
{
    hifi_decoder_t d; hifi_decoder_init(&d);
    uint8_t out[64]; sink_t s = { out, sizeof out, 0 };
    feed(&d, aiff_basic, sizeof aiff_basic, 0, &s);
    CHECK(eof(&d, &s) == HIFI_END_OF_STREAM);
    CHECK(d.fmt.sample_format == HIFI_FMT_S16BE);
    CHECK(d.fmt.sample_rate   == 8000);
    CHECK(s.len == 8);
    static const uint8_t expect[] = {0,1, 0,2, 0,3, 0,4};
    CHECK(!memcmp(s.buf, expect, 8));
    printf("  ok: aiff_basic\n");
}

/* Bug 3: fl32 must be big-endian. */
static void test_aifc_fl32(void)
{
    hifi_decoder_t d; hifi_decoder_init(&d);
    uint8_t out[64]; sink_t s = { out, sizeof out, 0 };
    feed(&d, aifc_fl32, sizeof aifc_fl32, 0, &s);
    CHECK(eof(&d, &s) == HIFI_END_OF_STREAM);
    CHECK(d.fmt.sample_format   == HIFI_FMT_F32BE);
    CHECK(d.fmt.bits_per_sample == 32);
    CHECK(s.len == 8);
    static const uint8_t expect[] = {0x3F,0x80,0,0, 0x40,0,0,0};
    CHECK(!memcmp(s.buf, expect, 8));
    printf("  ok: aifc_fl32 -> F32BE\n");
}

/* Bug 1: no frame alignment required; feed in 1/3/7-byte pieces. */
static void test_chunked_feeds(void)
{
    static const size_t chunks[] = {1, 3, 7};
    for (size_t k = 0; k < 3; ++k) {
        hifi_decoder_t d; hifi_decoder_init(&d);
        uint8_t out[64]; sink_t s = { out, sizeof out, 0 };
        feed(&d, wav_basic, sizeof wav_basic, chunks[k], &s);
        CHECK(eof(&d, &s) == HIFI_END_OF_STREAM);
        CHECK(s.len == 8);
        static const uint8_t expect[] = {1,0, 2,0, 3,0, 4,0};
        CHECK(!memcmp(s.buf, expect, 8));
    }
    printf("  ok: chunked 1/3/7-byte feeds\n");
}

/* Bug 4: errors are sticky. */
static void test_sticky_error(void)
{
    hifi_decoder_t d; hifi_decoder_init(&d);
    uint8_t out[64]; sink_t s = { out, sizeof out, 0 };
    feed(&d, aiff_ssnd_first, sizeof aiff_ssnd_first, 0, &s);
    size_t used = 0, wrote = 0;
    hifi_result_t r1 = hifi_decoder_decode(&d, wav_basic, sizeof wav_basic,
                                           out, sizeof out, &used, &wrote);
    CHECK(r1 == HIFI_ERR_CORRUPT);
    CHECK(used == 0 && wrote == 0);
    hifi_result_t r2 = hifi_decoder_decode(&d, NULL, 0, out, sizeof out,
                                           &used, &wrote);
    CHECK(r2 == HIFI_ERR_CORRUPT);
    printf("  ok: sticky errors\n");
}

/* Bug 5: SSND before COMM is rejected. */
static void test_ssnd_before_comm(void)
{
    hifi_decoder_t d; hifi_decoder_init(&d);
    uint8_t out[64]; sink_t s = { out, sizeof out, 0 };
    hifi_result_t r = feed(&d, aiff_ssnd_first, sizeof aiff_ssnd_first, 0, &s);
    CHECK(r == HIFI_ERR_CORRUPT);
    printf("  ok: ssnd_before_comm -> ERR_CORRUPT\n");
}

/* Bug 6: truncation reported at EOF. */
static void test_truncated_data(void)
{
    hifi_decoder_t d; hifi_decoder_init(&d);
    uint8_t out[64]; sink_t s = { out, sizeof out, 0 };
    feed(&d, wav_trunc_data, sizeof wav_trunc_data, 0, &s);
    CHECK(eof(&d, &s) == HIFI_ERR_CORRUPT);
    printf("  ok: truncated_data -> ERR_CORRUPT\n");
}

static void test_truncated_header(void)
{
    hifi_decoder_t d; hifi_decoder_init(&d);
    uint8_t out[64]; sink_t s = { out, sizeof out, 0 };
    feed(&d, wav_trunc_data, 8, 0, &s);
    CHECK(eof(&d, &s) == HIFI_ERR_CORRUPT);
    printf("  ok: truncated_header -> ERR_CORRUPT\n");
}

/* Bug 2: feed example must not drop header bytes across calls. */
static void test_split_header(void)
{
    hifi_decoder_t d; hifi_decoder_init(&d);
    uint8_t out[64]; sink_t s = { out, sizeof out, 0 };
    feed(&d, wav_basic + 0,  4, 4, &s);
    feed(&d, wav_basic + 4,  4, 4, &s);
    feed(&d, wav_basic + 8,  4, 4, &s);
    feed(&d, wav_basic + 12, sizeof wav_basic - 12, 0, &s);
    CHECK(eof(&d, &s) == HIFI_END_OF_STREAM);
    CHECK(d.fmt.container == HIFI_CONTAINER_WAV);
    CHECK(s.len == 8);
    printf("  ok: split header not dropped\n");
}

/* Nit 1: BUF_TOO_SMALL must not poison the decoder. */
static void test_no_output_space_not_sticky(void)
{
    hifi_decoder_t d; hifi_decoder_init(&d);
    uint8_t out[64]; sink_t s = { out, sizeof out, 0 };
    feed(&d, wav_basic, 44, 0, &s);          /* header only */
    size_t used = 0, wrote = 0;
    hifi_result_t r = hifi_decoder_decode(&d, wav_basic + 44, 8,
                                          NULL, 0, &used, &wrote);
    CHECK(r == HIFI_ERR_BUF_TOO_SMALL);
    CHECK(d.err_code == 0);                  /* not sticky */
    r = hifi_decoder_decode(&d, wav_basic + 44, 8, out, sizeof out,
                            &used, &wrote);
    CHECK(r == HIFI_OK);
    CHECK(wrote == 8);
    printf("  ok: BUF_TOO_SMALL is non-sticky\n");
}

/* Nit 2: streaming WAV (data size 0xFFFFFFFF) ends cleanly. */
static void test_streaming_wav(void)
{
    hifi_decoder_t d; hifi_decoder_init(&d);
    uint8_t out[64]; sink_t s = { out, sizeof out, 0 };
    feed(&d, wav_streaming, sizeof wav_streaming, 0, &s);
    CHECK(eof(&d, &s) == HIFI_END_OF_STREAM);   /* not ERR_CORRUPT */
    CHECK(d.fmt.data_bytes == 0xFFFFFFFFu);
    CHECK(s.len == 8);
    printf("  ok: streaming wav (data size 0xFFFFFFFF)\n");
}

int main(void)
{
    printf("hifi_decoder tests\n");
    test_wav_basic();
    test_wav_odd_list();
    test_aiff_basic();
    test_aifc_fl32();
    test_chunked_feeds();
    test_sticky_error();
    test_ssnd_before_comm();
    test_truncated_data();
    test_truncated_header();
    test_split_header();
    test_no_output_space_not_sticky();
    test_streaming_wav();
    printf("all passed\n");
    return 0;
}