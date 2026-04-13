#include "index.h"

#include <stdlib.h>
#include <string.h>

static size_t hash_int_key(int key, size_t bucket_count) {
    unsigned int value = (unsigned int)key;
    return (size_t)((value * 2654435761u) % (unsigned int)bucket_count);
}

static size_t hash_department_key(const char *key, size_t bucket_count) {
    return (size_t)(hash_string(key) % bucket_count);
}

bool id_index_init(IdIndex *index, size_t bucket_count) {
    if (index == NULL || bucket_count == 0) {
        return false;
    }

    index->buckets = (IdIndexNode **)db_calloc(bucket_count, sizeof(IdIndexNode *));
    if (index->buckets == NULL) {
        return false;
    }

    index->bucket_count = bucket_count;
    return true;
}

void id_index_destroy(IdIndex *index) {
    size_t bucket;

    if (index == NULL) {
        return;
    }

    if (index->buckets != NULL) {
        for (bucket = 0; bucket < index->bucket_count; ++bucket) {
            IdIndexNode *node = index->buckets[bucket];
            while (node != NULL) {
                IdIndexNode *next = node->next;
                free(node);
                node = next;
            }
        }
        free(index->buckets);
    }

    index->buckets = NULL;
    index->bucket_count = 0;
}

bool id_index_insert(IdIndex *index, int key, struct Record *record) {
    size_t bucket;
    IdIndexNode *node;

    if (index == NULL || index->buckets == NULL || record == NULL) {
        return false;
    }

    if (id_index_find(index, key) != NULL) {
        return false;
    }

    node = (IdIndexNode *)db_calloc(1, sizeof(IdIndexNode));
    if (node == NULL) {
        return false;
    }

    bucket = hash_int_key(key, index->bucket_count);
    node->key = key;
    node->record = record;
    node->next = index->buckets[bucket];
    index->buckets[bucket] = node;
    return true;
}

void id_index_remove(IdIndex *index, int key) {
    size_t bucket;
    IdIndexNode *previous = NULL;
    IdIndexNode *current;

    if (index == NULL || index->buckets == NULL) {
        return;
    }

    bucket = hash_int_key(key, index->bucket_count);
    current = index->buckets[bucket];
    while (current != NULL) {
        if (current->key == key) {
            if (previous == NULL) {
                index->buckets[bucket] = current->next;
            } else {
                previous->next = current->next;
            }
            free(current);
            return;
        }
        previous = current;
        current = current->next;
    }
}

struct Record *id_index_find(const IdIndex *index, int key) {
    size_t bucket;
    IdIndexNode *current;

    if (index == NULL || index->buckets == NULL || index->bucket_count == 0) {
        return NULL;
    }

    bucket = hash_int_key(key, index->bucket_count);
    current = index->buckets[bucket];
    while (current != NULL) {
        if (current->key == key) {
            return current->record;
        }
        current = current->next;
    }

    return NULL;
}

bool department_index_init(DepartmentIndex *index, size_t bucket_count) {
    if (index == NULL || bucket_count == 0) {
        return false;
    }

    index->buckets = (DepartmentIndexNode **)db_calloc(bucket_count, sizeof(DepartmentIndexNode *));
    if (index->buckets == NULL) {
        return false;
    }

    index->bucket_count = bucket_count;
    return true;
}

void department_index_destroy(DepartmentIndex *index) {
    size_t bucket;

    if (index == NULL) {
        return;
    }

    if (index->buckets != NULL) {
        for (bucket = 0; bucket < index->bucket_count; ++bucket) {
            DepartmentIndexNode *node = index->buckets[bucket];
            while (node != NULL) {
                DepartmentIndexNode *next = node->next;
                DepartmentRecordNode *record_node = node->records;
                while (record_node != NULL) {
                    DepartmentRecordNode *record_next = record_node->next;
                    free(record_node);
                    record_node = record_next;
                }
                free(node);
                node = next;
            }
        }
        free(index->buckets);
    }

    index->buckets = NULL;
    index->bucket_count = 0;
}

const DepartmentIndexNode *department_index_find(const DepartmentIndex *index, const char *key) {
    size_t bucket;
    DepartmentIndexNode *current;

    if (index == NULL || index->buckets == NULL || key == NULL || index->bucket_count == 0) {
        return NULL;
    }

    bucket = hash_department_key(key, index->bucket_count);
    current = index->buckets[bucket];
    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

size_t id_index_estimate_memory(const IdIndex *index) {
    size_t bytes = 0;
    size_t bucket;

    if (index == NULL) {
        return 0;
    }

    bytes += index->bucket_count * sizeof(IdIndexNode *);

    if (index->buckets == NULL) {
        return bytes;
    }

    for (bucket = 0; bucket < index->bucket_count; ++bucket) {
        IdIndexNode *node = index->buckets[bucket];
        while (node != NULL) {
            bytes += sizeof(IdIndexNode);
            node = node->next;
        }
    }

    return bytes;
}

bool department_index_insert(DepartmentIndex *index, const char *key, struct Record *record) {
    size_t bucket;
    DepartmentIndexNode *entry;
    DepartmentRecordNode *record_node;

    if (index == NULL || index->buckets == NULL || key == NULL || record == NULL) {
        return false;
    }

    bucket = hash_department_key(key, index->bucket_count);
    entry = index->buckets[bucket];
    while (entry != NULL) {
        if (strcmp(entry->key, key) == 0) {
            break;
        }
        entry = entry->next;
    }

    if (entry == NULL) {
        entry = (DepartmentIndexNode *)db_calloc(1, sizeof(DepartmentIndexNode));
        if (entry == NULL) {
            return false;
        }
        if (!safe_copy(entry->key, sizeof(entry->key), key)) {
            free(entry);
            return false;
        }
        entry->next = index->buckets[bucket];
        index->buckets[bucket] = entry;
    }

    record_node = (DepartmentRecordNode *)db_calloc(1, sizeof(DepartmentRecordNode));
    if (record_node == NULL) {
        return false;
    }

    record_node->record = record;
    record_node->next = entry->records;
    entry->records = record_node;
    return true;
}

void department_index_remove(DepartmentIndex *index, const char *key, struct Record *record) {
    size_t bucket;
    DepartmentIndexNode *previous_entry = NULL;
    DepartmentIndexNode *entry;

    if (index == NULL || index->buckets == NULL || key == NULL || record == NULL) {
        return;
    }

    bucket = hash_department_key(key, index->bucket_count);
    entry = index->buckets[bucket];
    while (entry != NULL) {
        if (strcmp(entry->key, key) == 0) {
            DepartmentRecordNode *previous_record = NULL;
            DepartmentRecordNode *current_record = entry->records;

            while (current_record != NULL) {
                if (current_record->record == record) {
                    if (previous_record == NULL) {
                        entry->records = current_record->next;
                    } else {
                        previous_record->next = current_record->next;
                    }
                    free(current_record);
                    break;
                }
                previous_record = current_record;
                current_record = current_record->next;
            }

            if (entry->records == NULL) {
                if (previous_entry == NULL) {
                    index->buckets[bucket] = entry->next;
                } else {
                    previous_entry->next = entry->next;
                }
                free(entry);
            }
            return;
        }
        previous_entry = entry;
        entry = entry->next;
    }
}

size_t department_index_estimate_memory(const DepartmentIndex *index) {
    size_t bytes = 0;
    size_t bucket;

    if (index == NULL) {
        return 0;
    }

    bytes += index->bucket_count * sizeof(DepartmentIndexNode *);

    if (index->buckets == NULL) {
        return bytes;
    }

    for (bucket = 0; bucket < index->bucket_count; ++bucket) {
        DepartmentIndexNode *entry = index->buckets[bucket];
        while (entry != NULL) {
            DepartmentRecordNode *record_node = entry->records;
            bytes += sizeof(DepartmentIndexNode);
            while (record_node != NULL) {
                bytes += sizeof(DepartmentRecordNode);
                record_node = record_node->next;
            }
            entry = entry->next;
        }
    }

    return bytes;
}

void id_index_collect_stats(const IdIndex *index, IdIndexStats *stats) {
    size_t bucket;

    if (stats == NULL) {
        return;
    }

    memset(stats, 0, sizeof(*stats));
    if (index == NULL || index->buckets == NULL) {
        return;
    }

    for (bucket = 0; bucket < index->bucket_count; ++bucket) {
        const IdIndexNode *node = index->buckets[bucket];
        size_t chain_length = 0;

        while (node != NULL) {
            chain_length += 1;
            stats->entry_count += 1;
            node = node->next;
        }

        if (chain_length > 0) {
            stats->used_buckets += 1;
            if (chain_length > stats->max_chain_length) {
                stats->max_chain_length = chain_length;
            }
        }
    }
}

void department_index_collect_stats(const DepartmentIndex *index, DepartmentIndexStats *stats) {
    size_t bucket;

    if (stats == NULL) {
        return;
    }

    memset(stats, 0, sizeof(*stats));
    if (index == NULL || index->buckets == NULL) {
        return;
    }

    for (bucket = 0; bucket < index->bucket_count; ++bucket) {
        const DepartmentIndexNode *node = index->buckets[bucket];
        size_t chain_length = 0;

        while (node != NULL) {
            const DepartmentRecordNode *record_node = node->records;
            chain_length += 1;
            stats->key_count += 1;
            while (record_node != NULL) {
                stats->record_count += 1;
                record_node = record_node->next;
            }
            node = node->next;
        }

        if (chain_length > 0) {
            stats->used_buckets += 1;
            if (chain_length > stats->max_chain_length) {
                stats->max_chain_length = chain_length;
            }
        }
    }
}
