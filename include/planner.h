#ifndef PLANNER_H
#define PLANNER_H

#include <stdbool.h>

#include "database.h"

bool planner_choose_plan(const QuerySpec *query, bool is_count, QueryPlan *plan, char *error, size_t error_capacity);
const char *plan_type_to_string(QueryPlanType type);

#endif
