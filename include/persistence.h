#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include <stdbool.h>

#include "database.h"

bool persistence_save_csv(const Database *database, const char *path, char *error, size_t error_capacity);
bool persistence_load_csv(Database *database, const char *path, char *error, size_t error_capacity);

#endif
