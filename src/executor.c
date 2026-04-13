#include "executor.h"

#include <stdio.h>
#include <string.h>

#include "benchmark.h"
#include "persistence.h"
#include "planner.h"
#include "timer.h"
#include "wal.h"

static void summary_reset(CommandExecutionSummary *summary, CommandType type) {
    if (summary == NULL) {
        return;
    }

    memset(summary, 0, sizeof(*summary));
    summary->type = type;
}

static bool command_is_cacheable(CommandType type) {
    return type == CMD_SELECT || type == CMD_COUNT;
}

static bool command_mutates_database(CommandType type) {
    return type == CMD_INSERT ||
           type == CMD_UPDATE ||
           type == CMD_DELETE ||
           type == CMD_LOAD;
}

static void summary_set_message(CommandExecutionSummary *summary, SummaryMessageLevel level, const char *message) {
    if (summary == NULL) {
        return;
    }

    summary->message_level = level;
    if (message != NULL) {
        safe_copy(summary->message, sizeof(summary->message), message);
    }
}

static void summary_set_index_used(CommandExecutionSummary *summary, QueryPlanType plan_type) {
    const char *index_name = "None";

    if (summary == NULL) {
        return;
    }

    switch (plan_type) {
        case PLAN_HASH_ID_EXACT:
            index_name = "Hash(id)";
            break;
        case PLAN_HASH_DEPARTMENT_EXACT:
            index_name = "Hash(department)";
            break;
        case PLAN_BPTREE_ID_EXACT:
        case PLAN_BPTREE_ID_RANGE:
        case PLAN_BPTREE_ID_ORDERED:
            index_name = "B+Tree(id)";
            break;
        case PLAN_BPTREE_AGE_EXACT:
        case PLAN_BPTREE_AGE_RANGE:
        case PLAN_BPTREE_AGE_ORDERED:
            index_name = "B+Tree(age)";
            break;
        case PLAN_FULL_SCAN:
        default:
            index_name = "None";
            break;
    }

    safe_copy(summary->index_used, sizeof(summary->index_used), index_name);
}

static void summary_capture_plan(
    CommandExecutionSummary *summary,
    const QuerySpec *query,
    const QueryPlan *plan,
    const QueryExecutionStats *stats,
    size_t database_memory_bytes
) {
    if (summary == NULL) {
        return;
    }

    summary->show_query_panel = true;
    summary->database_memory_bytes = database_memory_bytes;

    if (query != NULL) {
        summary->has_query = true;
        summary->query = *query;
    }

    if (plan != NULL) {
        summary->plan = *plan;
        summary_set_index_used(summary, plan->type);
        safe_copy(summary->optimizer_path, sizeof(summary->optimizer_path), plan_type_to_string(plan->type));
    } else {
        safe_copy(summary->index_used, sizeof(summary->index_used), "None");
        safe_copy(summary->optimizer_path, sizeof(summary->optimizer_path), "NONE");
    }

    if (stats != NULL) {
        summary->stats = *stats;
    }
}

static void summary_capture_result(CommandExecutionSummary *summary, QueryResult *result) {
    if (summary == NULL || result == NULL) {
        return;
    }

    summary->has_result_rows = true;
    summary->result = *result;
    memset(result, 0, sizeof(*result));
}

static void snapshot_database_memory(CommandExecutionSummary *summary, const Database *database) {
    if (summary == NULL || database == NULL) {
        return;
    }

    summary->database_memory_bytes = database_estimate_memory_usage(database);
}

static bool command_counts_toward_session(CommandType type) {
    switch (type) {
        case CMD_INSERT:
        case CMD_SELECT:
        case CMD_COUNT:
        case CMD_UPDATE:
        case CMD_DELETE:
        case CMD_SAVE:
        case CMD_LOAD:
        case CMD_BENCHMARK:
            return true;
        case CMD_HISTORY:
        case CMD_HELP:
        case CMD_EXIT:
        default:
            return false;
    }
}

static void app_context_invalidate_cache(AppContext *context) {
    if (context == NULL) {
        return;
    }

    if (context->cached_summary_valid) {
        command_execution_summary_destroy(&context->cached_summary);
        context->cached_summary_valid = false;
    }

    context->cached_command[0] = '\0';
    context->cached_generation = 0;
}

bool app_context_init(AppContext *context, const char *data_path, const char *log_path, const char *wal_path, char *error, size_t error_capacity) {
    if (context == NULL || data_path == NULL || log_path == NULL || wal_path == NULL) {
        snprintf(error, error_capacity, "Application context init received a null pointer.");
        return false;
    }

    memset(context, 0, sizeof(*context));
    if (!safe_copy(context->default_csv_path, sizeof(context->default_csv_path), data_path) ||
        !safe_copy(context->active_csv_path, sizeof(context->active_csv_path), data_path) ||
        !safe_copy(context->log_path, sizeof(context->log_path), log_path) ||
        !safe_copy(context->wal_path, sizeof(context->wal_path), wal_path)) {
        snprintf(error, error_capacity, "Configured paths exceeded maximum size.");
        return false;
    }

    safe_copy(context->last_save_timestamp, sizeof(context->last_save_timestamp), "Never");

    if (!database_init(&context->database)) {
        snprintf(error, error_capacity, "Failed to initialize database.");
        return false;
    }

    if (!history_init(&context->history)) {
        database_destroy(&context->database);
        snprintf(error, error_capacity, "Failed to initialize command history.");
        return false;
    }

    if (!logger_init(&context->logger, context->log_path)) {
        history_destroy(&context->history);
        database_destroy(&context->database);
        snprintf(error, error_capacity, "Failed to initialize query logger at %s.", context->log_path);
        return false;
    }

    context->running = true;
    return true;
}

void app_context_destroy(AppContext *context) {
    if (context == NULL) {
        return;
    }

    logger_close(&context->logger);
    app_context_invalidate_cache(context);
    history_destroy(&context->history);
    database_destroy(&context->database);
}

void app_context_snapshot_status(const AppContext *context, DashboardStatus *status) {
    if (context == NULL || status == NULL) {
        return;
    }

    memset(status, 0, sizeof(*status));
    status->record_count = context->database.count;
    status->memory_usage_bytes = database_estimate_memory_usage(&context->database);
    status->session_queries = context->session_queries;
}

void command_execution_summary_destroy(CommandExecutionSummary *summary) {
    if (summary == NULL) {
        return;
    }

    if (summary->has_result_rows) {
        query_result_destroy(&summary->result);
    }

    memset(summary, 0, sizeof(*summary));
}

bool command_execution_summary_clone(
    const CommandExecutionSummary *source,
    CommandExecutionSummary *destination,
    char *error,
    size_t error_capacity
) {
    CommandExecutionSummary clone;

    if (source == NULL || destination == NULL) {
        snprintf(error, error_capacity, "Summary clone received a null pointer.");
        return false;
    }

    memset(&clone, 0, sizeof(clone));
    clone = *source;
    memset(&clone.result, 0, sizeof(clone.result));
    clone.has_result_rows = false;

    if (source->has_result_rows && source->result.count > 0) {
        clone.result.rows = (Record **)db_calloc(source->result.count, sizeof(Record *));
        if (clone.result.rows == NULL) {
            snprintf(error, error_capacity, "Failed to clone query result rows.");
            return false;
        }

        memcpy(clone.result.rows, source->result.rows, source->result.count * sizeof(Record *));
        clone.result.count = source->result.count;
        clone.result.capacity = source->result.count;
        clone.has_result_rows = true;
    } else if (source->has_result_rows) {
        clone.has_result_rows = true;
    }

    command_execution_summary_destroy(destination);
    *destination = clone;
    return true;
}

bool execute_command(AppContext *context, const Command *command, CommandExecutionSummary *summary, char *error, size_t error_capacity) {
    const char *path;

    if (context == NULL || command == NULL) {
        snprintf(error, error_capacity, "Executor received a null pointer.");
        return false;
    }

    summary_reset(summary, command->type);
    path = command->has_path ? command->path : context->default_csv_path;

    switch (command->type) {
        case CMD_INSERT:
            if (!database_insert_record(&context->database, &command->record, error, error_capacity)) {
                return false;
            }
            if (!wal_append_insert(context->wal_path, &command->record, error, error_capacity)) {
                return false;
            }
            snapshot_database_memory(summary, &context->database);
            summary_set_message(summary, SUMMARY_MESSAGE_SUCCESS, "Record inserted.");
            return true;

        case CMD_SELECT: {
            QueryResult result;
            QueryPlan plan;
            QueryExecutionStats stats;
            memset(&result, 0, sizeof(result));
            memset(&plan, 0, sizeof(plan));
            memset(&stats, 0, sizeof(stats));

            if (!database_select(&context->database, &command->query, &result, &plan, &stats, error, error_capacity)) {
                return false;
            }

            summary_capture_plan(summary, &command->query, &plan, &stats, database_estimate_memory_usage(&context->database));
            summary_capture_result(summary, &result);
            if (summary == NULL) {
                query_result_destroy(&result);
            }
            summary_set_message(summary, SUMMARY_MESSAGE_INFO, "Query executed.");
            return true;
        }

        case CMD_COUNT: {
            size_t count = 0;
            QueryPlan plan;
            QueryExecutionStats stats;
            memset(&plan, 0, sizeof(plan));
            memset(&stats, 0, sizeof(stats));

            if (!database_count_matching(&context->database, &command->query, &count, &plan, &stats, error, error_capacity)) {
                return false;
            }

            summary_capture_plan(summary, &command->query, &plan, &stats, database_estimate_memory_usage(&context->database));
            summary->has_count_value = true;
            summary->count_value = count;
            summary_set_message(summary, SUMMARY_MESSAGE_INFO, "Aggregate query executed.");
            return true;
        }

        case CMD_UPDATE: {
            int resulting_id = command->target_id;
            const Record *updated_record;
            size_t index;

            if (!database_update_by_id(
                    &context->database,
                    command->target_id,
                    command->assignments,
                    command->assignment_count,
                    error,
                    error_capacity)) {
                return false;
            }

            for (index = 0; index < command->assignment_count; ++index) {
                if (command->assignments[index].field == FIELD_ID) {
                    resulting_id = command->assignments[index].int_value;
                }
            }

            updated_record = database_find_by_id(&context->database, resulting_id);
            if (updated_record == NULL) {
                snprintf(error, error_capacity, "Updated record could not be located for WAL logging.");
                return false;
            }

            if (!wal_append_update(context->wal_path, command->target_id, updated_record, error, error_capacity)) {
                return false;
            }

            snapshot_database_memory(summary, &context->database);
            summary_set_message(summary, SUMMARY_MESSAGE_SUCCESS, "Record updated.");
            return true;
        }

        case CMD_DELETE:
            if (!database_delete_by_id(&context->database, command->target_id, error, error_capacity)) {
                return false;
            }
            if (!wal_append_delete(context->wal_path, command->target_id, error, error_capacity)) {
                return false;
            }
            snapshot_database_memory(summary, &context->database);
            summary_set_message(summary, SUMMARY_MESSAGE_SUCCESS, "Record deleted.");
            return true;

        case CMD_SAVE:
            if (!persistence_save_csv(&context->database, path, error, error_capacity)) {
                return false;
            }
            format_current_timestamp(context->last_save_timestamp, sizeof(context->last_save_timestamp));
            if (strcmp(path, context->default_csv_path) == 0) {
                if (!wal_checkpoint(context->wal_path, error, error_capacity)) {
                    return false;
                }
                summary_set_message(summary, SUMMARY_MESSAGE_SUCCESS, "Saved snapshot and checkpointed WAL.");
            } else {
                summary_set_message(summary, SUMMARY_MESSAGE_SUCCESS, "Saved snapshot to custom path.");
            }
            snapshot_database_memory(summary, &context->database);
            return true;

        case CMD_LOAD: {
            Database loaded;
            bool loaded_initialized = false;

            if (!database_init(&loaded)) {
                snprintf(error, error_capacity, "Failed to initialize temporary database for LOAD.");
                return false;
            }
            loaded_initialized = true;

            if (!persistence_load_csv(&loaded, path, error, error_capacity)) {
                database_destroy(&loaded);
                return false;
            }

            if (!wal_replay(&loaded, context->wal_path, error, error_capacity)) {
                database_destroy(&loaded);
                return false;
            }

            database_destroy(&context->database);
            context->database = loaded;
            memset(&loaded, 0, sizeof(loaded));
            loaded_initialized = false;

            snapshot_database_memory(summary, &context->database);
            summary_set_message(summary, SUMMARY_MESSAGE_SUCCESS, "Loaded snapshot and replayed WAL.");
            safe_copy(context->active_csv_path, sizeof(context->active_csv_path), path);

            if (loaded_initialized) {
                database_destroy(&loaded);
            }
            return true;
        }

        case CMD_HISTORY:
            snapshot_database_memory(summary, &context->database);
            summary_set_message(summary, SUMMARY_MESSAGE_INFO, "Recent commands are shown in the history panel.");
            return true;

        case CMD_BENCHMARK: {
            BenchmarkReport report;
            if (!benchmark_run(command->benchmark_record_count, &report, error, error_capacity)) {
                return false;
            }

            if (summary != NULL) {
                summary->show_benchmark_panel = true;
                summary->benchmark = report;
            }
            snapshot_database_memory(summary, &context->database);
            summary_set_message(summary, SUMMARY_MESSAGE_INFO, "Benchmark completed.");
            return true;
        }

        case CMD_HELP:
            if (summary != NULL) {
                summary->show_help_panel = true;
            }
            snapshot_database_memory(summary, &context->database);
            summary_set_message(summary, SUMMARY_MESSAGE_INFO, "Command reference ready.");
            return true;

        case CMD_EXIT:
            context->running = false;
            snapshot_database_memory(summary, &context->database);
            summary_set_message(summary, SUMMARY_MESSAGE_INFO, "Shutting down MiniDB Engine.");
            return true;

        default:
            snprintf(error, error_capacity, "Unsupported command type.");
            return false;
    }
}

bool app_context_run_input(
    AppContext *context,
    const char *command_text,
    CommandExecutionSummary *summary,
    DashboardStatus *status,
    double *elapsed_ms,
    char *error,
    size_t error_capacity
) {
    char command_buffer[DB_MAX_COMMAND_LEN];
    char *trimmed;
    Command command;
    CommandExecutionSummary next_summary;
    double started_at;
    bool success;
    bool history_recorded = true;
    bool cacheable = false;

    if (context == NULL || command_text == NULL) {
        snprintf(error, error_capacity, "Application command runner received a null pointer.");
        return false;
    }

    if (!safe_copy(command_buffer, sizeof(command_buffer), command_text)) {
        snprintf(error, error_capacity, "Command exceeded maximum size.");
        return false;
    }

    trimmed = trim_whitespace(command_buffer);
    if (trimmed == NULL || *trimmed == '\0') {
        snprintf(error, error_capacity, "Empty command.");
        return false;
    }

    memset(&next_summary, 0, sizeof(next_summary));
    history_recorded = history_add(&context->history, trimmed);

    started_at = timer_now_ms();
    memset(&command, 0, sizeof(command));
    if (!parse_command(trimmed, &command, error, error_capacity)) {
        if (elapsed_ms != NULL) {
            *elapsed_ms = timer_now_ms() - started_at;
        }
        logger_log_query(&context->logger, trimmed, "PARSE_ERROR", elapsed_ms == NULL ? 0.0 : *elapsed_ms);
        app_context_snapshot_status(context, status);
        return false;
    }

    cacheable = command_is_cacheable(command.type);
    if (cacheable) {
        context->query_cache_lookups += 1;
        if (context->cached_summary_valid &&
            context->cached_generation == context->database_generation &&
            strcmp(context->cached_command, trimmed) == 0) {
            if (elapsed_ms != NULL) {
                *elapsed_ms = timer_now_ms() - started_at;
            }

            logger_log_query(&context->logger, trimmed, "CACHE_HIT", elapsed_ms == NULL ? 0.0 : *elapsed_ms);
            context->query_cache_hits += 1;
            if (command_counts_toward_session(command.type)) {
                context->session_queries += 1;
            }

            if (summary != NULL) {
                if (!command_execution_summary_clone(&context->cached_summary, &next_summary, error, error_capacity)) {
                    app_context_snapshot_status(context, status);
                    command_execution_summary_destroy(&next_summary);
                    return false;
                }
                summary_set_message(&next_summary, SUMMARY_MESSAGE_INFO, "Query served from session cache.");
                command_execution_summary_destroy(summary);
                *summary = next_summary;
                memset(&next_summary, 0, sizeof(next_summary));
            }

            app_context_snapshot_status(context, status);
            command_execution_summary_destroy(&next_summary);
            return true;
        }
    }

    success = execute_command(context, &command, summary == NULL ? NULL : &next_summary, error, error_capacity);
    if (elapsed_ms != NULL) {
        *elapsed_ms = timer_now_ms() - started_at;
    }

    logger_log_query(&context->logger, trimmed, success ? "OK" : "ERROR", elapsed_ms == NULL ? 0.0 : *elapsed_ms);

    if (success && command_counts_toward_session(command.type)) {
        context->session_queries += 1;
    }

    if (success && command_mutates_database(command.type)) {
        context->database_generation += 1;
        app_context_invalidate_cache(context);
    }

    if (success && cacheable && summary != NULL) {
        app_context_invalidate_cache(context);
        if (command_execution_summary_clone(&next_summary, &context->cached_summary, error, error_capacity)) {
            context->cached_summary_valid = true;
            context->cached_generation = context->database_generation;
            safe_copy(context->cached_command, sizeof(context->cached_command), trimmed);
        } else {
            app_context_invalidate_cache(context);
        }
    }

    if (success && summary != NULL) {
        if (!history_recorded && next_summary.message_level == SUMMARY_MESSAGE_NONE) {
            summary_set_message(&next_summary, SUMMARY_MESSAGE_WARNING, "Command executed, but history could not be recorded.");
        }
        command_execution_summary_destroy(summary);
        *summary = next_summary;
        memset(&next_summary, 0, sizeof(next_summary));
    }

    app_context_snapshot_status(context, status);
    command_execution_summary_destroy(&next_summary);
    return success;
}
