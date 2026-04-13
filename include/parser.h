#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include <stddef.h>

#include "database.h"

#define COMMAND_MAX_ASSIGNMENTS 4

typedef enum CommandType {
    CMD_INVALID = 0,
    CMD_INSERT,
    CMD_SELECT,
    CMD_UPDATE,
    CMD_DELETE,
    CMD_SAVE,
    CMD_LOAD,
    CMD_COUNT,
    CMD_HISTORY,
    CMD_BENCHMARK,
    CMD_HELP,
    CMD_EXIT
} CommandType;

typedef struct Command {
    CommandType type;
    Record record;
    QuerySpec query;
    int target_id;
    bool has_target_id;
    UpdateAssignment assignments[COMMAND_MAX_ASSIGNMENTS];
    size_t assignment_count;
    bool has_path;
    char path[DB_MAX_PATH_LEN];
    size_t benchmark_record_count;
} Command;

bool parse_command(const char *input, Command *command, char *error, size_t error_capacity);
const char *command_type_to_string(CommandType type);

#endif
