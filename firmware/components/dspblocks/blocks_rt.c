#include "dspblocks.h"

void dsp_gain_init(dsp_gain_t *g, float initial_gain)
{
    if (!g) return;
    g->current = initial_gain; g->target = initial_gain; g->step = 0.0f; g->remaining = 0U;
}

void dsp_gain_set_target(dsp_gain_t *g, float target, uint32_t ramp_frames)
{
    if (!g) return;
    g->target = target; g->remaining = ramp_frames;
    if (ramp_frames == 0U) { g->current = target; g->step = 0.0f; return; }
    float diff = target - g->current;
    if (diff < 0.0f) diff = -diff;
    g->step = diff / (float)ramp_frames;
}

void dsp_gain_process(dsp_gain_t *g, const float *in, float *out, size_t n_frames)
{
    if (!g || !in || !out || n_frames == 0U) return;
    float cur = g->current; const float tgt = g->target, step = g->step;
    uint32_t rem = g->remaining;
    if (rem == 0U || cur == tgt) {
        g->current = tgt;
        for (size_t i = 0U; i < n_frames; ++i) out[i] = in[i] * tgt;
        return;
    }
    const bool up = cur < tgt;
    for (size_t i = 0U; i < n_frames; ++i) {
        if (rem > 0U) {
            --rem;
            if (rem == 0U) cur = tgt;
            else cur = up ? (cur + step) : (cur - step);
            if ((up && cur > tgt) || (!up && cur < tgt)) cur = tgt;
        }
        out[i] = in[i] * cur;
    }
    g->remaining = rem; g->current = cur;
}

void dsp_polarity_process(bool invert, const float *in, float *out, size_t n_frames)
{
    if (!in || !out || n_frames == 0U) return;
    if (invert) {
        for (size_t i = 0U; i < n_frames; ++i) out[i] = -in[i];
    } else if (in != out) {
        for (size_t i = 0U; i < n_frames; ++i) out[i] = in[i];
    }
}

dsp_err_t dsp_delay_init(dsp_delay_t *d, float *buf, size_t cap, size_t delay)
{
    if (!d || !buf || cap == 0U) return DSP_E_PARAM;
    if (delay > cap) return DSP_E_RANGE;
    for (size_t i = 0U; i < cap; ++i) buf[i] = 0.0f;
    d->buffer = buf; d->capacity = cap; d->delay_samples = delay; d->write_pos = 0U;
    return DSP_OK;
}

dsp_err_t dsp_delay_set_delay(dsp_delay_t *d, size_t delay_samples)
{
    if (!d) return DSP_E_PARAM;
    if (delay_samples > d->capacity) return DSP_E_RANGE;
    d->delay_samples = delay_samples;
    return DSP_OK;
}

void dsp_delay_process(dsp_delay_t *d, const float *in, float *out, size_t n_frames)
{
    if (!d || !in || !out || n_frames == 0U) return;
    const size_t delay = d->delay_samples, cap = d->capacity;
    float *const buf = d->buffer;
    if (delay == 0U || !buf || cap == 0U || delay > cap) {
        if (in != out) for (size_t i = 0U; i < n_frames; ++i) out[i] = in[i];
        return;
    }
    size_t wp = d->write_pos;
    for (size_t i = 0U; i < n_frames; ++i) {
        const float x = in[i];
        const size_t rp = (wp >= delay) ? (wp - delay) : (wp + cap - delay);
        const float y = buf[rp];
        buf[wp] = x;
        wp = (wp + 1U < cap) ? (wp + 1U) : 0U;
        out[i] = y;
    }
    d->write_pos = wp;
}

void dsp_mix_process(const float *const *inputs, const float *gains, size_t n_inputs,
                     float *out, size_t n_frames)
{
    if (!inputs || !gains || !out || n_inputs == 0U || n_frames == 0U) return;
    for (size_t i = 0U; i < n_frames; ++i) {
        float sum = 0.0f;
        for (size_t k = 0U; k < n_inputs; ++k) sum += inputs[k][i] * gains[k];
        out[i] = sum;
    }
}

bool dsp_check_nan_inf(const float *buf, size_t n_frames)
{
    if (!buf) return false;
    for (size_t i = 0U; i < n_frames; ++i) if (dsp_is_nan_or_inf(buf[i])) return true;
    return false;
}

void dsp_flush_denormals(float *buf, size_t n_frames)
{
    if (!buf) return;
    for (size_t i = 0U; i < n_frames; ++i) buf[i] = dsp_flush_denormal(buf[i]);
}

bool dsp_sanitize_block(float *buf, size_t n_frames)
{
    if (!buf || n_frames == 0U) return true;
    for (size_t i = 0U; i < n_frames; ++i) {
        const float x = buf[i];
        if (dsp_is_nan_or_inf(x)) {
            for (size_t j = 0U; j < n_frames; ++j) buf[j] = 0.0f;
            return false;
        }
        buf[i] = dsp_flush_denormal(x);
    }
    return true;
}
