#include "database.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "planner.h"

static Field g_sort_field = FIELD_NONE;

typedef struct QueryCollectContext {
    const Predicate *predicate;
    QueryResult *result;
    QueryExecutionStats *stats;
    bool stop_at_limit;
    size_t limit;
    bool failed;
    char *error;
    size_t error_capacity;
} QueryCollectContext;

typedef struct QueryCountContext {
    const Predicate *predicate;
    QueryExecutionStats *stats;
    size_t count;
} QueryCountContext;

static bool query_result_reserve(QueryResult *result, size_t required_capacity) {
    Record **resized;
    size_t new_capacity;

    if (result == NULL) {
        return false;
    }

    if (result->capacity >= required_capacity) {
        return true;
    }

    new_capacity = result->capacity == 0 ? 8 : result->capacity * 2;
    while (new_capacity < required_capacity) {
        new_capacity *= 2;
    }

    resized = (Record **)db_realloc(result->rows, new_capacity * sizeof(Record *));
    if (resized == NULL) {
        return false;
    }

    result->rows = resized;
    result->capacity = new_capacity;
    return true;
}

static bool query_result_append(QueryResult *result, Record *record) {
    if (result == NULL || record == NULL) {
        return false;
    }

    if (!query_result_reserve(result, result->count + 1)) {
        return false;
    }

    result->rows[result->count++] = record;
    return true;
}

static void query_result_apply_limit(QueryResult *result, size_t limit) {
    if (result != NULL && result->count > limit) {
        result->count = limit;
    }
}

static int compare_records_for_sort(const void *left, const void *right) {
    const Record *lhs = *(const Record *const *)left;
    const Record *rhs = *(const Record *const *)right;
    int comparison = 0;

    switch (g_sort_field) {
        case FIELD_ID:
            comparison = (lhs->id > rhs->id) - (lhs->id < rhs->id);
            break;
        case FIELD_AGE:
            comparison = (lhs->age > rhs->age) - (lhs->age < rhs->age);
            break;
        case FIELD_NAME:
            comparison = strcmp(lhs->name, rhs->name);
            break;
        case FIELD_DEPARTMENT:
            comparison = strcmp(lhs->department, rhs->department);
            break;
        default:
            comparison = 0;
            break;
    }

    if (comparison != 0) {
        return comparison;
    }

    return (lhs->id > rhs->id) - (lhs->id < rhs->id);
}

static void query_result_sort(QueryResult *result, Field field) {
    if (result == NULL || result->count < 2 || field == FIELD_NONE) {
        return;
    }

    g_sort_field = field;
    qsort(result->rows, result->count, sizeof(Record *), compare_records_for_sort);
    g_sort_field = FIELD_NONE;
}

static bool database_reserve(Database *database, size_t required_capacity) {
    Record **resized;
    size_t new_capacity;

    if (database == NULL) {
        return false;
    }

    if (database->capacity >= required_capacity) {
        return true;
    }

    new_capacity = database->capacity == 0 ? 16 : database->capacity * 2;
    while (new_capacity < required_capacity) {
        new_capacity *= 2;
    }

    resized = (Record **)db_realloc(database->records, new_capacity * sizeof(Record *));
    if (resized == NULL) {
        return false;
    }

    database->records = resized;
    database->capacity = new_capacity;
    return true;
}

static bool validate_record(const Record *record, char *error, size_t error_capacity) {
    if (record == NULL) {
        snprintf(error, error_capacity, "Record pointer was null.");
        return false;
    }

    if (record->id < 0) {
        snprintf(error, error_capacity, "id must be non-negative.");
        return false;
    }

    if (record->name[0] == '\0') {
        snprintf(error, error_capacity, "name cannot be empty.");
        return false;
    }

    if (record->department[0] == '\0') {
        snprintf(error, error_capacity, "department cannot be empty.");
        return false;
    }

    if (record->age < 0 || record->age > 130) {
        snprintf(error, error_capacity, "age must be between 0 and 130.");
        return false;
    }

    return true;
}

static bool add_record_to_indexes(Database *database, Record *record, char *error, size_t error_capacity) {
    char rollback_error[256];

    if (!id_index_insert(&database->id_index, record->id, record)) {
        snprintf(error, error_capacity, "Failed to add id hash index entry for id=%d.", record->id);
        return false;
    }

    if (!department_index_insert(&database->department_index, record->department, record)) {
        id_index_remove(&database->id_index, record->id);
        snprintf(error, error_capacity, "Failed to add department hash index entry.");
        return false;
    }

    rollback_error[0] = '\0';
    if (!bptree_insert(&database->id_tree, record->id, record, error, error_capacity)) {
        department_index_remove(&database->department_index, record->department, record);
        id_index_remove(&database->id_index, record->id);
        return false;
    }

    if (!bptree_insert(&database->age_tree, record->age, record, error, error_capacity)) {
        bptree_remove(&database->id_tree, record->id, record, rollback_error, sizeof(rollback_error));
        department_index_remove(&database->department_index, record->department, record);
        id_index_remove(&database->id_index, record->id);
        return false;
    }
    return true;
}

static bool remove_record_from_indexes(Database *database, Record *record, char *error, size_t error_capacity) {
    char rollback_error[256];

    rollback_error[0] = '\0';
    if (!bptree_remove(&database->id_tree, record->id, record, error, error_capacity)) {
        return false;
    }

    if (!bptree_remove(&database->age_tree, record->age, record, error, error_capacity)) {
        bptree_insert(&database->id_tree, record->id, record, rollback_error, sizeof(rollback_error));
        return false;
    }

    id_index_remove(&database->id_index, record->id);
    department_index_remove(&database->department_index, record->department, record);
    return true;
}

bool database_init(Database *database) {
    if (database == NULL) {
        return false;
    }

    memset(database, 0, sizeof(*database));
    if (!id_index_init(&database->id_index, DB_ID_INDEX_BUCKETS)) {
        return false;
    }
    if (!department_index_init(&database->department_index, DB_DEPARTMENT_INDEX_BUCKETS)) {
        id_index_destroy(&database->id_index);
        return false;
    }
    if (!bptree_init(&database->id_tree, true, "id")) {
        department_index_destroy(&database->department_index);
        id_index_destroy(&database->id_index);
        return false;
    }
    if (!bptree_init(&database->age_tree, false, "age")) {
        bptree_destroy(&database->id_tree);
        department_index_destroy(&database->department_index);
        id_index_destroy(&database->id_index);
        return false;
    }

    return true;
}

void database_destroy(Database *database) {
    size_t index;

    if (database == NULL) {
        return;
    }

    for (index = 0; index < database->count; ++index) {
        free(database->records[index]);
    }
    free(database->records);

    bptree_destroy(&database->id_tree);
    bptree_destroy(&database->age_tree);
    id_index_destroy(&database->id_index);
    department_index_destroy(&database->department_index);

    database->records = NULL;
    database->count = 0;
    database->capacity = 0;
}

bool database_clear(Database *database, char *error, size_t error_capacity) {
    if (database == NULL) {
        snprintf(error, error_capacity, "Database pointer was null.");
        return false;
    }

    database_destroy(database);
    if (!database_init(database)) {
        snprintf(error, error_capacity, "Failed to reinitialize database after clearing.");
        return false;
    }

    return true;
}

bool database_insert_record(Database *database, const Record *record, char *error, size_t error_capacity) {
    Record *copy;

    if (database == NULL) {
        snprintf(error, error_capacity, "Database pointer was null.");
        return false;
    }

    if (!validate_record(record, error, error_capacity)) {
        return false;
    }

    if (id_index_find(&database->id_index, record->id) != NULL) {
        snprintf(error, error_capacity, "Record with id=%d already exists.", record->id);
        return false;
    }

    if (!database_reserve(database, database->count + 1)) {
        snprintf(error, error_capacity, "Failed to grow record storage.");
        return false;
    }

    copy = (Record *)db_calloc(1, sizeof(Record));
    if (copy == NULL) {
        snprintf(error, error_capacity, "Failed to allocate record.");
        return false;
    }

    *copy = *record;
    copy->slot = database->count;
    database->records[database->count] = copy;
    database->count += 1;

    if (!add_record_to_indexes(database, copy, error, error_capacity)) {
        database->count -= 1;
        database->records[database->count] = NULL;
        free(copy);
        return false;
    }

    return true;
}

bool database_delete_by_id(Database *database, int id, char *error, size_t error_capacity) {
    Record *record;
    size_t slot;
    size_t last_slot;

    if (database == NULL) {
        snprintf(error, error_capacity, "Database pointer was null.");
        return false;
    }

    record = id_index_find(&database->id_index, id);
    if (record == NULL) {
        snprintf(error, error_capacity, "Record with id=%d was not found.", id);
        return false;
    }

    if (!remove_record_from_indexes(database, record, error, error_capacity)) {
        return false;
    }

    slot = record->slot;
    last_slot = database->count - 1;
    if (slot != last_slot) {
        Record *last_record = database->records[last_slot];
        database->records[slot] = last_record;
        last_record->slot = slot;
    }

    database->records[last_slot] = NULL;
    database->count -= 1;
    free(record);
    return true;
}

bool database_update_by_id(
    Database *database,
    int id,
    const UpdateAssignment *assignments,
    size_t assignment_count,
    char *error,
    size_t error_capacity
) {
    Record *record;
    Record previous;
    Record updated;
    size_t index;
    char restore_error[256];

    if (database == NULL) {
        snprintf(error, error_capacity, "Database pointer was null.");
        return false;
    }

    if (assignments == NULL || assignment_count == 0) {
        snprintf(error, error_capacity, "UPDATE requires at least one assignment.");
        return false;
    }

    record = id_index_find(&database->id_index, id);
    if (record == NULL) {
        snprintf(error, error_capacity, "Record with id=%d was not found.", id);
        return false;
    }

    previous = *record;
    updated = previous;

    for (index = 0; index < assignment_count; ++index) {
        switch (assignments[index].field) {
            case FIELD_ID:
                updated.id = assignments[index].int_value;
                break;
            case FIELD_NAME:
                if (!safe_copy(updated.name, sizeof(updated.name), assignments[index].raw_value)) {
                    snprintf(error, error_capacity, "Updated name exceeds %d characters.", DB_MAX_NAME_LEN - 1);
                    return false;
                }
                break;
            case FIELD_AGE:
                updated.age = assignments[index].int_value;
                break;
            case FIELD_DEPARTMENT:
                if (!safe_copy(updated.department, sizeof(updated.department), assignments[index].raw_value)) {
                    snprintf(error, error_capacity, "Updated department exceeds %d characters.", DB_MAX_DEPARTMENT_LEN - 1);
                    return false;
                }
                break;
            default:
                snprintf(error, error_capacity, "Unsupported field in UPDATE.");
                return false;
        }
    }

    if (!validate_record(&updated, error, error_capacity)) {
        return false;
    }

    if (updated.id != previous.id && id_index_find(&database->id_index, updated.id) != NULL) {
        snprintf(error, error_capacity, "Record with id=%d already exists.", updated.id);
        return false;
    }

    if (!remove_record_from_indexes(database, record, error, error_capacity)) {
        return false;
    }

    *record = updated;
    record->slot = previous.slot;

    if (!add_record_to_indexes(database, record, error, error_capacity)) {
        char details[256];
        safe_copy(details, sizeof(details), error);
        *record = previous;
        record->slot = previous.slot;
        restore_error[0] = '\0';
        if (!add_record_to_indexes(database, record, restore_error, sizeof(restore_error))) {
            snprintf(error, error_capacity, "Update rollback failed after index rebuild error: %s | rollback: %s", details, restore_error);
        } else {
            snprintf(error, error_capacity, "%s", details);
        }
        return false;
    }

    return true;
}

const Record *database_find_by_id(const Database *database, int id) {
    if (database == NULL) {
        return NULL;
    }

    return id_index_find(&database->id_index, id);
}

bool database_condition_matches(const Record *record, const Condition *condition) {
    int comparison = 0;

    if (record == NULL) {
        return false;
    }
    if (condition == NULL || !condition->active) {
        return true;
    }

    if (condition->field == FIELD_ID || condition->field == FIELD_AGE) {
        int actual = condition->field == FIELD_ID ? record->id : record->age;
        int expected = condition->int_value;
        comparison = (actual > expected) - (actual < expected);
    } else {
        const char *actual = condition->field == FIELD_NAME ? record->name : record->department;
        comparison = strcmp(actual, condition->raw_value);
    }

    switch (condition->op) {
        case CMP_EQ:
            return comparison == 0;
        case CMP_NEQ:
            return comparison != 0;
        case CMP_GT:
            return comparison > 0;
        case CMP_GTE:
            return comparison >= 0;
        case CMP_LT:
            return comparison < 0;
        case CMP_LTE:
            return comparison <= 0;
        default:
            return false;
    }
}

bool predicate_is_empty(const Predicate *predicate) {
    return predicate == NULL || predicate->condition_count == 0;
}

bool predicate_matches_record(const Record *record, const Predicate *predicate) {
    size_t index;
    bool result;
    bool current_group;

    if (predicate_is_empty(predicate)) {
        return true;
    }

    current_group = database_condition_matches(record, &predicate->conditions[0]);
    result = false;

    for (index = 1; index < predicate->condition_count; ++index) {
        if (predicate->operators[index - 1] == LOGIC_AND) {
            current_group = current_group && database_condition_matches(record, &predicate->conditions[index]);
        } else if (predicate->operators[index - 1] == LOGIC_OR) {
            result = result || current_group;
            current_group = database_condition_matches(record, &predicate->conditions[index]);
        }
    }

    return result || current_group;
}

static bool collect_record_from_tree(Record *record, void *context) {
    QueryCollectContext *collector = (QueryCollectContext *)context;

    if (collector->stats != NULL) {
        collector->stats->rows_scanned += 1;
    }

    if (!predicate_matches_record(record, collector->predicate)) {
        return true;
    }

    if (!query_result_append(collector->result, record)) {
        collector->failed = true;
        snprintf(collector->error, collector->error_capacity, "Failed to build query result.");
        return false;
    }

    if (collector->stats != NULL) {
        collector->stats->rows_returned += 1;
    }

    if (collector->stop_at_limit && collector->result->count >= collector->limit) {
        return false;
    }

    return true;
}

static bool count_record_from_tree(Record *record, void *context) {
    QueryCountContext *counter = (QueryCountContext *)context;

    if (counter->stats != NULL) {
        counter->stats->rows_scanned += 1;
    }

    if (predicate_matches_record(record, counter->predicate)) {
        counter->count += 1;
        if (counter->stats != NULL) {
            counter->stats->rows_returned += 1;
        }
    }

    return true;
}

static void init_default_query(QuerySpec *query) {
    if (query != NULL) {
        memset(query, 0, sizeof(*query));
    }
}

bool database_select(
    const Database *database,
    const QuerySpec *query,
    QueryResult *result,
    QueryPlan *plan,
    QueryExecutionStats *stats,
    char *error,
    size_t error_capacity
) {
    QuerySpec default_query;
    QueryPlan chosen_plan;

    if (database == NULL || result == NULL) {
        snprintf(error, error_capacity, "Database or result pointer was null.");
        return false;
    }

    init_default_query(&default_query);
    if (query == NULL) {
        query = &default_query;
    }

    memset(result, 0, sizeof(*result));
    memset(&chosen_plan, 0, sizeof(chosen_plan));
    if (stats != NULL) {
        memset(stats, 0, sizeof(*stats));
    }
    if (!planner_choose_plan(query, false, &chosen_plan, error, error_capacity)) {
        return false;
    }

    switch (chosen_plan.type) {
        case PLAN_HASH_ID_EXACT: {
            Record *record = id_index_find(&database->id_index, chosen_plan.driver_condition.int_value);
            if (record != NULL && stats != NULL) {
                stats->rows_scanned = 1;
            }
            if (record != NULL) {
                bool matches = predicate_matches_record(record, &query->predicate);
                if (matches && !query_result_append(result, record)) {
                    snprintf(error, error_capacity, "Failed to build query result.");
                    query_result_destroy(result);
                    return false;
                }
                if (matches && stats != NULL) {
                    stats->rows_returned = 1;
                }
            }
            break;
        }

        case PLAN_HASH_DEPARTMENT_EXACT: {
            const DepartmentIndexNode *entry = department_index_find(&database->department_index, chosen_plan.driver_condition.raw_value);
            const DepartmentRecordNode *node = entry == NULL ? NULL : entry->records;

            while (node != NULL) {
                bool matches;
                if (stats != NULL) {
                    stats->rows_scanned += 1;
                }
                matches = predicate_matches_record(node->record, &query->predicate);
                if (matches && !query_result_append(result, node->record)) {
                    snprintf(error, error_capacity, "Failed to build query result.");
                    query_result_destroy(result);
                    return false;
                }
                if (matches && stats != NULL) {
                    stats->rows_returned += 1;
                }
                node = node->next;
            }
            break;
        }

        case PLAN_BPTREE_ID_EXACT:
        case PLAN_BPTREE_AGE_EXACT: {
            const BPlusValueNode *value = chosen_plan.driver_field == FIELD_ID
                ? bptree_find(&database->id_tree, chosen_plan.range.lower)
                : bptree_find(&database->age_tree, chosen_plan.range.lower);

            while (value != NULL) {
                bool matches;
                if (stats != NULL) {
                    stats->rows_scanned += 1;
                }
                matches = predicate_matches_record(value->record, &query->predicate);
                if (matches && !query_result_append(result, value->record)) {
                    snprintf(error, error_capacity, "Failed to build query result.");
                    query_result_destroy(result);
                    return false;
                }
                if (matches && stats != NULL) {
                    stats->rows_returned += 1;
                }
                value = value->next;
            }
            break;
        }

        case PLAN_BPTREE_ID_RANGE:
        case PLAN_BPTREE_AGE_RANGE:
        case PLAN_BPTREE_ID_ORDERED:
        case PLAN_BPTREE_AGE_ORDERED: {
            QueryCollectContext collector;
            bool ok;

            memset(&collector, 0, sizeof(collector));
            collector.predicate = &query->predicate;
            collector.result = result;
            collector.stats = stats;
            collector.stop_at_limit = chosen_plan.limit_pushdown && query->has_limit;
            collector.limit = query->limit;
            collector.error = error;
            collector.error_capacity = error_capacity;

            if (chosen_plan.type == PLAN_BPTREE_ID_RANGE || chosen_plan.type == PLAN_BPTREE_ID_EXACT) {
                ok = bptree_range_scan(&database->id_tree, &chosen_plan.range, collect_record_from_tree, &collector, error, error_capacity);
            } else if (chosen_plan.type == PLAN_BPTREE_AGE_RANGE || chosen_plan.type == PLAN_BPTREE_AGE_EXACT) {
                ok = bptree_range_scan(&database->age_tree, &chosen_plan.range, collect_record_from_tree, &collector, error, error_capacity);
            } else if (chosen_plan.type == PLAN_BPTREE_ID_ORDERED) {
                ok = bptree_traverse_all(&database->id_tree, collect_record_from_tree, &collector, error, error_capacity);
            } else {
                ok = bptree_traverse_all(&database->age_tree, collect_record_from_tree, &collector, error, error_capacity);
            }

            if (!ok || collector.failed) {
                query_result_destroy(result);
                return false;
            }
            break;
        }

        case PLAN_FULL_SCAN:
        default: {
            size_t index;
            for (index = 0; index < database->count; ++index) {
                bool matches;
                if (stats != NULL) {
                    stats->rows_scanned += 1;
                }
                matches = predicate_matches_record(database->records[index], &query->predicate);
                if (matches && !query_result_append(result, database->records[index])) {
                    snprintf(error, error_capacity, "Failed to build query result.");
                    query_result_destroy(result);
                    return false;
                }
                if (matches && stats != NULL) {
                    stats->rows_returned += 1;
                }
            }
            break;
        }
    }

    if (chosen_plan.sort_required && query->order_by.active) {
        query_result_sort(result, query->order_by.field);
    }

    if (query->has_limit) {
        query_result_apply_limit(result, query->limit);
    }

    if (plan != NULL) {
        *plan = chosen_plan;
    }
    return true;
}

bool database_count_matching(
    const Database *database,
    const QuerySpec *query,
    size_t *count,
    QueryPlan *plan,
    QueryExecutionStats *stats,
    char *error,
    size_t error_capacity
) {
    QuerySpec default_query;
    QueryPlan chosen_plan;

    if (database == NULL || count == NULL) {
        snprintf(error, error_capacity, "Database or count pointer was null.");
        return false;
    }

    init_default_query(&default_query);
    if (query == NULL) {
        query = &default_query;
    }

    *count = 0;
    memset(&chosen_plan, 0, sizeof(chosen_plan));
    if (stats != NULL) {
        memset(stats, 0, sizeof(*stats));
    }
    if (!planner_choose_plan(query, true, &chosen_plan, error, error_capacity)) {
        return false;
    }

    switch (chosen_plan.type) {
        case PLAN_HASH_ID_EXACT: {
            Record *record = id_index_find(&database->id_index, chosen_plan.driver_condition.int_value);
            if (record != NULL && stats != NULL) {
                stats->rows_scanned = 1;
            }
            if (record != NULL && predicate_matches_record(record, &query->predicate)) {
                *count = 1;
                if (stats != NULL) {
                    stats->rows_returned = 1;
                }
            }
            break;
        }

        case PLAN_HASH_DEPARTMENT_EXACT: {
            const DepartmentIndexNode *entry = department_index_find(&database->department_index, chosen_plan.driver_condition.raw_value);
            const DepartmentRecordNode *node = entry == NULL ? NULL : entry->records;
            while (node != NULL) {
                if (stats != NULL) {
                    stats->rows_scanned += 1;
                }
                if (predicate_matches_record(node->record, &query->predicate)) {
                    *count += 1;
                    if (stats != NULL) {
                        stats->rows_returned += 1;
                    }
                }
                node = node->next;
            }
            break;
        }

        case PLAN_BPTREE_ID_EXACT:
        case PLAN_BPTREE_AGE_EXACT: {
            const BPlusValueNode *value = chosen_plan.driver_field == FIELD_ID
                ? bptree_find(&database->id_tree, chosen_plan.range.lower)
                : bptree_find(&database->age_tree, chosen_plan.range.lower);
            while (value != NULL) {
                if (stats != NULL) {
                    stats->rows_scanned += 1;
                }
                if (predicate_matches_record(value->record, &query->predicate)) {
                    *count += 1;
                    if (stats != NULL) {
                        stats->rows_returned += 1;
                    }
                }
                value = value->next;
            }
            break;
        }

        case PLAN_BPTREE_ID_RANGE:
        case PLAN_BPTREE_AGE_RANGE:
        case PLAN_BPTREE_ID_ORDERED:
        case PLAN_BPTREE_AGE_ORDERED: {
            QueryCountContext counter;
            bool ok;

            memset(&counter, 0, sizeof(counter));
            counter.predicate = &query->predicate;
            counter.stats = stats;

            if (chosen_plan.type == PLAN_BPTREE_ID_RANGE || chosen_plan.type == PLAN_BPTREE_ID_EXACT) {
                ok = bptree_range_scan(&database->id_tree, &chosen_plan.range, count_record_from_tree, &counter, error, error_capacity);
            } else if (chosen_plan.type == PLAN_BPTREE_AGE_RANGE || chosen_plan.type == PLAN_BPTREE_AGE_EXACT) {
                ok = bptree_range_scan(&database->age_tree, &chosen_plan.range, count_record_from_tree, &counter, error, error_capacity);
            } else if (chosen_plan.type == PLAN_BPTREE_ID_ORDERED) {
                ok = bptree_traverse_all(&database->id_tree, count_record_from_tree, &counter, error, error_capacity);
            } else {
                ok = bptree_traverse_all(&database->age_tree, count_record_from_tree, &counter, error, error_capacity);
            }

            if (!ok) {
                return false;
            }
            *count = counter.count;
            break;
        }

        case PLAN_FULL_SCAN:
        default: {
            size_t index;
            for (index = 0; index < database->count; ++index) {
                if (stats != NULL) {
                    stats->rows_scanned += 1;
                }
                if (predicate_matches_record(database->records[index], &query->predicate)) {
                    *count += 1;
                    if (stats != NULL) {
                        stats->rows_returned += 1;
                    }
                }
            }
            break;
        }
    }

    if (plan != NULL) {
        *plan = chosen_plan;
    }
    return true;
}

void query_result_destroy(QueryResult *result) {
    if (result == NULL) {
        return;
    }

    free(result->rows);
    result->rows = NULL;
    result->count = 0;
    result->capacity = 0;
}

size_t database_estimate_memory_usage(const Database *database) {
    if (database == NULL) {
        return 0;
    }

    return sizeof(*database) +
           (database->capacity * sizeof(Record *)) +
           (database->count * sizeof(Record)) +
           id_index_estimate_memory(&database->id_index) +
           department_index_estimate_memory(&database->department_index) +
           bptree_estimate_memory(&database->id_tree) +
           bptree_estimate_memory(&database->age_tree);
}

const char *field_to_string(Field field) {
    switch (field) {
        case FIELD_ID:
            return "id";
        case FIELD_NAME:
            return "name";
        case FIELD_AGE:
            return "age";
        case FIELD_DEPARTMENT:
            return "department";
        default:
            return "unknown";
    }
}
