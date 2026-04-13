#ifndef LOGGER_H
#define LOGGER_H

#include <stdbool.h>

#include "common.h"

typedef struct Logger {
    FILE *handle;
    char path[DB_MAX_PATH_LEN];
} Logger;

bool logger_init(Logger *logger, const char *path);
void logger_close(Logger *logger);
bool logger_log_query(Logger *logger, const char *command, const char *status, double elapsed_ms);

#endif
