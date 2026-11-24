#include "pcm5122_ctl.h"
#include "pcm5122_regs.h"

#include <stddef.h>

#define PCM5122_I2C_ADDR_MIN 0x08U
#define PCM5122_I2C_ADDR_MAX 0x77U
#define PCM5122_STATUS_REG_COUNT 6U
/* Page 0 / Register 95 (0x5F): CKMF is bit 2 and CERF is bit 0.
 * TI SLAS763C, Table 108, p. 95 (CERF description continues on p. 96). */
#define PCM5122_CLK_ERR_FLAGS 0x05U

static const uint8_t s_status_regs[PCM5122_STATUS_REG_COUNT] = {
    0x5BU, 0x5CU, 0x5DU, 0x5FU, 0x76U, 0x72U
};

int pcm5122_ctl_init_tdm(const pcm5122_bus_t *bus, uint8_t addr7, uint32_t offset_bck)
{
    pcm5122_reg_write_t writes[PCM5122_TDM_WRITE_COUNT];
    if (bus == NULL || bus->write == NULL ||
        addr7 < PCM5122_I2C_ADDR_MIN || addr7 > PCM5122_I2C_ADDR_MAX ||
        pcm5122_tdm_writes(offset_bck, writes) != 0) {
        return -1;
    }
    for (size_t i = 0U; i < PCM5122_TDM_WRITE_COUNT; i++) {
        if (bus->write(bus->ctx, addr7, writes[i].reg, writes[i].val) != 0) {
            return -1;
        }
    }
    return 0;
}

int pcm5122_ctl_read_status(const pcm5122_bus_t *bus, uint8_t addr7, pcm5122_status_t *out)
{
    uint8_t raw[PCM5122_STATUS_REG_COUNT];
    if (bus == NULL || bus->read == NULL || out == NULL ||
        addr7 < PCM5122_I2C_ADDR_MIN || addr7 > PCM5122_I2C_ADDR_MAX) {
        return -1;
    }
    for (size_t i = 0U; i < PCM5122_STATUS_REG_COUNT; i++) {
        if (bus->read(bus->ctx, addr7, s_status_regs[i], &raw[i]) != 0) {
            return -1;
        }
    }
    out->fs_class = (uint8_t)((raw[0] >> 4) & 7U);
    out->sck_ratio_code = (uint8_t)(raw[0] & 0x0FU);
    out->bck_per_frame = (uint16_t)(((uint16_t)(raw[1] & 1U) << 8) | (uint16_t)raw[2]);
    out->clock_error = (uint8_t)((raw[3] & PCM5122_CLK_ERR_FLAGS) != 0U ? 1U : 0U);
    out->power_state = (uint8_t)(raw[4] & 0x0FU);
    out->xsmt_level = (uint8_t)(raw[5] & 3U);
    return 0;
}

int pcm5122_status_is_running_48k_tdm(const pcm5122_status_t *st)
{
    if (st == NULL) {
        return 0;
    }
    return (st->fs_class == 3U && st->sck_ratio_code == 6U &&
            st->bck_per_frame == 256U && st->clock_error == 0U &&
            st->power_state == 5U) ? 1 : 0;
}
