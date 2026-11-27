#include "bench_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>

static int cmp_u32(const void *a, const void *b)
{
    const uint32_t va = *(const uint32_t *)a;
    const uint32_t vb = *(const uint32_t *)b;
    if (va < vb) return -1;
    if (va > vb) return 1;
    return 0;
}

void bench_sort_u32(uint32_t *a, size_t n)
{
    qsort(a, n, sizeof(uint32_t), cmp_u32);
}

void bench_compute_stats(const uint32_t *sorted, size_t n,
                         bench_stats_t *out)
{
    uint64_t sum = 0;
    for (size_t i = 0; i < n; i++) {
        sum += sorted[i];
    }

    out->min = sorted[0];
    out->max = sorted[n - 1];
    out->avg = (uint32_t)(sum / n);

    size_t rank = (size_t)ceil(0.99 * (double)n);
    if (rank < 1) rank = 1;
    if (rank > n) rank = n;
    out->p99 = sorted[rank - 1];
}

void bench_print_csv_header(void)
{
    printf("CSV,scenario,impl,mem,channels,block,biquads_per_ch,"
           "avg_cycles,min_cycles,max_cycles,p99_cycles,"
           "budget_us,avg_load_pct,worst_load_pct,p99_load_pct\n");
}

void bench_print_csv_row(const char *scenario,
                         const char *impl,
                         const char *mem,
                         int channels,
                         int block,
                         int biquads_per_ch,
                         const bench_stats_t *stats,
                         double budget_us,
                         double cpu_mhz)
{
    const double avg_us = (double)stats->avg / cpu_mhz;
    const double max_us = (double)stats->max / cpu_mhz;
    const double p99_us = (double)stats->p99 / cpu_mhz;

    const double avg_load = 100.0 * avg_us / budget_us;
    const double worst_load = 100.0 * max_us / budget_us;
    const double p99_load = 100.0 * p99_us / budget_us;

    printf("CSV,%s,%s,%s,%d,%d,%d,"
           "%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ","
           "%.3f,%.2f,%.2f,%.2f\n",
           scenario, impl, mem, channels, block, biquads_per_ch,
           stats->avg, stats->min, stats->max, stats->p99,
           budget_us, avg_load, worst_load, p99_load);
}