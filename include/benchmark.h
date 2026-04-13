#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <stdbool.h>
#include <stddef.h>

#include "common.h"

typedef struct BenchmarkReport {
    size_t record_count;
    size_t exact_lookup_count;
    size_t range_query_count;
    double insert_ms;
    double insert_throughput_rows_per_sec;
    double exact_lookup_ms;
    double exact_lookup_latency_ms;
    double range_scan_ms;
    double range_scan_latency_ms;
    size_t memory_usage_bytes;
    char exact_lookup_plan[DB_MAX_PLAN_DESCRIPTION];
    char range_scan_plan[DB_MAX_PLAN_DESCRIPTION];
} BenchmarkReport;

bool benchmark_run(size_t record_count, BenchmarkReport *report, char *error, size_t error_capacity);

#endif
