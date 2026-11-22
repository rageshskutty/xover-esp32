#include "config_slot.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

int main(void)
{
    const uint8_t sample[] = "123456789";
    CHECK(config_crc32(sample, 9U) == UINT32_C(0xCBF43926));
    CHECK(config_crc32(NULL, 0U) == 0U);
    CHECK(config_crc32(NULL, 1U) == 0U);
    {
        const uint8_t payload[] = {1U, 2U, 3U};
        config_slot_header_t h = {UINT32_C(0x1234), 7U, 3U, 0U};
        h.crc32 = config_crc32(payload, sizeof(payload));
        CHECK(config_slot_valid(&h, payload, sizeof(payload), h.magic, h.version) == 1);
        CHECK(config_slot_valid(&h, payload, sizeof(payload), 0U, h.version) == 0);
        CHECK(config_slot_valid(&h, payload, sizeof(payload), h.magic, 8U) == 0);
        CHECK(config_slot_valid(&h, payload, 2U, h.magic, h.version) == 0);
        h.length = 0U;
        CHECK(config_slot_valid(&h, payload, sizeof(payload), h.magic, h.version) == 0);
        h.length = 3U; h.crc32 ^= 1U;
        CHECK(config_slot_valid(&h, payload, sizeof(payload), h.magic, h.version) == 0);
        CHECK(config_slot_valid(NULL, payload, sizeof(payload), h.magic, h.version) == 0);
        CHECK(config_slot_valid(&h, NULL, sizeof(payload), h.magic, h.version) == 0);
    }
    {
        const uint8_t both[] = {1U, 1U}, only_a[] = {1U, 0U}, none[] = {0U, 0U};
        CHECK(config_slot_choose(both, 1U) == 1);
        CHECK(config_slot_choose(only_a, 1U) == 0);
        CHECK(config_slot_choose(only_a, 0U) == 0);
        CHECK(config_slot_choose(none, 0U) == -1);
        CHECK(config_slot_choose(both, 2U) == -1);
        CHECK(config_slot_choose(NULL, 0U) == -1);
    }
    puts("PASS: config CRC and A/B slot logic");
    return 0;
}