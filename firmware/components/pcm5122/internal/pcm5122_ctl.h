#ifndef PCM5122_CTL_H
#define PCM5122_CTL_H

#include <stdint.h>

typedef int (*pcm5122_write_fn)(void *ctx, uint8_t addr7, uint8_t reg, uint8_t val);
typedef int (*pcm5122_read_fn)(void *ctx, uint8_t addr7, uint8_t reg, uint8_t *val);
typedef struct { pcm5122_write_fn write; pcm5122_read_fn read; void *ctx; } pcm5122_bus_t;
typedef struct {
    uint8_t fs_class; uint8_t sck_ratio_code; uint16_t bck_per_frame;
    uint8_t clock_error; uint8_t power_state; uint8_t xsmt_level;
} pcm5122_status_t;

int pcm5122_ctl_init_tdm(const pcm5122_bus_t *bus, uint8_t addr7, uint32_t offset_bck);
int pcm5122_ctl_read_status(const pcm5122_bus_t *bus, uint8_t addr7, pcm5122_status_t *out);
int pcm5122_status_is_running_48k_tdm(const pcm5122_status_t *st);

#endif /* PCM5122_CTL_H */
