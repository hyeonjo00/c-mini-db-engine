#ifndef BPTREE_H
#define BPTREE_H

#include <stdbool.h>
#include <stddef.h>

#include "common.h"

struct Record;

typedef struct BPlusValueNode {
    struct Record *record;
    struct BPlusValueNode *next;
} BPlusValueNode;

typedef struct BPlusTreeNode {
    bool is_leaf;
    size_t key_count;
    int keys[DB_BPTREE_MAX_KEYS];
    struct BPlusTreeNode *children[DB_BPTREE_MAX_KEYS + 1];
    BPlusValueNode *values[DB_BPTREE_MAX_KEYS];
    struct BPlusTreeNode *next;
} BPlusTreeNode;

typedef struct BPlusTree {
    BPlusTreeNode *root;
    BPlusTreeNode *first_leaf;
    bool unique_keys;
    char name[32];
} BPlusTree;

typedef struct BPlusRange {
    bool has_lower;
    int lower;
    bool lower_inclusive;
    bool has_upper;
    int upper;
    bool upper_inclusive;
} BPlusRange;

typedef struct BPlusTreeStats {
    size_t node_count;
    size_t internal_node_count;
    size_t leaf_node_count;
    size_t key_count;
    size_t value_count;
    size_t depth;
    size_t leaf_chain_length;
} BPlusTreeStats;

typedef bool (*BPlusTreeVisitor)(struct Record *record, void *context);

bool bptree_init(BPlusTree *tree, bool unique_keys, const char *name);
void bptree_destroy(BPlusTree *tree);
bool bptree_insert(BPlusTree *tree, int key, struct Record *record, char *error, size_t error_capacity);
bool bptree_remove(BPlusTree *tree, int key, struct Record *record, char *error, size_t error_capacity);
const BPlusValueNode *bptree_find(const BPlusTree *tree, int key);
struct Record *bptree_find_first(const BPlusTree *tree, int key);
bool bptree_range_scan(const BPlusTree *tree, const BPlusRange *range, BPlusTreeVisitor visitor, void *context, char *error, size_t error_capacity);
bool bptree_traverse_all(const BPlusTree *tree, BPlusTreeVisitor visitor, void *context, char *error, size_t error_capacity);
size_t bptree_estimate_memory(const BPlusTree *tree);
void bptree_collect_stats(const BPlusTree *tree, BPlusTreeStats *stats);

#endif
