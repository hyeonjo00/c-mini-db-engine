#ifndef HISTORY_H
#define HISTORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef struct History {
    char **items;
    size_t count;
    size_t capacity;
} History;

bool history_init(History *history);
void history_destroy(History *history);
bool history_add(History *history, const char *command);
void history_print(const History *history, FILE *stream);

#endif
