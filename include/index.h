#ifndef INDEX_H
#define INDEX_H

#include <stdbool.h>
#include <stddef.h>

#include "common.h"

struct Record;

typedef struct IdIndexNode {
    int key;
    struct Record *record;
    struct IdIndexNode *next;
} IdIndexNode;

typedef struct IdIndex {
    IdIndexNode **buckets;
    size_t bucket_count;
} IdIndex;

typedef struct DepartmentRecordNode {
    struct Record *record;
    struct DepartmentRecordNode *next;
} DepartmentRecordNode;

typedef struct DepartmentIndexNode {
    char key[DB_MAX_DEPARTMENT_LEN];
    DepartmentRecordNode *records;
    struct DepartmentIndexNode *next;
} DepartmentIndexNode;

typedef struct DepartmentIndex {
    DepartmentIndexNode **buckets;
    size_t bucket_count;
} DepartmentIndex;

typedef struct IdIndexStats {
    size_t entry_count;
    size_t used_buckets;
    size_t max_chain_length;
} IdIndexStats;

typedef struct DepartmentIndexStats {
    size_t key_count;
    size_t record_count;
    size_t used_buckets;
    size_t max_chain_length;
} DepartmentIndexStats;

bool id_index_init(IdIndex *index, size_t bucket_count);
void id_index_destroy(IdIndex *index);
bool id_index_insert(IdIndex *index, int key, struct Record *record);
void id_index_remove(IdIndex *index, int key);
struct Record *id_index_find(const IdIndex *index, int key);

bool department_index_init(DepartmentIndex *index, size_t bucket_count);
void department_index_destroy(DepartmentIndex *index);
bool department_index_insert(DepartmentIndex *index, const char *key, struct Record *record);
void department_index_remove(DepartmentIndex *index, const char *key, struct Record *record);
const DepartmentIndexNode *department_index_find(const DepartmentIndex *index, const char *key);
size_t id_index_estimate_memory(const IdIndex *index);
size_t department_index_estimate_memory(const DepartmentIndex *index);
void id_index_collect_stats(const IdIndex *index, IdIndexStats *stats);
void department_index_collect_stats(const DepartmentIndex *index, DepartmentIndexStats *stats);

#endif
