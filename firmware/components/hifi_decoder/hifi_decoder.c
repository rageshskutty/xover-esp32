#include "hifi_decoder.h"
#include <string.h>

enum { ST_INIT = 0, ST_CHUNK_HDR, ST_CHUNK_DATA, ST_SKIP, ST_DATA, ST_DONE };
enum { CK_NONE = 0, CK_WAV_FMT, CK_AIFF_COMM, CK_AIFF_SSND_HDR };

static inline uint16_t rd_le16(const uint8_t *p)
{ return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
static inline uint16_t rd_be16(const uint8_t *p)
{ return ((uint16_t)p[0] << 8) | (uint16_t)p[1]; }
static inline uint32_t rd_le32(const uint8_t *p)
{ return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static inline uint32_t rd_be32(const uint8_t *p)
{ return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | (uint32_t)p[3]; }

static uint32_t parse_aiff_rate(const uint8_t *p)
{
    uint16_t exp  = rd_be16(p);
    uint64_t mant = ((uint64_t)rd_be32(p + 2) << 32) | rd_be32(p + 6);
    if (exp == 0 && mant == 0) return 0;
    int e = (int)exp - 16383 - 63;
    if (e >= 0)  { return e > 31  ? 0 : (uint32_t)(mant <<  e); }
    else         { return -e > 63 ? 0 : (uint32_t)(mant >> -e); }
}

static size_t fill_scratch(hifi_decoder_t *d, const uint8_t *in, size_t avail)
{
    size_t need = (size_t)d->scratch_need - d->scratch_have;
    size_t take = avail < need ? avail : need;
    if (take) memcpy(d->scratch + d->scratch_have, in, take);
    d->scratch_have = (uint8_t)(d->scratch_have + take);
    return take;
}

static void finish_data_chunk(hifi_decoder_t *d)
{
    d->scratch_have = 0;
    d->scratch_need = 8;
    d->chunk_kind   = CK_NONE;
    if (d->data_pad) {
        d->skip_bytes = 1;
        d->state      = ST_SKIP;
    } else {
        d->state      = ST_CHUNK_HDR;
    }
}

void hifi_decoder_init(hifi_decoder_t *d)
{
    if (!d) return;
    memset(d, 0, sizeof(*d));
    d->state        = ST_INIT;
    d->scratch_need = 12;
}

hifi_result_t hifi_decoder_decode(hifi_decoder_t *d,
                                  const uint8_t *in, size_t in_len,
                                  uint8_t       *out, size_t out_cap,
                                  size_t *consumed, size_t *written)
{
    size_t c = 0, w = 0;
    hifi_result_t ret = HIFI_NEED_MORE_DATA;

    if (!d || !consumed || !written) return HIFI_ERR_INVALID_ARG;
    if (in_len  && !in)             return HIFI_ERR_INVALID_ARG;
    if (out_cap && !out)            return HIFI_ERR_INVALID_ARG;

    *consumed = 0;
    *written  = 0;

    if (d->err_code)
        return (hifi_result_t)d->err_code;

    for (;;) {
        switch (d->state) {

        case ST_INIT: {
            c += fill_scratch(d, in + c, in_len - c);
            if (d->scratch_have < d->scratch_need) goto done;
            const uint8_t *s = d->scratch;
            if (!memcmp(s, "RIFF", 4) && !memcmp(s + 8, "WAVE", 4)) {
                d->fmt.container = HIFI_CONTAINER_WAV;
            } else if (!memcmp(s, "FORM", 4) &&
                      (!memcmp(s + 8, "AIFF", 4) || !memcmp(s + 8, "AIFC", 4))) {
                d->is_aifc = !memcmp(s + 8, "AIFC", 4);
                d->fmt.container = d->is_aifc ? HIFI_CONTAINER_AIFC
                                              : HIFI_CONTAINER_AIFF;
            } else {
                ret = HIFI_ERR_INVALID_FMT;
                goto done;
            }
            d->scratch_have = 0;
            d->scratch_need = 8;
            d->state        = ST_CHUNK_HDR;
            break;
        }

        case ST_CHUNK_HDR: {
            c += fill_scratch(d, in + c, in_len - c);
            if (d->scratch_have < d->scratch_need) goto done;
            const uint8_t *s = d->scratch;
            int is_wav = (d->fmt.container == HIFI_CONTAINER_WAV);
            d->chunk_size   = is_wav ? rd_le32(s + 4) : rd_be32(s + 4);
            d->scratch_have = 0;

            if (is_wav) {
                if (!memcmp(s, "fmt ", 4)) {
                    if (d->chunk_size < 16) { ret = HIFI_ERR_CORRUPT; goto done; }
                    d->chunk_kind   = CK_WAV_FMT;
                    d->scratch_need = d->chunk_size >= 40 ? 40
                                                          : (uint8_t)d->chunk_size;
                    d->state = ST_CHUNK_DATA;
                } else if (!memcmp(s, "data", 4)) {
                    if (d->fmt.bytes_per_frame == 0) {
                        ret = HIFI_ERR_CORRUPT; goto done;
                    }
                    d->fmt.data_bytes  = d->chunk_size;
                    d->data_remaining  = d->chunk_size;
                    d->data_pad        = (uint8_t)(d->chunk_size & 1);
                    d->streaming_data  = (uint8_t)(d->chunk_size == 0xFFFFFFFFu);
                    d->state           = ST_DATA;
                } else {
                    uint32_t skip = d->chunk_size + (d->chunk_size & 1);
                    if (skip) {
                        d->skip_bytes   = skip;
                        d->chunk_kind   = CK_NONE;
                        d->scratch_need = 8;
                        d->state        = ST_SKIP;
                    }
                }
            } else {
                if (!memcmp(s, "COMM", 4)) {
                    if (d->chunk_size < 18) { ret = HIFI_ERR_CORRUPT; goto done; }
                    d->chunk_kind   = CK_AIFF_COMM;
                    d->scratch_need = d->chunk_size >= 22 ? 22 : 18;
                    d->state        = ST_CHUNK_DATA;
                } else if (!memcmp(s, "SSND", 4)) {
                    if (d->chunk_size < 8 || d->fmt.bytes_per_frame == 0) {
                        ret = HIFI_ERR_CORRUPT; goto done;
                    }
                    d->chunk_kind   = CK_AIFF_SSND_HDR;
                    d->scratch_need = 8;
                    d->state        = ST_CHUNK_DATA;
                } else {
                    uint32_t skip = d->chunk_size + (d->chunk_size & 1);
                    if (skip) {
                        d->skip_bytes   = skip;
                        d->chunk_kind   = CK_NONE;
                        d->scratch_need = 8;
                        d->state        = ST_SKIP;
                    }
                }
            }
            break;
        }

        case ST_CHUNK_DATA: {
            c += fill_scratch(d, in + c, in_len - c);
            if (d->scratch_have < d->scratch_need) goto done;
            const uint8_t *s = d->scratch;

            if (d->chunk_kind == CK_WAV_FMT) {
                uint16_t tag   = rd_le16(s + 0);
                uint16_t ch    = rd_le16(s + 2);
                uint32_t rate  = rd_le32(s + 4);
                uint16_t align = rd_le16(s + 12);
                uint16_t bits  = rd_le16(s + 14);

                if (tag == 0xFFFE && d->scratch_need >= 40)
                    tag = rd_le16(s + 24);

                if (!ch || !bits) { ret = HIFI_ERR_CORRUPT; goto done; }

                hifi_sample_format_t sf;
                if (tag == 1) {
                    switch (bits) {
                    case  8: sf = HIFI_FMT_U8;    break;
                    case 16: sf = HIFI_FMT_S16LE; break;
                    case 24: sf = HIFI_FMT_S24LE; break;
                    case 32: sf = HIFI_FMT_S32LE; break;
                    default: ret = HIFI_ERR_UNSUPPORTED; goto done;
                    }
                } else if (tag == 3 && bits == 32) {
                    sf = HIFI_FMT_F32LE;
                } else {
                    ret = HIFI_ERR_UNSUPPORTED; goto done;
                }

                d->fmt.sample_format   = sf;
                d->fmt.channels        = ch;
                d->fmt.sample_rate     = rate;
                d->fmt.bits_per_sample = bits;
                d->fmt.bytes_per_frame = (uint16_t)(align ? align
                    : (uint16_t)(ch * ((bits + 7) / 8)));

                uint32_t extra = d->chunk_size - d->scratch_need
                               + (d->chunk_size & 1);
                d->scratch_have = 0;
                d->scratch_need = 8;
                if (extra) {
                    d->skip_bytes = extra;
                    d->chunk_kind = CK_NONE;
                    d->state      = ST_SKIP;
                } else {
                    d->state      = ST_CHUNK_HDR;
                }

            } else if (d->chunk_kind == CK_AIFF_COMM) {
                uint16_t ch   = rd_be16(s + 0);
                uint16_t bits = rd_be16(s + 6);
                uint32_t rate = parse_aiff_rate(s + 8);

                if (!ch || !bits || !rate) { ret = HIFI_ERR_CORRUPT; goto done; }

                int le = 0, flt = 0;
                if (d->is_aifc && d->scratch_need >= 22) {
                    const uint8_t *ct = s + 18;
                    if      (!memcmp(ct, "sowt", 4)) { le = 1; }
                    else if (!memcmp(ct, "fl32", 4)) { flt = 1; }              /* BE float */
                    else if (!memcmp(ct, "FL32", 4)) { le  = 1; flt = 1; }     /* LE float */
                    else if (!memcmp(ct, "fl64", 4) || !memcmp(ct, "FL64", 4)) {
                        ret = HIFI_ERR_UNSUPPORTED; goto done;
                    }
                    else if (memcmp(ct, "NONE", 4) && memcmp(ct, "twos", 4)) {
                        ret = HIFI_ERR_UNSUPPORTED; goto done;
                    }
                }

                hifi_sample_format_t sf;
                if (flt) {
                    if (bits != 32) { ret = HIFI_ERR_UNSUPPORTED; goto done; }
                    sf = le ? HIFI_FMT_F32LE : HIFI_FMT_F32BE;
                } else if (bits == 8) {
                    sf = HIFI_FMT_S8;
                } else {
                    hifi_sample_format_t le_f, be_f;
                    switch (bits) {
                    case 16: le_f = HIFI_FMT_S16LE; be_f = HIFI_FMT_S16BE; break;
                    case 24: le_f = HIFI_FMT_S24LE; be_f = HIFI_FMT_S24BE; break;
                    case 32: le_f = HIFI_FMT_S32LE; be_f = HIFI_FMT_S32BE; break;
                    default: ret = HIFI_ERR_UNSUPPORTED; goto done;
                    }
                    sf = le ? le_f : be_f;
                }

                d->fmt.sample_format   = sf;
                d->fmt.channels        = ch;
                d->fmt.sample_rate     = rate;
                d->fmt.bits_per_sample = bits;
                d->fmt.bytes_per_frame = (uint16_t)(ch * ((bits + 7) / 8));

                uint32_t extra = d->chunk_size - d->scratch_need
                               + (d->chunk_size & 1);
                d->scratch_have = 0;
                d->scratch_need = 8;
                if (extra) {
                    d->skip_bytes = extra;
                    d->chunk_kind = CK_NONE;
                    d->state      = ST_SKIP;
                } else {
                    d->state      = ST_CHUNK_HDR;
                }

            } else if (d->chunk_kind == CK_AIFF_SSND_HDR) {
                uint32_t offset = rd_be32(s + 0);
                uint32_t total  = d->chunk_size;
                if (offset > total - 8) { ret = HIFI_ERR_CORRUPT; goto done; }
                uint32_t pcm_bytes = total - 8 - offset;

                d->fmt.data_bytes = pcm_bytes;
                d->data_remaining = pcm_bytes;
                d->data_pad       = (uint8_t)(total & 1);
                d->streaming_data = 0;

                d->scratch_have = 0;
                if (offset) {
                    d->skip_bytes   = offset;
                    d->scratch_need = 8;
                    d->chunk_kind   = CK_AIFF_SSND_HDR;
                    d->state        = ST_SKIP;
                } else {
                    d->chunk_kind   = CK_NONE;
                    d->state        = ST_DATA;
                }
            }
            break;
        }

        case ST_SKIP: {
            size_t avail = in_len - c;
            size_t n     = avail < d->skip_bytes ? avail : d->skip_bytes;
            c            += n;
            d->skip_bytes -= (uint32_t)n;
            if (d->skip_bytes) goto done;

            d->scratch_have = 0;
            d->scratch_need = 8;
            if (d->chunk_kind == CK_AIFF_SSND_HDR) {
                d->chunk_kind = CK_NONE;
                d->state      = ST_DATA;
            } else {
                d->state      = ST_CHUNK_HDR;
            }
            break;
        }

        case ST_DATA: {
            if (!d->header_emitted) {
                d->header_emitted = 1;
                ret = HIFI_HEADER_READY;
                goto done;
            }
            if (d->data_remaining == 0) {
                finish_data_chunk(d);
                break;
            }
            if (out_cap == w) {
                if (c == 0 && w == 0) {
                    ret = HIFI_ERR_BUF_TOO_SMALL;
                    goto done;
                }
                goto done;
            }
            size_t avail = in_len - c;
            if (avail == 0) goto done;
            size_t take = avail;
            if (take > out_cap - w)       take = out_cap - w;
            if (take > d->data_remaining) take = d->data_remaining;
            memcpy(out + w, in + c, take);
            c += take; w += take;
            d->data_remaining -= (uint32_t)take;
            ret = HIFI_OK;
            break;
        }

        case ST_DONE:
            ret = HIFI_END_OF_STREAM;
            goto done;

        default:
            ret = HIFI_ERR_CORRUPT;
            goto done;
        }
    }

done:
    if (ret < 0 && ret != HIFI_ERR_BUF_TOO_SMALL) {
        d->err_code = (int8_t)ret;
    } else if (in_len == 0 && c == 0 && w == 0 && ret == HIFI_NEED_MORE_DATA) {
        hifi_result_t eof = HIFI_END_OF_STREAM;
        if      (d->state == ST_INIT)                                            eof = HIFI_ERR_CORRUPT;
        else if (d->state == ST_CHUNK_HDR  && d->scratch_have > 0)               eof = HIFI_ERR_CORRUPT;
        else if (d->state == ST_CHUNK_DATA && d->scratch_have < d->scratch_need) eof = HIFI_ERR_CORRUPT;
        else if (d->state == ST_SKIP       && d->skip_bytes > 0)                 eof = HIFI_ERR_CORRUPT;
        else if (d->state == ST_DATA       && d->data_remaining > 0 &&
                                              !d->streaming_data)                eof = HIFI_ERR_CORRUPT;
        else if (!d->header_emitted)                                             eof = HIFI_ERR_CORRUPT;
        ret = eof;
        if (ret < 0) d->err_code = (int8_t)ret;
    }
    *consumed = c;
    *written  = w;
    return ret;
}