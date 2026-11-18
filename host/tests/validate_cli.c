#include <stdio.h>
#include <string.h>

#include "dspgraph.h"

/* Prints the distinct issue codes for one config file, one per line. */
int main(int argc, char **argv)
{
    static char buf[32768];
    dspg_report_t rep;
    size_t n;
    FILE *f;
    if (argc != 2 || (f = fopen(argv[1], "rb")) == NULL) {
        return 2;
    }
    n = fread(buf, 1U, sizeof(buf), f);
    (void)fclose(f);
    (void)dspg_validate(buf, n, &rep);
    for (uint8_t i = 0U; i < rep.n; i++) {
        (void)printf("%s\n", rep.items[i].code);
    }
    return 0;
}
