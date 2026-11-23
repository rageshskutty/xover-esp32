#include "pcm5122_ctl.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

typedef struct {
    uint8_t regs[256];
    uint8_t wr_addr[8], wr_reg[8], wr_val[8];
    unsigned n_wr;
    uint8_t rd_reg[16];
    unsigned n_rd;
    int fail_write_at; /* index of the write or read call that fails, -1 for none */
    int fail_read_at;
} mock_t;

static int mock_write(void *ctx, uint8_t addr7, uint8_t reg, uint8_t val)
{
    mock_t *m = (mock_t *)ctx;
    const unsigned i = m->n_wr++;
    if (i < 8U) { m->wr_addr[i] = addr7; m->wr_reg[i] = reg; m->wr_val[i] = val; }
    return ((int)i == m->fail_write_at) ? 1 : 0;
}

static int mock_read(void *ctx, uint8_t addr7, uint8_t reg, uint8_t *val)
{
    mock_t *m = (mock_t *)ctx;
    const unsigned i = m->n_rd++;
    (void)addr7;
    if (i < 16U) m->rd_reg[i] = reg;
    if ((int)i == m->fail_read_at) return 1;
    *val = m->regs[reg];
    return 0;
}

static void reset(mock_t *m)
{
    memset(m, 0, sizeof *m);
    m->fail_write_at = -1;
    m->fail_read_at = -1;
}

static void running(mock_t *m)
{
    m->regs[0x5B] = 0x36U; /* DTFS = 3 (32-48 kHz), DTSR = 6 (SCK = 256 fS) */
    m->regs[0x5C] = 0x01U; /* DTBR[8] */
    m->regs[0x5D] = 0x00U; /* DTBR[7:0]: BCK ratio 256 */
    m->regs[0x5F] = 0x00U;
    m->regs[0x76] = 0x85U; /* BOTM = 1, PSTM = 5 (run) */
    m->regs[0x72] = 0x03U; /* MTST = 11: XSMT at or above 0.7 VDD */
}

int main(void)
{
    mock_t m;
    pcm5122_bus_t bus = {mock_write, mock_read, &m};
    pcm5122_status_t st;

    /* Init writes: page select, format, offset, in that order, to the given address. */
    reset(&m);
    CHECK(pcm5122_ctl_init_tdm(&bus, 0x4DU, 64U) == 0);
    CHECK(m.n_wr == 3U && m.n_rd == 0U);
    CHECK(m.wr_addr[0] == 0x4DU && m.wr_reg[0] == 0x00U && m.wr_val[0] == 0x00U);
    CHECK(m.wr_addr[1] == 0x4DU && m.wr_reg[1] == 0x28U && m.wr_val[1] == 0x13U);
    CHECK(m.wr_addr[2] == 0x4DU && m.wr_reg[2] == 0x29U && m.wr_val[2] == 0x40U);

    /* A failed write stops the sequence at once. */
    for (int k = 0; k < 3; k++) {
        reset(&m);
        m.fail_write_at = k;
        CHECK(pcm5122_ctl_init_tdm(&bus, 0x4CU, 0U) == -1);
        CHECK(m.n_wr == (unsigned)(k + 1));
    }

    /* Bad arguments fail before any bus access. */
    reset(&m);
    CHECK(pcm5122_ctl_init_tdm(&bus, 0x4CU, 256U) == -1);
    CHECK(pcm5122_ctl_init_tdm(&bus, 0x07U, 0U) == -1);
    CHECK(pcm5122_ctl_init_tdm(&bus, 0x78U, 0U) == -1);
    CHECK(pcm5122_ctl_init_tdm(NULL, 0x4CU, 0U) == -1);
    {
        pcm5122_bus_t nowrite = {NULL, mock_read, &m};
        CHECK(pcm5122_ctl_init_tdm(&nowrite, 0x4CU, 0U) == -1);
    }
    CHECK(m.n_wr == 0U && m.n_rd == 0U);

    /* Status decode of a healthy 48 kHz TDM stream; reads in a fixed order. */
    reset(&m);
    running(&m);
    memset(&st, 0xEE, sizeof st);
    CHECK(pcm5122_ctl_read_status(&bus, 0x4CU, &st) == 0);
    CHECK(m.n_rd == 6U && m.n_wr == 0U);
    CHECK(m.rd_reg[0] == 0x5BU && m.rd_reg[1] == 0x5CU && m.rd_reg[2] == 0x5DU);
    CHECK(m.rd_reg[3] == 0x5FU && m.rd_reg[4] == 0x76U && m.rd_reg[5] == 0x72U);
    CHECK(st.fs_class == 3U && st.sck_ratio_code == 6U && st.bck_per_frame == 256U);
    CHECK(st.clock_error == 0U && st.power_state == 5U && st.xsmt_level == 3U);
    CHECK(pcm5122_status_is_running_48k_tdm(&st) == 1);

    /* Field masks: reserved bits do not leak in. */
    reset(&m);
    running(&m);
    m.regs[0x5B] = 0xFFU;
    m.regs[0x5C] = 0xFFU;
    m.regs[0x5D] = 0xFFU;
    m.regs[0x76] = 0xFFU;
    m.regs[0x72] = 0xFFU;
    CHECK(pcm5122_ctl_read_status(&bus, 0x4CU, &st) == 0);
    CHECK(st.fs_class == 7U && st.sck_ratio_code == 15U && st.bck_per_frame == 511U);
    CHECK(st.power_state == 15U && st.xsmt_level == 3U);

    /* Clock error is CERF (bit 0) or CKMF (bit 2); LTSH (bit 4) and CSRF (bit 1) alone are not errors. */
    {
        const uint8_t val[5] = {0x01U, 0x04U, 0x14U, 0x10U, 0x02U};
        const uint8_t err[5] = {1U, 1U, 1U, 0U, 0U};
        for (size_t i = 0U; i < 5U; i++) {
            reset(&m);
            running(&m);
            m.regs[0x5F] = val[i];
            CHECK(pcm5122_ctl_read_status(&bus, 0x4CU, &st) == 0);
            CHECK(st.clock_error == err[i]);
        }
    }

    /* The running test needs every field right. */
    reset(&m);
    running(&m);
    CHECK(pcm5122_ctl_read_status(&bus, 0x4CU, &st) == 0);
    {
        pcm5122_status_t bad;
        bad = st; bad.fs_class = 4U;        CHECK(pcm5122_status_is_running_48k_tdm(&bad) == 0);
        bad = st; bad.sck_ratio_code = 5U;  CHECK(pcm5122_status_is_running_48k_tdm(&bad) == 0);
        bad = st; bad.bck_per_frame = 128U; CHECK(pcm5122_status_is_running_48k_tdm(&bad) == 0);
        bad = st; bad.clock_error = 1U;     CHECK(pcm5122_status_is_running_48k_tdm(&bad) == 0);
        bad = st; bad.power_state = 4U;     CHECK(pcm5122_status_is_running_48k_tdm(&bad) == 0);
        CHECK(pcm5122_status_is_running_48k_tdm(NULL) == 0);
    }

    /* A failed read returns -1 and leaves the output untouched. */
    for (int k = 0; k < 6; k++) {
        reset(&m);
        running(&m);
        m.fail_read_at = k;
        memset(&st, 0xEE, sizeof st);
        CHECK(pcm5122_ctl_read_status(&bus, 0x4CU, &st) == -1);
        CHECK(m.n_rd == (unsigned)(k + 1));
        {
            const unsigned char *b = (const unsigned char *)&st;
            for (size_t i = 0U; i < sizeof st; i++) CHECK(b[i] == 0xEEU);
        }
    }
    reset(&m);
    CHECK(pcm5122_ctl_read_status(&bus, 0x4CU, NULL) == -1);
    CHECK(pcm5122_ctl_read_status(NULL, 0x4CU, &st) == -1);
    CHECK(pcm5122_ctl_read_status(&bus, 0x07U, &st) == -1);
    {
        pcm5122_bus_t noread = {mock_write, NULL, &m};
        CHECK(pcm5122_ctl_read_status(&noread, 0x4CU, &st) == -1);
    }
    CHECK(m.n_rd == 0U);

    puts("PASS: PCM5122 control and status decode");
    return 0;
}
