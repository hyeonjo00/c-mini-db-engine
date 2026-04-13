#include "logger.h"

#include <stdio.h>
#include <string.h>

bool logger_init(Logger *logger, const char *path) {
    if (logger == NULL || path == NULL) {
        return false;
    }

    memset(logger, 0, sizeof(*logger));
    if (!safe_copy(logger->path, sizeof(logger->path), path)) {
        return false;
    }

    if (!ensure_parent_directory(path)) {
        return false;
    }

    logger->handle = fopen(path, "a");
    return logger->handle != NULL;
}

void logger_close(Logger *logger) {
    if (logger == NULL) {
        return;
    }

    if (logger->handle != NULL) {
        fclose(logger->handle);
        logger->handle = NULL;
    }
}

bool logger_log_query(Logger *logger, const char *command, const char *status, double elapsed_ms) {
    char timestamp[32];

    if (logger == NULL || logger->handle == NULL || command == NULL || status == NULL) {
        return false;
    }

    format_current_timestamp(timestamp, sizeof(timestamp));
    fprintf(logger->handle, "[%s] status=%s elapsed_ms=%.3f command=%s\n", timestamp, status, elapsed_ms, command);
    fflush(logger->handle);
    return true;
}
