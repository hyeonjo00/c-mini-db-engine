#ifndef EXECUTOR_H
#define EXECUTOR_H

#include <stdbool.h>

#include "database.h"
#include "history.h"
#include "logger.h"
#include "parser.h"
#include "benchmark.h"

typedef struct DashboardStatus {
    size_t record_count;
    size_t memory_usage_bytes;
    size_t session_queries;
} DashboardStatus;

typedef enum SummaryMessageLevel {
    SUMMARY_MESSAGE_NONE = 0,
    SUMMARY_MESSAGE_INFO,
    SUMMARY_MESSAGE_SUCCESS,
    SUMMARY_MESSAGE_WARNING
} SummaryMessageLevel;

typedef struct CommandExecutionSummary {
    CommandType type;
    SummaryMessageLevel message_level;
    char message[256];
    bool show_query_panel;
    bool show_benchmark_panel;
    bool show_help_panel;
    bool has_query;
    QuerySpec query;
    bool has_result_rows;
    QueryResult result;
    bool has_count_value;
    size_t count_value;
    QueryPlan plan;
    QueryExecutionStats stats;
    char index_used[64];
    char optimizer_path[DB_MAX_PLAN_DESCRIPTION];
    size_t database_memory_bytes;
    BenchmarkReport benchmark;
} CommandExecutionSummary;

typedef struct AppContext {
    Database database;
    History history;
    Logger logger;
    bool running;
    size_t session_queries;
    size_t database_generation;
    size_t query_cache_lookups;
    size_t query_cache_hits;
    bool cached_summary_valid;
    char cached_command[DB_MAX_COMMAND_LEN];
    size_t cached_generation;
    CommandExecutionSummary cached_summary;
    char default_csv_path[DB_MAX_PATH_LEN];
    char active_csv_path[DB_MAX_PATH_LEN];
    char log_path[DB_MAX_PATH_LEN];
    char wal_path[DB_MAX_PATH_LEN];
    char last_save_timestamp[32];
} AppContext;

bool app_context_init(AppContext *context, const char *data_path, const char *log_path, const char *wal_path, char *error, size_t error_capacity);
void app_context_destroy(AppContext *context);
bool execute_command(AppContext *context, const Command *command, CommandExecutionSummary *summary, char *error, size_t error_capacity);
void app_context_snapshot_status(const AppContext *context, DashboardStatus *status);
void command_execution_summary_destroy(CommandExecutionSummary *summary);
bool command_execution_summary_clone(
    const CommandExecutionSummary *source,
    CommandExecutionSummary *destination,
    char *error,
    size_t error_capacity
);
bool app_context_run_input(
    AppContext *context,
    const char *command_text,
    CommandExecutionSummary *summary,
    DashboardStatus *status,
    double *elapsed_ms,
    char *error,
    size_t error_capacity
);

#endif
