#include "benchmark.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "database.h"
#include "timer.h"

static size_t benchmark_min_size(size_t left, size_t right) {
    return left < right ? left : right;
}

bool benchmark_run(size_t record_count, BenchmarkReport *report, char *error, size_t error_capacity) {
    static const char *departments[] = {
        "Cardiology",
        "Oncology",
        "Neurology",
        "Radiology",
        "Emergency",
        "Pediatrics",
        "Surgery"
    };

    Database database;
    bool database_initialized = false;
    double started_at;
    size_t index;

    if (report == NULL) {
        snprintf(error, error_capacity, "Benchmark received a null report pointer.");
        return false;
    }

    memset(report, 0, sizeof(*report));
    if (!database_init(&database)) {
        snprintf(error, error_capacity, "Failed to initialize benchmark database.");
        return false;
    }
    database_initialized = true;

    srand(1337);
    started_at = timer_now_ms();
    for (index = 0; index < record_count; ++index) {
        Record record;
        memset(&record, 0, sizeof(record));
        record.id = (int)(index + 1);
        snprintf(record.name, sizeof(record.name), "Employee%06zu", index + 1);
        record.age = 20 + (rand() % 45);
        snprintf(record.department, sizeof(record.department), "%s", departments[rand() % (sizeof(departments) / sizeof(departments[0]))]);

        if (!database_insert_record(&database, &record, error, error_capacity)) {
            goto cleanup;
        }
    }
    report->insert_ms = timer_now_ms() - started_at;
    report->record_count = record_count;
    report->insert_throughput_rows_per_sec = report->insert_ms > 0.0
        ? ((double)record_count * 1000.0) / report->insert_ms
        : 0.0;

    report->exact_lookup_count = benchmark_min_size(record_count, 1000);
    started_at = timer_now_ms();
    for (index = 0; index < report->exact_lookup_count; ++index) {
        QuerySpec query;
        QueryResult result;
        QueryPlan plan;
        QueryExecutionStats stats;
        int random_id = (rand() % (int)record_count) + 1;

        memset(&query, 0, sizeof(query));
        memset(&result, 0, sizeof(result));
        memset(&plan, 0, sizeof(plan));
        memset(&stats, 0, sizeof(stats));

        query.predicate.condition_count = 1;
        query.predicate.conditions[0].active = true;
        query.predicate.conditions[0].field = FIELD_ID;
        query.predicate.conditions[0].op = CMP_EQ;
        query.predicate.conditions[0].value_is_int = true;
        query.predicate.conditions[0].int_value = random_id;
        snprintf(query.predicate.conditions[0].raw_value, sizeof(query.predicate.conditions[0].raw_value), "%d", random_id);

        if (!database_select(&database, &query, &result, &plan, &stats, error, error_capacity)) {
            goto cleanup;
        }
        query_result_destroy(&result);
        safe_copy(report->exact_lookup_plan, sizeof(report->exact_lookup_plan), plan.description);
    }
    report->exact_lookup_ms = timer_now_ms() - started_at;
    report->exact_lookup_latency_ms = report->exact_lookup_count > 0
        ? report->exact_lookup_ms / (double)report->exact_lookup_count
        : 0.0;

    report->range_query_count = 100;
    started_at = timer_now_ms();
    for (index = 0; index < report->range_query_count; ++index) {
        QuerySpec query;
        QueryResult result;
        QueryPlan plan;
        QueryExecutionStats stats;
        int threshold = 25 + (rand() % 30);

        memset(&query, 0, sizeof(query));
        memset(&result, 0, sizeof(result));
        memset(&plan, 0, sizeof(plan));
        memset(&stats, 0, sizeof(stats));

        query.predicate.condition_count = 1;
        query.predicate.conditions[0].active = true;
        query.predicate.conditions[0].field = FIELD_AGE;
        query.predicate.conditions[0].op = CMP_GT;
        query.predicate.conditions[0].value_is_int = true;
        query.predicate.conditions[0].int_value = threshold;
        snprintf(query.predicate.conditions[0].raw_value, sizeof(query.predicate.conditions[0].raw_value), "%d", threshold);
        query.order_by.active = true;
        query.order_by.field = FIELD_AGE;
        query.has_limit = true;
        query.limit = 128;

        if (!database_select(&database, &query, &result, &plan, &stats, error, error_capacity)) {
            goto cleanup;
        }
        query_result_destroy(&result);
        safe_copy(report->range_scan_plan, sizeof(report->range_scan_plan), plan.description);
    }
    report->range_scan_ms = timer_now_ms() - started_at;
    report->range_scan_latency_ms = report->range_query_count > 0
        ? report->range_scan_ms / (double)report->range_query_count
        : 0.0;
    report->memory_usage_bytes = database_estimate_memory_usage(&database);

cleanup:
    if (database_initialized) {
        database_destroy(&database);
    }
    return report->record_count == record_count;
}
