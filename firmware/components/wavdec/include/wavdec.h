#ifndef WAVDEC_H
#define WAVDEC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WAVDEC_OK                = 0,   /* PCM bytes written to out buffer       */
    WAVDEC_HEADER_READY      = 1,   /* header parsed; fmt.* is valid         */
    WAVDEC_NEED_MORE_DATA    = 2,   /* feed more input                       */
    WAVDEC_END_OF_STREAM     = 3,   /* clean end (only on in_len==0)         */
    WAVDEC_ERR_INVALID_ARG   = -1,
    WAVDEC_ERR_INVALID_FMT   = -2,  /* not WAV/AIFF, or unsupported codec    */
    WAVDEC_ERR_CORRUPT       = -3,  /* truncated/mismatched chunk sizes      */
    WAVDEC_ERR_BUF_TOO_SMALL = -4,  /* out_cap==0 while data is pending      */
    WAVDEC_ERR_UNSUPPORTED   = -5,  /* valid container, unsupported PCM bits */
} wavdec_result_t;

typedef enum {
    WAVDEC_CONTAINER_UNKNOWN = 0,
    WAVDEC_CONTAINER_WAV,
    WAVDEC_CONTAINER_AIFF,
    WAVDEC_CONTAINER_AIFC,
} wavdec_container_t;

typedef enum {
    WAVDEC_FMT_U8     = 0,  /* WAV unsigned 8-bit; subtract 128 (or XOR 0x80)
                             to get signed S8                                */
    WAVDEC_FMT_S8     = 1,  /* AIFF signed 8-bit                              */
    WAVDEC_FMT_S16LE  = 2,  /* WAV, AIFC sowt                                 */
    WAVDEC_FMT_S16BE  = 3,  /* AIFF                                           */
    WAVDEC_FMT_S24LE  = 4,  /* packed 3-byte little-endian                    */
    WAVDEC_FMT_S24BE  = 5,  /* packed 3-byte big-endian                       */
    WAVDEC_FMT_S32LE  = 6,
    WAVDEC_FMT_S32BE  = 7,
    WAVDEC_FMT_F32LE  = 8,  /* IEEE 754 binary32 LE (WAV tag 3, AIFC FL32)    */
    WAVDEC_FMT_F32BE  = 9,  /* IEEE 754 binary32 BE (AIFC fl32)               */
} wavdec_sample_format_t;

typedef struct {
    wavdec_container_t     container;
    wavdec_sample_format_t sample_format;
    uint32_t             sample_rate;
    uint16_t             channels;
    uint16_t             bits_per_sample;
    uint32_t             data_bytes;       /* 0xFFFFFFFF => unknown length  */
    uint16_t             bytes_per_frame;  /* channels * ceil(bits/8)       */
} wavdec_format_t;

typedef struct wavdec {
    uint8_t  state;
    uint8_t  chunk_kind;
    uint8_t  scratch_have;
    uint8_t  scratch_need;
    uint8_t  header_emitted;
    uint8_t  is_aifc;
    uint8_t  data_pad;
    int8_t   err_code;        /* sticky: once set, every call returns it     */
    uint8_t  streaming_data;  /* 1 if WAV data chunk declares unknown size   */

    uint32_t chunk_size;
    uint32_t data_remaining;
    uint32_t skip_bytes;

    wavdec_format_t fmt;
    uint8_t  scratch[64];
} wavdec_t;

void wavdec_init(wavdec_t *d);

/*
 * Streaming decode. Feed any number of input bytes, receive any number of
 * decoded PCM bytes. Output is a contiguous byte stream in the byte order
 * declared by fmt.sample_format — it is NOT aligned to frame boundaries
 * across calls. To reconstruct frames, concatenate output across calls.
 *
 *   in_len == 0  is the EOF signal. It returns:
 *                  WAVDEC_END_OF_STREAM    if the stream ended cleanly
 *                                        (including streaming WAV with a
 *                                        0xFFFFFFFF data size),
 *                  WAVDEC_ERR_CORRUPT      if truncated mid-header/mid-data.
 *
 * Errors are sticky, with one exception: WAVDEC_ERR_BUF_TOO_SMALL is transient
 * and does not poison the decoder — pass a real buffer next call and the
 * decoder resumes from where it left off.
 *
 * A return of WAVDEC_OK or WAVDEC_HEADER_READY always consumes input
 * (bytes_consumed > 0) unless the output buffer is full. Callers must not
 * treat bytes_consumed == 0 as an infinite-loop hazard without also
 * checking for WAVDEC_NEED_MORE_DATA / WAVDEC_ERR_BUF_TOO_SMALL.
 */
wavdec_result_t wavdec_decode(wavdec_t *d,
                                  const uint8_t *in, size_t in_len,
                                  uint8_t *out, size_t out_cap,
                                  size_t *consumed,
                                  size_t *written);

#ifdef __cplusplus
}
#endif
#endif