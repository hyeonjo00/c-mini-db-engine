#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define DB_MAX_NAME_LEN 50
#define DB_MAX_DEPARTMENT_LEN 50
#define DB_MAX_VALUE_LEN 64
#define DB_MAX_COMMAND_LEN 512
#define DB_MAX_PATH_LEN 260
#define DB_MAX_PREDICATES 8
#define DB_MAX_PLAN_DESCRIPTION 96
#define DB_DEFAULT_DATA_PATH "data/db.csv"
#define DB_DEFAULT_LOG_PATH "data/query.log"
#define DB_DEFAULT_WAL_PATH "data/db.log"
#define DB_ID_INDEX_BUCKETS 257
#define DB_DEPARTMENT_INDEX_BUCKETS 257
#define DB_BPTREE_MAX_KEYS 4

void *db_calloc(size_t count, size_t size);
void *db_realloc(void *ptr, size_t size);
char *db_strdup(const char *text);
bool safe_copy(char *destination, size_t destination_size, const char *source);
char *trim_whitespace(char *text);
bool string_ieq(const char *left, const char *right);
bool parse_int_strict(const char *text, int *value);
unsigned long hash_string(const char *text);
bool ensure_directory(const char *path);
bool ensure_parent_directory(const char *file_path);
void format_current_timestamp(char *buffer, size_t buffer_size);

#endif
