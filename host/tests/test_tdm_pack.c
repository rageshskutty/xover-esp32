#include "tdm_pack.h"

#include <stdint.h>
#include <stdio.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

int main(void)
{
    int32_t c0[] = {10, 20}, c1[] = {11, 21}, c2[] = {12, 22};
    int32_t c3[] = {13, 23}, c4[] = {14, 24}, c5[] = {15, 25};
    const int32_t *const channels[] = {c0, c1, c2, c3, c4, c5};
    int32_t out[16];
    for (size_t i = 0U; i < 16U; i++) out[i] = -1;
    CHECK(tdm_pack_8slot(channels, 2U, out, 16U) == 0);
    for (size_t f = 0U; f < 2U; f++) {
        for (size_t ch = 0U; ch < 6U; ch++)
            CHECK(out[f * 8U + ch] == 10 * (int32_t)(f + 1U) + (int32_t)ch);
        CHECK(out[f * 8U + 6U] == 0 && out[f * 8U + 7U] == 0);
    }
    for (size_t i = 0U; i < 16U; i++) out[i] = 777;
    CHECK(tdm_pack_8slot(channels, 2U, out, 15U) == -1);
    for (size_t i = 0U; i < 16U; i++) CHECK(out[i] == 777);
    {
        const int32_t *bad[6] = {c0, c1, NULL, c3, c4, c5};
        CHECK(tdm_pack_8slot(bad, 2U, out, 16U) == -1);
        CHECK(tdm_pack_8slot(channels, SIZE_MAX / 8U + 1U, out, SIZE_MAX) == -1);
    }
    for (size_t i = 0U; i < 16U; i++) CHECK(out[i] == 777);
    CHECK(tdm_pack_8slot(NULL, 2U, out, 16U) == -1);
    CHECK(tdm_pack_8slot(channels, 2U, NULL, 16U) == -1);
    CHECK(tdm_pack_8slot(channels, 0U, NULL, 0U) == 0);
    puts("PASS: TDM slot packer");
    return 0;
}