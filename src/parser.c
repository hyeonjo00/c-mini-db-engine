#include "parser.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef struct TokenList {
    char items[48][DB_MAX_COMMAND_LEN];
    size_t count;
} TokenList;

static void set_error(char *error, size_t error_capacity, const char *message) {
    if (error != NULL && error_capacity > 0) {
        snprintf(error, error_capacity, "%s", message);
    }
}

static bool parse_field(const char *text, Field *field) {
    if (string_ieq(text, "id")) {
        *field = FIELD_ID;
        return true;
    }
    if (string_ieq(text, "name")) {
        *field = FIELD_NAME;
        return true;
    }
    if (string_ieq(text, "age")) {
        *field = FIELD_AGE;
        return true;
    }
    if (string_ieq(text, "department")) {
        *field = FIELD_DEPARTMENT;
        return true;
    }
    *field = FIELD_NONE;
    return false;
}

static bool parse_compare_operator(const char *text, CompareOperator *op) {
    if (strcmp(text, "=") == 0) {
        *op = CMP_EQ;
        return true;
    }
    if (strcmp(text, "!=") == 0) {
        *op = CMP_NEQ;
        return true;
    }
    if (strcmp(text, ">") == 0) {
        *op = CMP_GT;
        return true;
    }
    if (strcmp(text, ">=") == 0) {
        *op = CMP_GTE;
        return true;
    }
    if (strcmp(text, "<") == 0) {
        *op = CMP_LT;
        return true;
    }
    if (strcmp(text, "<=") == 0) {
        *op = CMP_LTE;
        return true;
    }
    *op = CMP_NONE;
    return false;
}

static bool tokenize(const char *input, TokenList *tokens, char *error, size_t error_capacity) {
    char current[DB_MAX_COMMAND_LEN];
    size_t current_length = 0;
    bool in_quote = false;
    const char *cursor;

    if (input == NULL || tokens == NULL) {
        set_error(error, error_capacity, "Tokenizer received a null input.");
        return false;
    }

    memset(tokens, 0, sizeof(*tokens));
    memset(current, 0, sizeof(current));

    for (cursor = input; ; ++cursor) {
        char ch = *cursor;
        bool at_end = ch == '\0';
        bool is_boundary = !in_quote && (at_end || isspace((unsigned char)ch));

        if (ch == '"') {
            in_quote = !in_quote;
            continue;
        }

        if (is_boundary) {
            if (current_length > 0) {
                if (tokens->count >= (sizeof(tokens->items) / sizeof(tokens->items[0]))) {
                    set_error(error, error_capacity, "Too many tokens in command.");
                    return false;
                }
                current[current_length] = '\0';
                if (!safe_copy(tokens->items[tokens->count], sizeof(tokens->items[tokens->count]), current)) {
                    set_error(error, error_capacity, "Token exceeded maximum size.");
                    return false;
                }
                tokens->count += 1;
                current_length = 0;
                current[0] = '\0';
            }

            if (at_end) {
                break;
            }
            continue;
        }

        if (current_length + 1 >= sizeof(current)) {
            set_error(error, error_capacity, "Command exceeded maximum length.");
            return false;
        }

        current[current_length++] = ch;
    }

    if (in_quote) {
        set_error(error, error_capacity, "Unclosed quote in command.");
        return false;
    }

    return true;
}

static bool split_expression_token(
    const char *token,
    char *field_buffer,
    size_t field_capacity,
    char *operator_buffer,
    size_t operator_capacity,
    char *value_buffer,
    size_t value_capacity
) {
    static const char *operators[] = {">=", "<=", "!=", "=", ">", "<"};
    size_t index;

    if (token == NULL || field_buffer == NULL || operator_buffer == NULL || value_buffer == NULL) {
        return false;
    }

    for (index = 0; index < sizeof(operators) / sizeof(operators[0]); ++index) {
        const char *match = strstr(token, operators[index]);
        if (match != NULL) {
            size_t field_length = (size_t)(match - token);
            const char *value = match + strlen(operators[index]);

            if (field_length == 0 || *value == '\0') {
                return false;
            }
            if (field_length >= field_capacity || strlen(value) >= value_capacity || strlen(operators[index]) >= operator_capacity) {
                return false;
            }

            memcpy(field_buffer, token, field_length);
            field_buffer[field_length] = '\0';
            strcpy(operator_buffer, operators[index]);
            strcpy(value_buffer, value);
            return true;
        }
    }

    return false;
}

static bool assignment_from_token(const char *token, UpdateAssignment *assignment, char *error, size_t error_capacity) {
    char field_text[DB_MAX_VALUE_LEN];
    char op_text[4];
    char value_text[DB_MAX_COMMAND_LEN];

    if (!split_expression_token(token, field_text, sizeof(field_text), op_text, sizeof(op_text), value_text, sizeof(value_text))) {
        set_error(error, error_capacity, "Assignments must use field=value syntax.");
        return false;
    }

    if (strcmp(op_text, "=") != 0) {
        set_error(error, error_capacity, "Assignments must use '='.");
        return false;
    }

    if (!parse_field(field_text, &assignment->field)) {
        snprintf(error, error_capacity, "Unknown field in assignment: %s", field_text);
        return false;
    }

    memset(assignment->raw_value, 0, sizeof(assignment->raw_value));
    assignment->int_value = 0;
    assignment->value_is_int = false;

    if (assignment->field == FIELD_ID || assignment->field == FIELD_AGE) {
        if (!parse_int_strict(value_text, &assignment->int_value)) {
            snprintf(error, error_capacity, "Expected integer value for %s.", field_text);
            return false;
        }
        assignment->value_is_int = true;
        snprintf(assignment->raw_value, sizeof(assignment->raw_value), "%d", assignment->int_value);
    } else if (!safe_copy(assignment->raw_value, sizeof(assignment->raw_value), value_text)) {
        snprintf(error, error_capacity, "Value for %s is too long.", field_text);
        return false;
    }

    return true;
}

static bool condition_from_parts(
    const char *field_text,
    const char *operator_text,
    const char *value_text,
    Condition *condition,
    char *error,
    size_t error_capacity
) {
    if (!parse_field(field_text, &condition->field)) {
        snprintf(error, error_capacity, "Unknown field in WHERE clause: %s", field_text);
        return false;
    }

    if (!parse_compare_operator(operator_text, &condition->op)) {
        snprintf(error, error_capacity, "Unsupported operator in WHERE clause: %s", operator_text);
        return false;
    }

    if ((condition->field == FIELD_NAME || condition->field == FIELD_DEPARTMENT) &&
        !(condition->op == CMP_EQ || condition->op == CMP_NEQ)) {
        snprintf(error, error_capacity, "String fields only support '=' and '!='.");
        return false;
    }

    condition->active = true;
    condition->value_is_int = false;
    condition->int_value = 0;

    if (condition->field == FIELD_ID || condition->field == FIELD_AGE) {
        if (!parse_int_strict(value_text, &condition->int_value)) {
            snprintf(error, error_capacity, "Expected integer value for field %s.", field_text);
            return false;
        }
        condition->value_is_int = true;
        snprintf(condition->raw_value, sizeof(condition->raw_value), "%d", condition->int_value);
    } else if (!safe_copy(condition->raw_value, sizeof(condition->raw_value), value_text)) {
        snprintf(error, error_capacity, "WHERE value is too long for field %s.", field_text);
        return false;
    }

    return true;
}

static bool parse_condition(const TokenList *tokens, size_t start_index, Condition *condition, size_t *next_index, char *error, size_t error_capacity) {
    char field_text[DB_MAX_VALUE_LEN];
    char op_text[4];
    char value_text[DB_MAX_COMMAND_LEN];

    if (start_index >= tokens->count) {
        set_error(error, error_capacity, "WHERE clause is incomplete.");
        return false;
    }

    memset(condition, 0, sizeof(*condition));
    if (split_expression_token(tokens->items[start_index], field_text, sizeof(field_text), op_text, sizeof(op_text), value_text, sizeof(value_text))) {
        if (!condition_from_parts(field_text, op_text, value_text, condition, error, error_capacity)) {
            return false;
        }
        *next_index = start_index + 1;
        return true;
    }

    if (start_index + 2 >= tokens->count) {
        set_error(error, error_capacity, "WHERE clause must look like field operator value.");
        return false;
    }

    if (!condition_from_parts(tokens->items[start_index], tokens->items[start_index + 1], tokens->items[start_index + 2], condition, error, error_capacity)) {
        return false;
    }

    *next_index = start_index + 3;
    return true;
}

static bool is_select_clause_keyword(const char *token) {
    return string_ieq(token, "ORDER") || string_ieq(token, "LIMIT");
}

static bool parse_predicate(const TokenList *tokens, size_t start_index, Predicate *predicate, size_t *next_index, char *error, size_t error_capacity) {
    size_t current = start_index;

    if (predicate == NULL) {
        set_error(error, error_capacity, "Predicate pointer was null.");
        return false;
    }

    memset(predicate, 0, sizeof(*predicate));
    if (current >= tokens->count) {
        set_error(error, error_capacity, "WHERE clause is incomplete.");
        return false;
    }

    while (current < tokens->count) {
        size_t condition_end;

        if (is_select_clause_keyword(tokens->items[current])) {
            break;
        }

        if (predicate->condition_count >= DB_MAX_PREDICATES) {
            snprintf(error, error_capacity, "WHERE supports at most %d conditions.", DB_MAX_PREDICATES);
            return false;
        }

        if (!parse_condition(tokens, current, &predicate->conditions[predicate->condition_count], &condition_end, error, error_capacity)) {
            return false;
        }
        predicate->condition_count += 1;
        current = condition_end;

        if (current >= tokens->count || is_select_clause_keyword(tokens->items[current])) {
            break;
        }

        if (string_ieq(tokens->items[current], "AND")) {
            predicate->operators[predicate->condition_count - 1] = LOGIC_AND;
            current += 1;
            continue;
        }
        if (string_ieq(tokens->items[current], "OR")) {
            predicate->operators[predicate->condition_count - 1] = LOGIC_OR;
            current += 1;
            continue;
        }

        set_error(error, error_capacity, "Expected AND, OR, ORDER, or LIMIT after condition.");
        return false;
    }

    if (predicate->condition_count == 0) {
        set_error(error, error_capacity, "WHERE clause is incomplete.");
        return false;
    }

    *next_index = current;
    return true;
}

static bool parse_order_by(const TokenList *tokens, size_t start_index, QuerySpec *query, size_t *next_index, char *error, size_t error_capacity) {
    Field field;

    if (start_index + 2 >= tokens->count) {
        set_error(error, error_capacity, "ORDER BY clause is incomplete.");
        return false;
    }

    if (!string_ieq(tokens->items[start_index], "ORDER") || !string_ieq(tokens->items[start_index + 1], "BY")) {
        set_error(error, error_capacity, "Expected ORDER BY.");
        return false;
    }

    if (!parse_field(tokens->items[start_index + 2], &field)) {
        snprintf(error, error_capacity, "Unsupported ORDER BY field: %s", tokens->items[start_index + 2]);
        return false;
    }

    query->order_by.active = true;
    query->order_by.field = field;
    *next_index = start_index + 3;
    return true;
}

static bool parse_limit(const TokenList *tokens, size_t start_index, QuerySpec *query, size_t *next_index, char *error, size_t error_capacity) {
    int parsed_limit;

    if (start_index + 1 >= tokens->count) {
        set_error(error, error_capacity, "LIMIT clause is incomplete.");
        return false;
    }

    if (!string_ieq(tokens->items[start_index], "LIMIT")) {
        set_error(error, error_capacity, "Expected LIMIT clause.");
        return false;
    }

    if (!parse_int_strict(tokens->items[start_index + 1], &parsed_limit) || parsed_limit <= 0) {
        set_error(error, error_capacity, "LIMIT must be a positive integer.");
        return false;
    }

    query->has_limit = true;
    query->limit = (size_t)parsed_limit;
    *next_index = start_index + 2;
    return true;
}

static bool parse_insert(const TokenList *tokens, Command *command, char *error, size_t error_capacity) {
    if (tokens->count != 5) {
        set_error(error, error_capacity, "INSERT expects: INSERT <id> <name> <age> <department>");
        return false;
    }

    if (!parse_int_strict(tokens->items[1], &command->record.id)) {
        set_error(error, error_capacity, "INSERT id must be an integer.");
        return false;
    }
    if (!safe_copy(command->record.name, sizeof(command->record.name), tokens->items[2])) {
        snprintf(error, error_capacity, "name exceeds %d characters.", DB_MAX_NAME_LEN - 1);
        return false;
    }
    if (!parse_int_strict(tokens->items[3], &command->record.age)) {
        set_error(error, error_capacity, "INSERT age must be an integer.");
        return false;
    }
    if (!safe_copy(command->record.department, sizeof(command->record.department), tokens->items[4])) {
        snprintf(error, error_capacity, "department exceeds %d characters.", DB_MAX_DEPARTMENT_LEN - 1);
        return false;
    }

    return true;
}

static bool parse_select(const TokenList *tokens, Command *command, char *error, size_t error_capacity) {
    size_t index = 1;
    bool saw_where = false;
    bool saw_order = false;
    bool saw_limit = false;

    if (tokens->count == 1) {
        return true;
    }

    if (strcmp(tokens->items[index], "*") == 0) {
        index += 1;
    }

    while (index < tokens->count) {
        if (string_ieq(tokens->items[index], "WHERE")) {
            if (saw_where) {
                set_error(error, error_capacity, "SELECT accepts only one WHERE clause.");
                return false;
            }
            if (!parse_predicate(tokens, index + 1, &command->query.predicate, &index, error, error_capacity)) {
                return false;
            }
            saw_where = true;
            continue;
        }

        if (string_ieq(tokens->items[index], "ORDER")) {
            if (saw_order) {
                set_error(error, error_capacity, "SELECT accepts only one ORDER BY clause.");
                return false;
            }
            if (!parse_order_by(tokens, index, &command->query, &index, error, error_capacity)) {
                return false;
            }
            saw_order = true;
            continue;
        }

        if (string_ieq(tokens->items[index], "LIMIT")) {
            if (saw_limit) {
                set_error(error, error_capacity, "SELECT accepts only one LIMIT clause.");
                return false;
            }
            if (!parse_limit(tokens, index, &command->query, &index, error, error_capacity)) {
                return false;
            }
            saw_limit = true;
            continue;
        }

        snprintf(error, error_capacity, "Unexpected token in SELECT: %s", tokens->items[index]);
        return false;
    }

    return true;
}

static bool parse_count(const TokenList *tokens, Command *command, char *error, size_t error_capacity) {
    size_t next_index = 0;

    if (tokens->count == 1) {
        return true;
    }

    if (!string_ieq(tokens->items[1], "WHERE")) {
        set_error(error, error_capacity, "COUNT optionally supports 'WHERE ...'.");
        return false;
    }

    if (!parse_predicate(tokens, 2, &command->query.predicate, &next_index, error, error_capacity)) {
        return false;
    }

    if (next_index != tokens->count) {
        set_error(error, error_capacity, "COUNT does not support ORDER BY or LIMIT.");
        return false;
    }

    return true;
}

static bool parse_update(const TokenList *tokens, Command *command, char *error, size_t error_capacity) {
    size_t index;
    UpdateAssignment selector;

    if (tokens->count < 3) {
        set_error(error, error_capacity, "UPDATE expects: UPDATE id=<target> field=value ...");
        return false;
    }

    if (!assignment_from_token(tokens->items[1], &selector, error, error_capacity)) {
        return false;
    }

    if (selector.field != FIELD_ID || !selector.value_is_int) {
        set_error(error, error_capacity, "UPDATE selector must be id=<target>.");
        return false;
    }

    command->target_id = selector.int_value;
    command->has_target_id = true;

    for (index = 2; index < tokens->count; ++index) {
        if (command->assignment_count >= COMMAND_MAX_ASSIGNMENTS) {
            snprintf(error, error_capacity, "UPDATE supports at most %d assignments.", COMMAND_MAX_ASSIGNMENTS);
            return false;
        }
        if (!assignment_from_token(tokens->items[index], &command->assignments[command->assignment_count], error, error_capacity)) {
            return false;
        }
        command->assignment_count += 1;
    }

    if (command->assignment_count == 0) {
        set_error(error, error_capacity, "UPDATE needs at least one field assignment.");
        return false;
    }

    return true;
}

static bool parse_delete(const TokenList *tokens, Command *command, char *error, size_t error_capacity) {
    UpdateAssignment selector;
    Condition condition;
    size_t next_index = 0;

    if (tokens->count < 2) {
        set_error(error, error_capacity, "DELETE expects id=<target>.");
        return false;
    }

    if (string_ieq(tokens->items[1], "WHERE")) {
        if (!parse_condition(tokens, 2, &condition, &next_index, error, error_capacity)) {
            return false;
        }
        if (next_index != tokens->count) {
            set_error(error, error_capacity, "DELETE only supports WHERE id = <value>.");
            return false;
        }
        if (condition.field != FIELD_ID || condition.op != CMP_EQ) {
            set_error(error, error_capacity, "DELETE only supports WHERE id = <value>.");
            return false;
        }
        command->target_id = condition.int_value;
        command->has_target_id = true;
        return true;
    }

    if (!assignment_from_token(tokens->items[1], &selector, error, error_capacity)) {
        return false;
    }
    if (selector.field != FIELD_ID || !selector.value_is_int) {
        set_error(error, error_capacity, "DELETE selector must be id=<target>.");
        return false;
    }
    if (tokens->count != 2) {
        set_error(error, error_capacity, "DELETE only supports a single id selector.");
        return false;
    }

    command->target_id = selector.int_value;
    command->has_target_id = true;
    return true;
}

static bool parse_path_command(const TokenList *tokens, Command *command, char *error, size_t error_capacity) {
    if (tokens->count == 1) {
        return true;
    }

    if (tokens->count != 2) {
        set_error(error, error_capacity, "Command accepts at most one optional path.");
        return false;
    }

    if (!safe_copy(command->path, sizeof(command->path), tokens->items[1])) {
        set_error(error, error_capacity, "Path exceeded maximum size.");
        return false;
    }

    command->has_path = true;
    return true;
}

bool parse_command(const char *input, Command *command, char *error, size_t error_capacity) {
    TokenList tokens;

    if (command == NULL) {
        set_error(error, error_capacity, "Command pointer was null.");
        return false;
    }

    memset(command, 0, sizeof(*command));
    if (!tokenize(input, &tokens, error, error_capacity)) {
        return false;
    }

    if (tokens.count == 0) {
        set_error(error, error_capacity, "Empty command.");
        return false;
    }

    if (string_ieq(tokens.items[0], "INSERT")) {
        command->type = CMD_INSERT;
        return parse_insert(&tokens, command, error, error_capacity);
    }
    if (string_ieq(tokens.items[0], "SELECT")) {
        command->type = CMD_SELECT;
        return parse_select(&tokens, command, error, error_capacity);
    }
    if (string_ieq(tokens.items[0], "COUNT")) {
        command->type = CMD_COUNT;
        return parse_count(&tokens, command, error, error_capacity);
    }
    if (string_ieq(tokens.items[0], "UPDATE")) {
        command->type = CMD_UPDATE;
        return parse_update(&tokens, command, error, error_capacity);
    }
    if (string_ieq(tokens.items[0], "DELETE")) {
        command->type = CMD_DELETE;
        return parse_delete(&tokens, command, error, error_capacity);
    }
    if (string_ieq(tokens.items[0], "SAVE")) {
        command->type = CMD_SAVE;
        return parse_path_command(&tokens, command, error, error_capacity);
    }
    if (string_ieq(tokens.items[0], "LOAD")) {
        command->type = CMD_LOAD;
        return parse_path_command(&tokens, command, error, error_capacity);
    }
    if (string_ieq(tokens.items[0], "HISTORY")) {
        if (tokens.count != 1) {
            set_error(error, error_capacity, "HISTORY does not take arguments.");
            return false;
        }
        command->type = CMD_HISTORY;
        return true;
    }
    if (string_ieq(tokens.items[0], "HELP")) {
        if (tokens.count != 1) {
            set_error(error, error_capacity, "HELP does not take arguments.");
            return false;
        }
        command->type = CMD_HELP;
        return true;
    }
    if (string_ieq(tokens.items[0], "EXIT")) {
        if (tokens.count != 1) {
            set_error(error, error_capacity, "EXIT does not take arguments.");
            return false;
        }
        command->type = CMD_EXIT;
        return true;
    }
    if (string_ieq(tokens.items[0], "BENCHMARK")) {
        command->type = CMD_BENCHMARK;
        command->benchmark_record_count = 10000;
        if (tokens.count == 1) {
            return true;
        }
        if (tokens.count != 2) {
            set_error(error, error_capacity, "BENCHMARK accepts an optional record count.");
            return false;
        }
        {
            int parsed_count;
            if (!parse_int_strict(tokens.items[1], &parsed_count) || parsed_count <= 0) {
                set_error(error, error_capacity, "BENCHMARK record count must be a positive integer.");
                return false;
            }
            command->benchmark_record_count = (size_t)parsed_count;
        }
        return true;
    }

    snprintf(error, error_capacity, "Unknown command: %s", tokens.items[0]);
    return false;
}

const char *command_type_to_string(CommandType type) {
    switch (type) {
        case CMD_INSERT:
            return "INSERT";
        case CMD_SELECT:
            return "SELECT";
        case CMD_UPDATE:
            return "UPDATE";
        case CMD_DELETE:
            return "DELETE";
        case CMD_SAVE:
            return "SAVE";
        case CMD_LOAD:
            return "LOAD";
        case CMD_COUNT:
            return "COUNT";
        case CMD_HISTORY:
            return "HISTORY";
        case CMD_BENCHMARK:
            return "BENCHMARK";
        case CMD_HELP:
            return "HELP";
        case CMD_EXIT:
            return "EXIT";
        default:
            return "INVALID";
    }
}
