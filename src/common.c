#include "common.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

void *db_calloc(size_t count, size_t size) {
    if (count == 0 || size == 0) {
        count = 1;
        size = 1;
    }
    return calloc(count, size);
}

void *db_realloc(void *ptr, size_t size) {
    if (size == 0) {
        size = 1;
    }
    return realloc(ptr, size);
}

char *db_strdup(const char *text) {
    size_t length;
    char *copy;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text) + 1;
    copy = (char *)db_calloc(length, sizeof(char));
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length);
    return copy;
}

bool safe_copy(char *destination, size_t destination_size, const char *source) {
    size_t length;

    if (destination == NULL || source == NULL || destination_size == 0) {
        return false;
    }

    length = strlen(source);
    if (length >= destination_size) {
        destination[0] = '\0';
        return false;
    }

    memcpy(destination, source, length + 1);
    return true;
}

char *trim_whitespace(char *text) {
    char *end;

    if (text == NULL) {
        return NULL;
    }

    while (*text != '\0' && isspace((unsigned char)*text)) {
        ++text;
    }

    if (*text == '\0') {
        return text;
    }

    end = text + strlen(text) - 1;
    while (end > text && isspace((unsigned char)*end)) {
        *end = '\0';
        --end;
    }

    return text;
}

bool string_ieq(const char *left, const char *right) {
    if (left == NULL || right == NULL) {
        return false;
    }

    while (*left != '\0' && *right != '\0') {
        if (tolower((unsigned char)*left) != tolower((unsigned char)*right)) {
            return false;
        }
        ++left;
        ++right;
    }

    return *left == '\0' && *right == '\0';
}

bool parse_int_strict(const char *text, int *value) {
    char *end = NULL;
    long parsed;

    if (text == NULL || value == NULL || *text == '\0') {
        return false;
    }

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
        return false;
    }

    if (parsed < INT_MIN || parsed > INT_MAX) {
        return false;
    }

    *value = (int)parsed;
    return true;
}

unsigned long hash_string(const char *text) {
    unsigned long hash = 5381;
    int character;

    if (text == NULL) {
        return 0;
    }

    while ((character = (unsigned char)*text++) != 0) {
        hash = ((hash << 5) + hash) + (unsigned long)character;
    }

    return hash;
}

bool ensure_directory(const char *path) {
    if (path == NULL || *path == '\0') {
        return false;
    }

#ifdef _WIN32
    if (CreateDirectoryA(path, NULL) != 0) {
        return true;
    }
    return GetLastError() == ERROR_ALREADY_EXISTS;
#else
    return mkdir(path, 0755) == 0 || errno == EEXIST;
#endif
}

bool ensure_parent_directory(const char *file_path) {
    char directory[DB_MAX_PATH_LEN];
    char *separator;

    if (file_path == NULL || *file_path == '\0') {
        return false;
    }

    if (!safe_copy(directory, sizeof(directory), file_path)) {
        return false;
    }

    separator = strrchr(directory, '/');
    if (separator == NULL) {
        separator = strrchr(directory, '\\');
    }

    if (separator == NULL) {
        return true;
    }

    *separator = '\0';
    if (directory[0] == '\0') {
        return true;
    }

    return ensure_directory(directory);
}

void format_current_timestamp(char *buffer, size_t buffer_size) {
    time_t now;
    struct tm timestamp;

    if (buffer == NULL || buffer_size == 0) {
        return;
    }

    now = time(NULL);
#ifdef _WIN32
    localtime_s(&timestamp, &now);
#else
    localtime_r(&now, &timestamp);
#endif

    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", &timestamp);
}
