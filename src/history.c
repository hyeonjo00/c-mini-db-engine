#include "history.h"

#include <stdlib.h>
#include <string.h>

#include "common.h"

static bool history_reserve(History *history, size_t required_capacity) {
    char **resized;
    size_t new_capacity;

    if (history == NULL) {
        return false;
    }

    if (history->capacity >= required_capacity) {
        return true;
    }

    new_capacity = history->capacity == 0 ? 16 : history->capacity * 2;
    while (new_capacity < required_capacity) {
        new_capacity *= 2;
    }

    resized = (char **)db_realloc(history->items, new_capacity * sizeof(char *));
    if (resized == NULL) {
        return false;
    }

    history->items = resized;
    history->capacity = new_capacity;
    return true;
}

bool history_init(History *history) {
    if (history == NULL) {
        return false;
    }

    history->items = NULL;
    history->count = 0;
    history->capacity = 0;
    return true;
}

void history_destroy(History *history) {
    size_t index;

    if (history == NULL) {
        return;
    }

    for (index = 0; index < history->count; ++index) {
        free(history->items[index]);
    }

    free(history->items);
    history->items = NULL;
    history->count = 0;
    history->capacity = 0;
}

bool history_add(History *history, const char *command) {
    char *copy;

    if (history == NULL || command == NULL) {
        return false;
    }

    if (!history_reserve(history, history->count + 1)) {
        return false;
    }

    copy = db_strdup(command);
    if (copy == NULL) {
        return false;
    }

    history->items[history->count++] = copy;
    return true;
}

void history_print(const History *history, FILE *stream) {
    size_t index;

    if (history == NULL || stream == NULL) {
        return;
    }

    for (index = 0; index < history->count; ++index) {
        fprintf(stream, "%4zu  %s\n", index + 1, history->items[index]);
    }
}
