#include "planner.h"

#include <stdio.h>
#include <string.h>

static bool predicate_has_or(const Predicate *predicate) {
    size_t index;

    if (predicate == NULL) {
        return false;
    }

    for (index = 0; index + 1 < predicate->condition_count; ++index) {
        if (predicate->operators[index] == LOGIC_OR) {
            return true;
        }
    }

    return false;
}

static bool find_exact_condition(const Predicate *predicate, Field field, Condition *match) {
    size_t index;

    if (predicate == NULL) {
        return false;
    }

    for (index = 0; index < predicate->condition_count; ++index) {
        if (predicate->conditions[index].field == field && predicate->conditions[index].op == CMP_EQ) {
            if (match != NULL) {
                *match = predicate->conditions[index];
            }
            return true;
        }
    }

    return false;
}

static void apply_lower_bound(BPlusRange *range, int value, bool inclusive) {
    if (!range->has_lower ||
        value > range->lower ||
        (value == range->lower && !inclusive && range->lower_inclusive)) {
        range->has_lower = true;
        range->lower = value;
        range->lower_inclusive = inclusive;
    }
}

static void apply_upper_bound(BPlusRange *range, int value, bool inclusive) {
    if (!range->has_upper ||
        value < range->upper ||
        (value == range->upper && !inclusive && range->upper_inclusive)) {
        range->has_upper = true;
        range->upper = value;
        range->upper_inclusive = inclusive;
    }
}

static bool extract_numeric_range(const Predicate *predicate, Field field, BPlusRange *range, bool *found) {
    size_t index;

    if (predicate == NULL || range == NULL || found == NULL) {
        return false;
    }

    memset(range, 0, sizeof(*range));
    *found = false;

    for (index = 0; index < predicate->condition_count; ++index) {
        const Condition *condition = &predicate->conditions[index];
        if (condition->field != field || !condition->value_is_int) {
            continue;
        }

        switch (condition->op) {
            case CMP_EQ:
                *found = true;
                range->has_lower = true;
                range->lower = condition->int_value;
                range->lower_inclusive = true;
                range->has_upper = true;
                range->upper = condition->int_value;
                range->upper_inclusive = true;
                break;
            case CMP_GT:
                *found = true;
                apply_lower_bound(range, condition->int_value, false);
                break;
            case CMP_GTE:
                *found = true;
                apply_lower_bound(range, condition->int_value, true);
                break;
            case CMP_LT:
                *found = true;
                apply_upper_bound(range, condition->int_value, false);
                break;
            case CMP_LTE:
                *found = true;
                apply_upper_bound(range, condition->int_value, true);
                break;
            default:
                break;
        }
    }

    return true;
}

static bool range_is_exact(const BPlusRange *range) {
    return range != NULL &&
           range->has_lower &&
           range->has_upper &&
           range->lower == range->upper &&
           range->lower_inclusive &&
           range->upper_inclusive;
}

static void set_plan(
    QueryPlan *plan,
    QueryPlanType type,
    Field driver_field,
    bool uses_index,
    bool produces_ordered_rows,
    const char *description
) {
    memset(plan, 0, sizeof(*plan));
    plan->type = type;
    plan->driver_field = driver_field;
    plan->uses_index = uses_index;
    plan->produces_ordered_rows = produces_ordered_rows;
    safe_copy(plan->description, sizeof(plan->description), description);
}

static void finalize_plan(const QuerySpec *query, bool is_count, QueryPlan *plan) {
    char description[DB_MAX_PLAN_DESCRIPTION];

    if (query != NULL && !is_count && query->order_by.active) {
        if (plan->type == PLAN_HASH_ID_EXACT || plan->type == PLAN_BPTREE_ID_EXACT) {
            plan->sort_required = false;
        } else if (plan->produces_ordered_rows && plan->driver_field == query->order_by.field) {
            plan->sort_required = false;
        } else {
            plan->sort_required = true;
        }
    }

    if (query != NULL && !is_count && query->has_limit && plan->produces_ordered_rows) {
        plan->limit_pushdown = true;
    }

    safe_copy(description, sizeof(description), plan->description);
    if (plan->sort_required) {
        snprintf(plan->description, sizeof(plan->description), "%s + In-Memory Sort", description);
    }
}

bool planner_choose_plan(const QuerySpec *query, bool is_count, QueryPlan *plan, char *error, size_t error_capacity) {
    bool has_or;
    Condition exact_match;
    BPlusRange age_range;
    BPlusRange id_range;
    bool has_age_range = false;
    bool has_id_range = false;

    if (query == NULL || plan == NULL) {
        snprintf(error, error_capacity, "Planner received a null query or plan.");
        return false;
    }

    has_or = predicate_has_or(&query->predicate);
    memset(&exact_match, 0, sizeof(exact_match));
    memset(&age_range, 0, sizeof(age_range));
    memset(&id_range, 0, sizeof(id_range));

    if (has_or) {
        set_plan(plan, PLAN_FULL_SCAN, FIELD_NONE, false, false, "Full Table Scan");
        finalize_plan(query, is_count, plan);
        return true;
    }

    if (find_exact_condition(&query->predicate, FIELD_ID, &exact_match)) {
        set_plan(plan, PLAN_HASH_ID_EXACT, FIELD_ID, true, true, "Hash Index Exact Lookup (id)");
        plan->has_driver_condition = true;
        plan->driver_condition = exact_match;
        finalize_plan(query, is_count, plan);
        return true;
    }

    {
        extract_numeric_range(&query->predicate, FIELD_AGE, &age_range, &has_age_range);
        if (has_age_range) {
            if (range_is_exact(&age_range)) {
                set_plan(plan, PLAN_BPTREE_AGE_EXACT, FIELD_AGE, true, true, "B+ Tree Exact Search (age)");
            } else {
                set_plan(plan, PLAN_BPTREE_AGE_RANGE, FIELD_AGE, true, true, "B+ Tree Range Scan (age)");
            }
            plan->range = age_range;
            finalize_plan(query, is_count, plan);
            return true;
        }

        extract_numeric_range(&query->predicate, FIELD_ID, &id_range, &has_id_range);
        if (has_id_range) {
            if (range_is_exact(&id_range)) {
                set_plan(plan, PLAN_BPTREE_ID_EXACT, FIELD_ID, true, true, "B+ Tree Exact Search (id)");
            } else {
                set_plan(plan, PLAN_BPTREE_ID_RANGE, FIELD_ID, true, true, "B+ Tree Range Scan (id)");
            }
            plan->range = id_range;
            finalize_plan(query, is_count, plan);
            return true;
        }

        if (find_exact_condition(&query->predicate, FIELD_DEPARTMENT, &exact_match)) {
            set_plan(plan, PLAN_HASH_DEPARTMENT_EXACT, FIELD_DEPARTMENT, true, false, "Hash Index Exact Lookup (department)");
            plan->has_driver_condition = true;
            plan->driver_condition = exact_match;
            finalize_plan(query, is_count, plan);
            return true;
        }
    }

    if (!is_count && query->order_by.active) {
        if (query->order_by.field == FIELD_AGE) {
            set_plan(plan, PLAN_BPTREE_AGE_ORDERED, FIELD_AGE, true, true, "B+ Tree Ordered Traversal (age)");
            finalize_plan(query, is_count, plan);
            return true;
        }
        if (query->order_by.field == FIELD_ID) {
            set_plan(plan, PLAN_BPTREE_ID_ORDERED, FIELD_ID, true, true, "B+ Tree Ordered Traversal (id)");
            finalize_plan(query, is_count, plan);
            return true;
        }
    }

    set_plan(plan, PLAN_FULL_SCAN, FIELD_NONE, false, false, "Full Table Scan");
    finalize_plan(query, is_count, plan);
    return true;
}

const char *plan_type_to_string(QueryPlanType type) {
    switch (type) {
        case PLAN_HASH_ID_EXACT:
            return "HASH_ID_EXACT";
        case PLAN_HASH_DEPARTMENT_EXACT:
            return "HASH_DEPARTMENT_EXACT";
        case PLAN_BPTREE_ID_EXACT:
            return "BPTREE_ID_EXACT";
        case PLAN_BPTREE_ID_RANGE:
            return "BPTREE_ID_RANGE";
        case PLAN_BPTREE_AGE_EXACT:
            return "BPTREE_AGE_EXACT";
        case PLAN_BPTREE_AGE_RANGE:
            return "BPTREE_AGE_RANGE";
        case PLAN_BPTREE_ID_ORDERED:
            return "BPTREE_ID_ORDERED";
        case PLAN_BPTREE_AGE_ORDERED:
            return "BPTREE_AGE_ORDERED";
        case PLAN_FULL_SCAN:
        default:
            return "FULL_SCAN";
    }
}
