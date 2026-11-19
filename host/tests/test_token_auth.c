#include "xoweb_auth.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } while (0)

int main(void)
{
    uint8_t expected[32] = {0}, provided[32] = {0};
    memcpy(expected, "secret", 6U);
    memcpy(provided, "secret", 6U);
    CHECK(xoweb_token_equal(expected, 6U, provided, 6U) == 1);
    provided[0] ^= 1U;
    CHECK(xoweb_token_equal(expected, 6U, provided, 6U) == 0);
    provided[0] ^= 1U; provided[5] ^= 1U;
    CHECK(xoweb_token_equal(expected, 6U, provided, 6U) == 0);
    provided[5] ^= 1U;
    CHECK(xoweb_token_equal(expected, 6U, provided, 5U) == 0);
    CHECK(xoweb_token_equal(expected, 0U, provided, 6U) == 0);
    CHECK(xoweb_token_equal(expected, 33U, provided, 6U) == 0);
    CHECK(xoweb_token_equal(NULL, 6U, provided, 6U) == 0);
    CHECK(xoweb_token_equal(expected, 6U, NULL, 6U) == 0);
    puts("PASS: constant-time token comparison contract");
    return 0;
}