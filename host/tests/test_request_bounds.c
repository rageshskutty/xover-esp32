#include "xoweb_bounds.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static int ok(size_t b, size_t d, size_t n, size_t s)
{
    return xoweb_request_bounds_valid(b, d, n, s) == 1;
}


int main(void)
{
    CHECK(ok(1U, 1U, 0U, 0U));
    CHECK(ok(32768U, 8U, 64U, 32U));
    CHECK(!ok(0U, 1U, 0U, 0U));
    CHECK(!ok(32769U, 1U, 0U, 0U));
    CHECK(!ok(1U, 0U, 0U, 0U));
    CHECK(!ok(1U, 9U, 0U, 0U));
    CHECK(!ok(1U, 1U, 65U, 0U));
    CHECK(!ok(1U, 1U, 0U, 33U));
    CHECK(!ok(SIZE_MAX, 1U, 0U, 0U));
    CHECK(!ok(1U, SIZE_MAX, 0U, 0U));
    CHECK(!ok(1U, 1U, 0U, SIZE_MAX));
    puts("PASS: request bounds");
    return 0;
}