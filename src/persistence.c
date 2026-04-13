#include "persistence.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void csv_write_escaped(FILE *stream, const char *text) {
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

static bool csv_parse_line(const char *line, char fields[4][DB_MAX_COMMAND_LEN], size_t *field_count, char *error, size_t error_capacity) {
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
                    snprintf(error, error_capacity, "CSV field exceeded maximum size.");
                    return false;
                }
                fields[field_index][char_index++] = '"';
                ++cursor;
            } else {
                in_quote = !in_quote;
            }
            continue;
        }

        if (!in_quote && (ch == ',' || at_end)) {
            if (field_index >= 4) {
                snprintf(error, error_capacity, "CSV row contained too many columns.");
                return false;
            }
            fields[field_index][char_index] = '\0';
            field_index += 1;
            char_index = 0;

            if (field_index > 4) {
                snprintf(error, error_capacity, "CSV row contained too many columns.");
                return false;
            }

            if (at_end) {
                break;
            }
            continue;
        }

        if (field_index >= 4) {
            snprintf(error, error_capacity, "CSV row contained too many columns.");
            return false;
        }

        if (char_index + 1 >= DB_MAX_COMMAND_LEN) {
            snprintf(error, error_capacity, "CSV field exceeded maximum size.");
            return false;
        }

        fields[field_index][char_index++] = ch;
    }

    if (in_quote) {
        snprintf(error, error_capacity, "CSV row ended inside a quoted field.");
        return false;
    }

    *field_count = field_index;
    return true;
}

bool persistence_save_csv(const Database *database, const char *path, char *error, size_t error_capacity) {
    FILE *stream;
    size_t index;

    if (database == NULL || path == NULL) {
        snprintf(error, error_capacity, "Database or path pointer was null.");
        return false;
    }

    if (!ensure_parent_directory(path)) {
        snprintf(error, error_capacity, "Failed to create parent directory for %s.", path);
        return false;
    }

    stream = fopen(path, "w");
    if (stream == NULL) {
        snprintf(error, error_capacity, "Failed to open %s for writing.", path);
        return false;
    }

    fprintf(stream, "id,name,age,department\n");
    for (index = 0; index < database->count; ++index) {
        const Record *record = database->records[index];
        fprintf(stream, "%d,", record->id);
        csv_write_escaped(stream, record->name);
        fprintf(stream, ",%d,", record->age);
        csv_write_escaped(stream, record->department);
        fputc('\n', stream);
    }

    fclose(stream);
    return true;
}

bool persistence_load_csv(Database *database, const char *path, char *error, size_t error_capacity) {
    FILE *stream;
    char line[1024];
    size_t line_number = 0;
    Database loaded;
    bool initialized = false;
    bool success = false;

    if (database == NULL || path == NULL) {
        snprintf(error, error_capacity, "Database or path pointer was null.");
        return false;
    }

    stream = fopen(path, "r");
    if (stream == NULL) {
        snprintf(error, error_capacity, "Failed to open %s for reading.", path);
        return false;
    }

    if (!database_init(&loaded)) {
        fclose(stream);
        snprintf(error, error_capacity, "Failed to initialize temporary database for load.");
        return false;
    }
    initialized = true;

    while (fgets(line, sizeof(line), stream) != NULL) {
        char fields[4][DB_MAX_COMMAND_LEN] = {{0}};
        size_t field_count = 0;
        Record record;
        char *trimmed = trim_whitespace(line);

        if (trimmed[0] == '\0') {
            continue;
        }

        line_number += 1;
        if (!csv_parse_line(trimmed, fields, &field_count, error, error_capacity)) {
            char details[256];
            safe_copy(details, sizeof(details), error);
            snprintf(error, error_capacity, "CSV parse error on line %zu: %s", line_number, details);
            goto cleanup;
        }

        if (field_count != 4) {
            snprintf(error, error_capacity, "Expected 4 columns on line %zu, found %zu.", line_number, field_count);
            goto cleanup;
        }

        if (line_number == 1 &&
            string_ieq(fields[0], "id") &&
            string_ieq(fields[1], "name") &&
            string_ieq(fields[2], "age") &&
            string_ieq(fields[3], "department")) {
            continue;
        }

        memset(&record, 0, sizeof(record));
        if (!parse_int_strict(fields[0], &record.id)) {
            snprintf(error, error_capacity, "Invalid id on line %zu.", line_number);
            goto cleanup;
        }
        if (!safe_copy(record.name, sizeof(record.name), fields[1])) {
            snprintf(error, error_capacity, "name exceeds limit on line %zu.", line_number);
            goto cleanup;
        }
        if (!parse_int_strict(fields[2], &record.age)) {
            snprintf(error, error_capacity, "Invalid age on line %zu.", line_number);
            goto cleanup;
        }
        if (!safe_copy(record.department, sizeof(record.department), fields[3])) {
            snprintf(error, error_capacity, "department exceeds limit on line %zu.", line_number);
            goto cleanup;
        }

        if (!database_insert_record(&loaded, &record, error, error_capacity)) {
            char details[256];
            safe_copy(details, sizeof(details), error);
            snprintf(error, error_capacity, "Failed to load line %zu: %s", line_number, details);
            goto cleanup;
        }
    }

    fclose(stream);
    stream = NULL;
    database_destroy(database);
    *database = loaded;
    memset(&loaded, 0, sizeof(loaded));
    initialized = false;
    success = true;

cleanup:
    if (stream != NULL) {
        fclose(stream);
    }
    if (initialized) {
        database_destroy(&loaded);
    }
    return success;
}
