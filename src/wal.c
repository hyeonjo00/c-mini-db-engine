#include "wal.h"

#include <stdio.h>
#include <string.h>

static void wal_write_escaped(FILE *stream, const char *text) {
    const char *cursor;

    fputc('"', stream);
    for (cursor = text; *cursor != '\0'; ++cursor) {
        if (*cursor == '"') {
            fputc('"', stream);
        }
        fputc(*cursor, stream);
    }
    fputc('"', stream);
}

static bool wal_parse_line(const char *line, char fields[6][DB_MAX_COMMAND_LEN], size_t *field_count, char *error, size_t error_capacity) {
    size_t field_index = 0;
    size_t char_index = 0;
    bool in_quote = false;
    const char *cursor;

    for (cursor = line; ; ++cursor) {
        char ch = *cursor;
        bool at_end = ch == '\0' || ch == '\n' || ch == '\r';

        if (ch == '"') {
            if (in_quote && cursor[1] == '"') {
                if (char_index + 1 >= DB_MAX_COMMAND_LEN) {
                    snprintf(error, error_capacity, "WAL field exceeded maximum size.");
                    return false;
                }
                fields[field_index][char_index++] = '"';
                ++cursor;
            } else {
                in_quote = !in_quote;
            }
            continue;
        }

        if (!in_quote && (ch == '|' || at_end)) {
            if (field_index >= 6) {
                snprintf(error, error_capacity, "WAL row contained too many columns.");
                return false;
            }
            fields[field_index][char_index] = '\0';
            field_index += 1;
            char_index = 0;

            if (at_end) {
                break;
            }
            continue;
        }

        if (field_index >= 6 || char_index + 1 >= DB_MAX_COMMAND_LEN) {
            snprintf(error, error_capacity, "WAL field exceeded maximum size.");
            return false;
        }

        fields[field_index][char_index++] = ch;
    }

    if (in_quote) {
        snprintf(error, error_capacity, "WAL row ended inside a quoted field.");
        return false;
    }

    *field_count = field_index;
    return true;
}

static bool wal_open_append(const char *path, FILE **stream, char *error, size_t error_capacity) {
    if (!ensure_parent_directory(path)) {
        snprintf(error, error_capacity, "Failed to create WAL directory for %s.", path);
        return false;
    }

    *stream = fopen(path, "a");
    if (*stream == NULL) {
        snprintf(error, error_capacity, "Failed to open WAL at %s.", path);
        return false;
    }

    return true;
}

bool wal_append_insert(const char *path, const Record *record, char *error, size_t error_capacity) {
    FILE *stream;

    if (path == NULL || record == NULL) {
        snprintf(error, error_capacity, "WAL insert received a null pointer.");
        return false;
    }

    if (!wal_open_append(path, &stream, error, error_capacity)) {
        return false;
    }

    fprintf(stream, "INSERT|%d|", record->id);
    wal_write_escaped(stream, record->name);
    fprintf(stream, "|%d|", record->age);
    wal_write_escaped(stream, record->department);
    fputc('\n', stream);
    fclose(stream);
    return true;
}

bool wal_append_update(const char *path, int target_id, const Record *record, char *error, size_t error_capacity) {
    FILE *stream;

    if (path == NULL || record == NULL) {
        snprintf(error, error_capacity, "WAL update received a null pointer.");
        return false;
    }

    if (!wal_open_append(path, &stream, error, error_capacity)) {
        return false;
    }

    fprintf(stream, "UPDATE|%d|%d|", target_id, record->id);
    wal_write_escaped(stream, record->name);
    fprintf(stream, "|%d|", record->age);
    wal_write_escaped(stream, record->department);
    fputc('\n', stream);
    fclose(stream);
    return true;
}

bool wal_append_delete(const char *path, int id, char *error, size_t error_capacity) {
    FILE *stream;

    if (path == NULL) {
        snprintf(error, error_capacity, "WAL delete received a null path.");
        return false;
    }

    if (!wal_open_append(path, &stream, error, error_capacity)) {
        return false;
    }

    fprintf(stream, "DELETE|%d\n", id);
    fclose(stream);
    return true;
}

bool wal_checkpoint(const char *path, char *error, size_t error_capacity) {
    FILE *stream;

    if (path == NULL) {
        snprintf(error, error_capacity, "WAL checkpoint received a null path.");
        return false;
    }

    if (!ensure_parent_directory(path)) {
        snprintf(error, error_capacity, "Failed to create WAL directory for %s.", path);
        return false;
    }

    stream = fopen(path, "w");
    if (stream == NULL) {
        snprintf(error, error_capacity, "Failed to checkpoint WAL at %s.", path);
        return false;
    }

    fclose(stream);
    return true;
}

bool wal_replay(Database *database, const char *path, char *error, size_t error_capacity) {
    FILE *stream;
    char line[1024];
    size_t line_number = 0;

    if (database == NULL || path == NULL) {
        snprintf(error, error_capacity, "WAL replay received a null pointer.");
        return false;
    }

    stream = fopen(path, "r");
    if (stream == NULL) {
        return true;
    }

    while (fgets(line, sizeof(line), stream) != NULL) {
        char fields[6][DB_MAX_COMMAND_LEN] = {{0}};
        size_t field_count = 0;
        char *trimmed = trim_whitespace(line);

        if (trimmed[0] == '\0') {
            continue;
        }

        line_number += 1;
        if (!wal_parse_line(trimmed, fields, &field_count, error, error_capacity)) {
            char details[256];
            safe_copy(details, sizeof(details), error);
            snprintf(error, error_capacity, "WAL parse error on line %zu: %s", line_number, details);
            fclose(stream);
            return false;
        }

        if (string_ieq(fields[0], "INSERT")) {
            Record record;
            memset(&record, 0, sizeof(record));

            if (field_count != 5 ||
                !parse_int_strict(fields[1], &record.id) ||
                !safe_copy(record.name, sizeof(record.name), fields[2]) ||
                !parse_int_strict(fields[3], &record.age) ||
                !safe_copy(record.department, sizeof(record.department), fields[4])) {
                snprintf(error, error_capacity, "Malformed INSERT WAL record on line %zu.", line_number);
                fclose(stream);
                return false;
            }

            if (!database_insert_record(database, &record, error, error_capacity)) {
                char details[256];
                safe_copy(details, sizeof(details), error);
                snprintf(error, error_capacity, "Failed to replay INSERT on line %zu: %s", line_number, details);
                fclose(stream);
                return false;
            }
        } else if (string_ieq(fields[0], "UPDATE")) {
            UpdateAssignment assignments[4];
            int target_id = 0;
            int new_id = 0;
            int age = 0;

            if (field_count != 6 ||
                !parse_int_strict(fields[1], &target_id) ||
                !parse_int_strict(fields[2], &new_id) ||
                !parse_int_strict(fields[4], &age)) {
                snprintf(error, error_capacity, "Malformed UPDATE WAL record on line %zu.", line_number);
                fclose(stream);
                return false;
            }

            memset(assignments, 0, sizeof(assignments));
            assignments[0].field = FIELD_ID;
            assignments[0].int_value = new_id;
            assignments[0].value_is_int = true;
            snprintf(assignments[0].raw_value, sizeof(assignments[0].raw_value), "%d", new_id);

            assignments[1].field = FIELD_NAME;
            if (!safe_copy(assignments[1].raw_value, sizeof(assignments[1].raw_value), fields[3])) {
                snprintf(error, error_capacity, "Name exceeded limits on WAL line %zu.", line_number);
                fclose(stream);
                return false;
            }

            assignments[2].field = FIELD_AGE;
            assignments[2].int_value = age;
            assignments[2].value_is_int = true;
            snprintf(assignments[2].raw_value, sizeof(assignments[2].raw_value), "%d", age);

            assignments[3].field = FIELD_DEPARTMENT;
            if (!safe_copy(assignments[3].raw_value, sizeof(assignments[3].raw_value), fields[5])) {
                snprintf(error, error_capacity, "Department exceeded limits on WAL line %zu.", line_number);
                fclose(stream);
                return false;
            }

            if (!database_update_by_id(database, target_id, assignments, 4, error, error_capacity)) {
                char details[256];
                safe_copy(details, sizeof(details), error);
                snprintf(error, error_capacity, "Failed to replay UPDATE on line %zu: %s", line_number, details);
                fclose(stream);
                return false;
            }
        } else if (string_ieq(fields[0], "DELETE")) {
            int id = 0;

            if (field_count != 2 || !parse_int_strict(fields[1], &id)) {
                snprintf(error, error_capacity, "Malformed DELETE WAL record on line %zu.", line_number);
                fclose(stream);
                return false;
            }

            if (!database_delete_by_id(database, id, error, error_capacity)) {
                char details[256];
                safe_copy(details, sizeof(details), error);
                snprintf(error, error_capacity, "Failed to replay DELETE on line %zu: %s", line_number, details);
                fclose(stream);
                return false;
            }
        } else {
            snprintf(error, error_capacity, "Unknown WAL operation on line %zu.", line_number);
            fclose(stream);
            return false;
        }
    }

    fclose(stream);
    return true;
}
