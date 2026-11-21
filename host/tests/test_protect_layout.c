#include "dspgraph.h"

#include <stddef.h>

_Static_assert(sizeof(dspg_protect_t) == 52U, "protection record size");
_Static_assert(offsetof(dspg_protect_t, out_id) == 0U, "out_id offset");
_Static_assert(offsetof(dspg_protect_t, max_gain_db) == 32U, "max_gain_db offset");
_Static_assert(offsetof(dspg_protect_t, stop_below_hz) == 36U, "stop_below_hz offset");
_Static_assert(offsetof(dspg_protect_t, atten_below_db) == 40U, "atten_below_db offset");
_Static_assert(offsetof(dspg_protect_t, stop_above_hz) == 44U, "stop_above_hz offset");
_Static_assert(offsetof(dspg_protect_t, atten_above_db) == 48U, "atten_above_db offset");

int main(void)
{
    return 0;
}
