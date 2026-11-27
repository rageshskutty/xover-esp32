#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t min;
    uint32_t max;
    uint32_t avg;
    uint32_t p99;
} bench_stats_t;

void bench_sort_u32(uint32_t *a, size_t n);
void bench_compute_stats(const uint32_t *sorted, size_t n,
                         bench_stats_t *out);
void bench_print_csv_header(void);
void bench_print_csv_row(const char *scenario,
                         const char *impl,
                         const char *mem,
                         int channels,
                         int block,
                         int biquads_per_ch,
                         const bench_stats_t *stats,
                         double budget_us,
                         double cpu_mhz);

#ifdef __cplusplus
}
#endif