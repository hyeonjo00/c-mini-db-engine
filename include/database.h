#ifndef DATABASE_H
#define DATABASE_H

#include <stdbool.h>
#include <stddef.h>

#include "bptree.h"
#include "common.h"
#include "index.h"

typedef enum Field {
    FIELD_NONE = 0,
    FIELD_ID,
    FIELD_NAME,
    FIELD_AGE,
    FIELD_DEPARTMENT
} Field;

typedef enum CompareOperator {
    CMP_NONE = 0,
    CMP_EQ,
    CMP_NEQ,
    CMP_GT,
    CMP_GTE,
    CMP_LT,
    CMP_LTE
} CompareOperator;

typedef enum LogicalOperator {
    LOGIC_NONE = 0,
    LOGIC_AND,
    LOGIC_OR
} LogicalOperator;

typedef struct Record {
    int id;
    char name[DB_MAX_NAME_LEN];
    int age;
    char department[DB_MAX_DEPARTMENT_LEN];
    size_t slot;
} Record;

typedef struct Condition {
    bool active;
    Field field;
    CompareOperator op;
    char raw_value[DB_MAX_VALUE_LEN];
    int int_value;
    bool value_is_int;
} Condition;

typedef struct Predicate {
    Condition conditions[DB_MAX_PREDICATES];
    LogicalOperator operators[DB_MAX_PREDICATES - 1];
    size_t condition_count;
} Predicate;

typedef struct UpdateAssignment {
    Field field;
    char raw_value[DB_MAX_VALUE_LEN];
    int int_value;
    bool value_is_int;
} UpdateAssignment;

typedef struct QueryResult {
    Record **rows;
    size_t count;
    size_t capacity;
} QueryResult;

typedef struct OrderBy {
    bool active;
    Field field;
} OrderBy;

typedef struct QuerySpec {
    Predicate predicate;
    OrderBy order_by;
    bool has_limit;
    size_t limit;
} QuerySpec;

typedef enum QueryPlanType {
    PLAN_FULL_SCAN = 0,
    PLAN_HASH_ID_EXACT,
    PLAN_HASH_DEPARTMENT_EXACT,
    PLAN_BPTREE_ID_EXACT,
    PLAN_BPTREE_ID_RANGE,
    PLAN_BPTREE_AGE_EXACT,
    PLAN_BPTREE_AGE_RANGE,
    PLAN_BPTREE_ID_ORDERED,
    PLAN_BPTREE_AGE_ORDERED
} QueryPlanType;

typedef struct QueryPlan {
    QueryPlanType type;
    bool uses_index;
    bool produces_ordered_rows;
    bool sort_required;
    bool limit_pushdown;
    Field driver_field;
    bool has_driver_condition;
    Condition driver_condition;
    BPlusRange range;
    char description[DB_MAX_PLAN_DESCRIPTION];
} QueryPlan;

typedef struct QueryExecutionStats {
    size_t rows_scanned;
    size_t rows_returned;
} QueryExecutionStats;

typedef struct Database {
    Record **records;
    size_t count;
    size_t capacity;
    IdIndex id_index;
    DepartmentIndex department_index;
    BPlusTree id_tree;
    BPlusTree age_tree;
} Database;

bool database_init(Database *database);
void database_destroy(Database *database);
bool database_clear(Database *database, char *error, size_t error_capacity);

bool database_insert_record(Database *database, const Record *record, char *error, size_t error_capacity);
bool database_delete_by_id(Database *database, int id, char *error, size_t error_capacity);
bool database_update_by_id(
    Database *database,
    int id,
    const UpdateAssignment *assignments,
    size_t assignment_count,
    char *error,
    size_t error_capacity
);

bool database_select(
    const Database *database,
    const QuerySpec *query,
    QueryResult *result,
    QueryPlan *plan,
    QueryExecutionStats *stats,
    char *error,
    size_t error_capacity
);

bool database_count_matching(
    const Database *database,
    const QuerySpec *query,
    size_t *count,
    QueryPlan *plan,
    QueryExecutionStats *stats,
    char *error,
    size_t error_capacity
);

bool predicate_matches_record(const Record *record, const Predicate *predicate);
bool predicate_is_empty(const Predicate *predicate);
size_t database_estimate_memory_usage(const Database *database);
void query_result_destroy(QueryResult *result);
const Record *database_find_by_id(const Database *database, int id);
bool database_condition_matches(const Record *record, const Condition *condition);
const char *field_to_string(Field field);

#endif
