#ifndef WAL_H
#define WAL_H

#include <stdbool.h>

#include "database.h"

bool wal_append_insert(const char *path, const Record *record, char *error, size_t error_capacity);
bool wal_append_update(const char *path, int target_id, const Record *record, char *error, size_t error_capacity);
bool wal_append_delete(const char *path, int id, char *error, size_t error_capacity);
bool wal_checkpoint(const char *path, char *error, size_t error_capacity);
bool wal_replay(Database *database, const char *path, char *error, size_t error_capacity);

#endif
