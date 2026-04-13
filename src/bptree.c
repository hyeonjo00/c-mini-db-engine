#include "bptree.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BPTREE_MIN_KEYS (DB_BPTREE_MAX_KEYS / 2)

typedef struct BPlusInsertResult {
    bool split;
    int promoted_key;
    BPlusTreeNode *right;
} BPlusInsertResult;

static BPlusTreeNode *allocate_node(bool is_leaf) {
    BPlusTreeNode *node = (BPlusTreeNode *)db_calloc(1, sizeof(BPlusTreeNode));
    if (node != NULL) {
        node->is_leaf = is_leaf;
    }
    return node;
}

static BPlusValueNode *allocate_value_node(struct Record *record) {
    BPlusValueNode *node = (BPlusValueNode *)db_calloc(1, sizeof(BPlusValueNode));
    if (node != NULL) {
        node->record = record;
    }
    return node;
}

static void destroy_value_list(BPlusValueNode *node) {
    while (node != NULL) {
        BPlusValueNode *next = node->next;
        free(node);
        node = next;
    }
}

static void destroy_node(BPlusTreeNode *node) {
    size_t index;

    if (node == NULL) {
        return;
    }

    if (node->is_leaf) {
        for (index = 0; index < node->key_count; ++index) {
            destroy_value_list(node->values[index]);
        }
    } else {
        for (index = 0; index <= node->key_count; ++index) {
            destroy_node(node->children[index]);
        }
    }

    free(node);
}

static size_t estimate_value_list_memory(const BPlusValueNode *node) {
    size_t bytes = 0;

    while (node != NULL) {
        bytes += sizeof(BPlusValueNode);
        node = node->next;
    }

    return bytes;
}

static size_t estimate_node_memory(const BPlusTreeNode *node) {
    size_t bytes = 0;
    size_t index;

    if (node == NULL) {
        return 0;
    }

    bytes += sizeof(BPlusTreeNode);
    if (node->is_leaf) {
        for (index = 0; index < node->key_count; ++index) {
            bytes += estimate_value_list_memory(node->values[index]);
        }
    } else {
        for (index = 0; index <= node->key_count; ++index) {
            bytes += estimate_node_memory(node->children[index]);
        }
    }

    return bytes;
}

static int node_min_key(const BPlusTreeNode *node) {
    const BPlusTreeNode *cursor = node;

    while (cursor != NULL && !cursor->is_leaf) {
        cursor = cursor->children[0];
    }

    if (cursor == NULL || cursor->key_count == 0) {
        return 0;
    }

    return cursor->keys[0];
}

static void refresh_internal_keys(BPlusTreeNode *node) {
    size_t index;

    if (node == NULL || node->is_leaf) {
        return;
    }

    for (index = 0; index < node->key_count; ++index) {
        node->keys[index] = node_min_key(node->children[index + 1]);
    }
}

static size_t child_index_for_key(const BPlusTreeNode *node, int key) {
    size_t index = 0;

    while (index < node->key_count && key >= node->keys[index]) {
        ++index;
    }

    return index;
}

static BPlusTreeNode *find_leaf(const BPlusTree *tree, int key) {
    BPlusTreeNode *cursor;

    if (tree == NULL || tree->root == NULL) {
        return NULL;
    }

    cursor = tree->root;
    while (!cursor->is_leaf) {
        cursor = cursor->children[child_index_for_key(cursor, key)];
    }

    return cursor;
}

static bool append_value_to_existing_key(BPlusTree *tree, BPlusTreeNode *leaf, size_t index, struct Record *record, char *error, size_t error_capacity) {
    BPlusValueNode *value_node;

    if (tree->unique_keys) {
        snprintf(error, error_capacity, "%s B+ Tree already contains key=%d.", tree->name, leaf->keys[index]);
        return false;
    }

    value_node = allocate_value_node(record);
    if (value_node == NULL) {
        snprintf(error, error_capacity, "Failed to allocate B+ Tree value node.");
        return false;
    }

    value_node->next = leaf->values[index];
    leaf->values[index] = value_node;
    return true;
}

static bool leaf_insert_without_split(BPlusTree *tree, BPlusTreeNode *leaf, int key, struct Record *record, char *error, size_t error_capacity) {
    size_t insert_index = 0;
    size_t index;
    BPlusValueNode *value_node;

    while (insert_index < leaf->key_count && leaf->keys[insert_index] < key) {
        ++insert_index;
    }

    if (insert_index < leaf->key_count && leaf->keys[insert_index] == key) {
        return append_value_to_existing_key(tree, leaf, insert_index, record, error, error_capacity);
    }

    value_node = allocate_value_node(record);
    if (value_node == NULL) {
        snprintf(error, error_capacity, "Failed to allocate B+ Tree value node.");
        return false;
    }

    for (index = leaf->key_count; index > insert_index; --index) {
        leaf->keys[index] = leaf->keys[index - 1];
        leaf->values[index] = leaf->values[index - 1];
    }

    leaf->keys[insert_index] = key;
    leaf->values[insert_index] = value_node;
    leaf->key_count += 1;
    return true;
}

static bool leaf_insert_with_split(
    BPlusTree *tree,
    BPlusTreeNode *leaf,
    int key,
    struct Record *record,
    BPlusInsertResult *result,
    char *error,
    size_t error_capacity
) {
    int temp_keys[DB_BPTREE_MAX_KEYS + 1];
    BPlusValueNode *temp_values[DB_BPTREE_MAX_KEYS + 1];
    size_t existing_index = 0;
    size_t insert_index = 0;
    size_t total_keys = leaf->key_count + 1;
    size_t split_index = total_keys / 2;
    BPlusTreeNode *right;
    BPlusValueNode *value_node;
    size_t index;

    while (insert_index < leaf->key_count && leaf->keys[insert_index] < key) {
        ++insert_index;
    }

    if (insert_index < leaf->key_count && leaf->keys[insert_index] == key) {
        return append_value_to_existing_key(tree, leaf, insert_index, record, error, error_capacity);
    }

    value_node = allocate_value_node(record);
    if (value_node == NULL) {
        snprintf(error, error_capacity, "Failed to allocate B+ Tree value node.");
        return false;
    }

    for (index = 0; index < total_keys; ++index) {
        if (index == insert_index) {
            temp_keys[index] = key;
            temp_values[index] = value_node;
        } else {
            temp_keys[index] = leaf->keys[existing_index];
            temp_values[index] = leaf->values[existing_index];
            ++existing_index;
        }
    }

    right = allocate_node(true);
    if (right == NULL) {
        free(value_node);
        snprintf(error, error_capacity, "Failed to allocate split B+ Tree leaf node.");
        return false;
    }

    memset(leaf->values, 0, sizeof(leaf->values));
    memset(right->values, 0, sizeof(right->values));
    leaf->key_count = split_index;
    right->key_count = total_keys - split_index;

    /*
     * Leaf split logic:
     * 1. Insert the new key into a temporary array with room for one extra entry.
     * 2. Keep the lower half in the existing leaf and move the upper half into a new
     *    right-hand sibling leaf.
     * 3. Promote the first key in the new right leaf into the parent. In a B+ Tree,
     *    internal nodes store separator keys while all actual records stay in leaves.
     */
    for (index = 0; index < leaf->key_count; ++index) {
        leaf->keys[index] = temp_keys[index];
        leaf->values[index] = temp_values[index];
    }
    for (index = leaf->key_count; index < DB_BPTREE_MAX_KEYS; ++index) {
        leaf->values[index] = NULL;
    }

    for (index = 0; index < right->key_count; ++index) {
        right->keys[index] = temp_keys[split_index + index];
        right->values[index] = temp_values[split_index + index];
    }

    right->next = leaf->next;
    leaf->next = right;

    result->split = true;
    result->promoted_key = right->keys[0];
    result->right = right;
    return true;
}

static bool internal_insert_with_split(
    BPlusTreeNode *node,
    size_t child_index,
    int promoted_key,
    BPlusTreeNode *right_child,
    BPlusInsertResult *result,
    char *error,
    size_t error_capacity
) {
    int temp_keys[DB_BPTREE_MAX_KEYS + 1];
    BPlusTreeNode *temp_children[DB_BPTREE_MAX_KEYS + 2];
    size_t total_keys = node->key_count + 1;
    size_t split_index = total_keys / 2;
    size_t key_source = 0;
    size_t child_source = 0;
    size_t index;
    BPlusTreeNode *right;

    for (index = 0; index < total_keys; ++index) {
        if (index == child_index) {
            temp_keys[index] = promoted_key;
        } else {
            temp_keys[index] = node->keys[key_source++];
        }
    }

    for (index = 0; index < total_keys + 1; ++index) {
        if (index == child_index + 1) {
            temp_children[index] = right_child;
        } else {
            temp_children[index] = node->children[child_source++];
        }
    }

    right = allocate_node(false);
    if (right == NULL) {
        snprintf(error, error_capacity, "Failed to allocate split B+ Tree internal node.");
        return false;
    }

    node->key_count = split_index;
    for (index = 0; index < node->key_count; ++index) {
        node->keys[index] = temp_keys[index];
        node->children[index] = temp_children[index];
    }
    node->children[node->key_count] = temp_children[node->key_count];
    for (index = node->key_count + 1; index < DB_BPTREE_MAX_KEYS + 1; ++index) {
        node->children[index] = NULL;
    }

    right->key_count = total_keys - split_index - 1;
    for (index = 0; index < right->key_count; ++index) {
        right->keys[index] = temp_keys[split_index + 1 + index];
        right->children[index] = temp_children[split_index + 1 + index];
    }
    right->children[right->key_count] = temp_children[total_keys];

    /*
     * Internal split logic:
     * 1. Temporarily materialize keys and child pointers with the newly promoted child.
     * 2. Keep the lower half in the current node.
     * 3. Move the upper half into a new sibling node.
     * 4. Promote the middle separator key upward so the parent can route lookups
     *    between the left and right internal nodes.
     */
    result->split = true;
    result->promoted_key = temp_keys[split_index];
    result->right = right;
    refresh_internal_keys(node);
    refresh_internal_keys(right);
    return true;
}

static bool insert_recursive(
    BPlusTree *tree,
    BPlusTreeNode *node,
    int key,
    struct Record *record,
    BPlusInsertResult *result,
    char *error,
    size_t error_capacity
) {
    if (node->is_leaf) {
        if (node->key_count < DB_BPTREE_MAX_KEYS) {
            return leaf_insert_without_split(tree, node, key, record, error, error_capacity);
        }
        return leaf_insert_with_split(tree, node, key, record, result, error, error_capacity);
    }

    {
        size_t child_index = child_index_for_key(node, key);
        BPlusInsertResult child_result;

        memset(&child_result, 0, sizeof(child_result));
        if (!insert_recursive(tree, node->children[child_index], key, record, &child_result, error, error_capacity)) {
            return false;
        }

        if (!child_result.split) {
            refresh_internal_keys(node);
            return true;
        }

        if (node->key_count < DB_BPTREE_MAX_KEYS) {
            size_t index;

            for (index = node->key_count; index > child_index; --index) {
                node->keys[index] = node->keys[index - 1];
                node->children[index + 1] = node->children[index];
            }
            node->keys[child_index] = child_result.promoted_key;
            node->children[child_index + 1] = child_result.right;
            node->key_count += 1;
            refresh_internal_keys(node);
            return true;
        }

        return internal_insert_with_split(
            node,
            child_index,
            child_result.promoted_key,
            child_result.right,
            result,
            error,
            error_capacity);
    }
}

static bool remove_record_from_value_list(BPlusValueNode **head, struct Record *record, bool unique_keys) {
    BPlusValueNode *previous = NULL;
    BPlusValueNode *current = *head;

    while (current != NULL) {
        if (unique_keys || record == NULL || current->record == record) {
            if (previous == NULL) {
                *head = current->next;
            } else {
                previous->next = current->next;
            }
            free(current);
            return true;
        }
        previous = current;
        current = current->next;
    }

    return false;
}

static bool node_underflow(const BPlusTreeNode *node, bool is_root) {
    if (node == NULL || is_root) {
        return false;
    }
    return node->key_count < BPTREE_MIN_KEYS;
}

static void remove_parent_entry(BPlusTreeNode *parent, size_t key_index) {
    size_t index;

    for (index = key_index; index + 1 < parent->key_count; ++index) {
        parent->keys[index] = parent->keys[index + 1];
    }

    for (index = key_index + 1; index < parent->key_count; ++index) {
        parent->children[index] = parent->children[index + 1];
    }

    parent->children[parent->key_count] = NULL;
    parent->key_count -= 1;
}

static void borrow_leaf_from_left(BPlusTreeNode *parent, size_t child_index) {
    BPlusTreeNode *left = parent->children[child_index - 1];
    BPlusTreeNode *child = parent->children[child_index];
    size_t index;

    for (index = child->key_count; index > 0; --index) {
        child->keys[index] = child->keys[index - 1];
        child->values[index] = child->values[index - 1];
    }

    child->keys[0] = left->keys[left->key_count - 1];
    child->values[0] = left->values[left->key_count - 1];
    child->key_count += 1;
    left->key_count -= 1;
    left->values[left->key_count] = NULL;
    refresh_internal_keys(parent);
}

static void borrow_leaf_from_right(BPlusTreeNode *parent, size_t child_index) {
    BPlusTreeNode *child = parent->children[child_index];
    BPlusTreeNode *right = parent->children[child_index + 1];
    size_t index;

    child->keys[child->key_count] = right->keys[0];
    child->values[child->key_count] = right->values[0];
    child->key_count += 1;

    for (index = 0; index + 1 < right->key_count; ++index) {
        right->keys[index] = right->keys[index + 1];
        right->values[index] = right->values[index + 1];
    }

    right->key_count -= 1;
    right->values[right->key_count] = NULL;
    refresh_internal_keys(parent);
}

static void merge_leaf_with_left(BPlusTreeNode *parent, size_t child_index) {
    BPlusTreeNode *left = parent->children[child_index - 1];
    BPlusTreeNode *child = parent->children[child_index];
    size_t index;

    for (index = 0; index < child->key_count; ++index) {
        left->keys[left->key_count + index] = child->keys[index];
        left->values[left->key_count + index] = child->values[index];
    }
    left->key_count += child->key_count;
    left->next = child->next;

    free(child);
    parent->children[child_index] = NULL;
    remove_parent_entry(parent, child_index - 1);
    refresh_internal_keys(parent);
}

static void merge_leaf_with_right(BPlusTreeNode *parent, size_t child_index) {
    BPlusTreeNode *child = parent->children[child_index];
    BPlusTreeNode *right = parent->children[child_index + 1];
    size_t index;

    for (index = 0; index < right->key_count; ++index) {
        child->keys[child->key_count + index] = right->keys[index];
        child->values[child->key_count + index] = right->values[index];
    }
    child->key_count += right->key_count;
    child->next = right->next;

    free(right);
    parent->children[child_index + 1] = NULL;
    remove_parent_entry(parent, child_index);
    refresh_internal_keys(parent);
}

static void borrow_internal_from_left(BPlusTreeNode *parent, size_t child_index) {
    BPlusTreeNode *left = parent->children[child_index - 1];
    BPlusTreeNode *child = parent->children[child_index];
    size_t index;

    for (index = child->key_count; index > 0; --index) {
        child->keys[index] = child->keys[index - 1];
    }
    for (index = child->key_count + 1; index > 0; --index) {
        child->children[index] = child->children[index - 1];
    }

    child->keys[0] = parent->keys[child_index - 1];
    child->children[0] = left->children[left->key_count];
    child->key_count += 1;
    left->key_count -= 1;
    left->children[left->key_count + 1] = NULL;

    refresh_internal_keys(left);
    refresh_internal_keys(child);
    refresh_internal_keys(parent);
}

static void borrow_internal_from_right(BPlusTreeNode *parent, size_t child_index) {
    BPlusTreeNode *child = parent->children[child_index];
    BPlusTreeNode *right = parent->children[child_index + 1];
    size_t index;

    child->keys[child->key_count] = parent->keys[child_index];
    child->children[child->key_count + 1] = right->children[0];
    child->key_count += 1;

    for (index = 0; index + 1 < right->key_count; ++index) {
        right->keys[index] = right->keys[index + 1];
    }
    for (index = 0; index < right->key_count; ++index) {
        right->children[index] = right->children[index + 1];
    }
    right->children[right->key_count] = right->children[right->key_count + 1];
    right->children[right->key_count + 1] = NULL;
    right->key_count -= 1;

    refresh_internal_keys(child);
    refresh_internal_keys(right);
    refresh_internal_keys(parent);
}

static void merge_internal_with_left(BPlusTreeNode *parent, size_t child_index) {
    BPlusTreeNode *left = parent->children[child_index - 1];
    BPlusTreeNode *child = parent->children[child_index];
    size_t offset = left->key_count;
    size_t index;

    left->keys[offset] = parent->keys[child_index - 1];
    for (index = 0; index < child->key_count; ++index) {
        left->keys[offset + 1 + index] = child->keys[index];
    }
    for (index = 0; index <= child->key_count; ++index) {
        left->children[offset + 1 + index] = child->children[index];
    }
    left->key_count += 1 + child->key_count;

    free(child);
    parent->children[child_index] = NULL;
    remove_parent_entry(parent, child_index - 1);
    refresh_internal_keys(left);
    refresh_internal_keys(parent);
}

static void merge_internal_with_right(BPlusTreeNode *parent, size_t child_index) {
    BPlusTreeNode *child = parent->children[child_index];
    BPlusTreeNode *right = parent->children[child_index + 1];
    size_t offset = child->key_count;
    size_t index;

    child->keys[offset] = parent->keys[child_index];
    for (index = 0; index < right->key_count; ++index) {
        child->keys[offset + 1 + index] = right->keys[index];
    }
    for (index = 0; index <= right->key_count; ++index) {
        child->children[offset + 1 + index] = right->children[index];
    }
    child->key_count += 1 + right->key_count;

    free(right);
    parent->children[child_index + 1] = NULL;
    remove_parent_entry(parent, child_index);
    refresh_internal_keys(child);
    refresh_internal_keys(parent);
}

static void rebalance_child(BPlusTreeNode *parent, size_t child_index) {
    BPlusTreeNode *child = parent->children[child_index];
    BPlusTreeNode *left = child_index > 0 ? parent->children[child_index - 1] : NULL;
    BPlusTreeNode *right = child_index + 1 <= parent->key_count ? parent->children[child_index + 1] : NULL;

    if (child->is_leaf) {
        if (left != NULL && left->key_count > BPTREE_MIN_KEYS) {
            borrow_leaf_from_left(parent, child_index);
            return;
        }
        if (right != NULL && right->key_count > BPTREE_MIN_KEYS) {
            borrow_leaf_from_right(parent, child_index);
            return;
        }
        if (left != NULL) {
            merge_leaf_with_left(parent, child_index);
            return;
        }
        if (right != NULL) {
            merge_leaf_with_right(parent, child_index);
        }
        return;
    }

    if (left != NULL && left->key_count > BPTREE_MIN_KEYS) {
        borrow_internal_from_left(parent, child_index);
        return;
    }
    if (right != NULL && right->key_count > BPTREE_MIN_KEYS) {
        borrow_internal_from_right(parent, child_index);
        return;
    }
    if (left != NULL) {
        merge_internal_with_left(parent, child_index);
        return;
    }
    if (right != NULL) {
        merge_internal_with_right(parent, child_index);
    }
}

static bool delete_recursive(
    BPlusTree *tree,
    BPlusTreeNode *node,
    int key,
    struct Record *record,
    bool *removed,
    char *error,
    size_t error_capacity
) {
    (void)error;
    (void)error_capacity;

    if (node->is_leaf) {
        size_t index = 0;

        while (index < node->key_count && node->keys[index] < key) {
            ++index;
        }

        if (index == node->key_count || node->keys[index] != key) {
            *removed = false;
            return true;
        }

        if (!remove_record_from_value_list(&node->values[index], record, tree->unique_keys)) {
            *removed = false;
            return true;
        }

        *removed = true;
        if (node->values[index] != NULL) {
            return true;
        }

        for (; index + 1 < node->key_count; ++index) {
            node->keys[index] = node->keys[index + 1];
            node->values[index] = node->values[index + 1];
        }
        node->key_count -= 1;
        node->values[node->key_count] = NULL;
        return true;
    }

    {
        size_t child_index = child_index_for_key(node, key);
        BPlusTreeNode *child = node->children[child_index];

        if (!delete_recursive(tree, child, key, record, removed, error, error_capacity)) {
            return false;
        }

        if (!*removed) {
            return true;
        }

        if (node_underflow(child, false)) {
            rebalance_child(node, child_index);
        }
        refresh_internal_keys(node);
        return true;
    }
}

bool bptree_init(BPlusTree *tree, bool unique_keys, const char *name) {
    if (tree == NULL) {
        return false;
    }

    memset(tree, 0, sizeof(*tree));
    tree->root = allocate_node(true);
    if (tree->root == NULL) {
        return false;
    }
    tree->first_leaf = tree->root;
    tree->unique_keys = unique_keys;
    if (name != NULL) {
        safe_copy(tree->name, sizeof(tree->name), name);
    }
    return true;
}

void bptree_destroy(BPlusTree *tree) {
    if (tree == NULL) {
        return;
    }

    destroy_node(tree->root);
    tree->root = NULL;
    tree->first_leaf = NULL;
    tree->unique_keys = false;
    tree->name[0] = '\0';
}

bool bptree_insert(BPlusTree *tree, int key, struct Record *record, char *error, size_t error_capacity) {
    BPlusInsertResult result;
    BPlusTreeNode *prepared_root = NULL;

    if (tree == NULL || tree->root == NULL || record == NULL) {
        snprintf(error, error_capacity, "B+ Tree insert received a null pointer.");
        return false;
    }

    if (tree->root->key_count == DB_BPTREE_MAX_KEYS) {
        prepared_root = allocate_node(false);
        if (prepared_root == NULL) {
            snprintf(error, error_capacity, "Failed to allocate new B+ Tree root.");
            return false;
        }
    }

    memset(&result, 0, sizeof(result));
    if (!insert_recursive(tree, tree->root, key, record, &result, error, error_capacity)) {
        free(prepared_root);
        return false;
    }

    if (result.split) {
        BPlusTreeNode *new_root = prepared_root;
        new_root->keys[0] = result.promoted_key;
        new_root->children[0] = tree->root;
        new_root->children[1] = result.right;
        new_root->key_count = 1;
        tree->root = new_root;
        refresh_internal_keys(tree->root);
    } else {
        free(prepared_root);
    }

    return true;
}

const BPlusValueNode *bptree_find(const BPlusTree *tree, int key) {
    BPlusTreeNode *leaf;
    size_t index = 0;

    if (tree == NULL || tree->root == NULL) {
        return NULL;
    }

    leaf = find_leaf(tree, key);
    if (leaf == NULL) {
        return NULL;
    }

    while (index < leaf->key_count && leaf->keys[index] < key) {
        ++index;
    }

    if (index == leaf->key_count || leaf->keys[index] != key) {
        return NULL;
    }

    return leaf->values[index];
}

struct Record *bptree_find_first(const BPlusTree *tree, int key) {
    const BPlusValueNode *node = bptree_find(tree, key);
    return node == NULL ? NULL : node->record;
}

bool bptree_remove(BPlusTree *tree, int key, struct Record *record, char *error, size_t error_capacity) {
    bool removed = false;

    if (tree == NULL || tree->root == NULL) {
        snprintf(error, error_capacity, "B+ Tree remove received a null pointer.");
        return false;
    }

    if (!delete_recursive(tree, tree->root, key, record, &removed, error, error_capacity)) {
        return false;
    }

    if (!removed) {
        snprintf(error, error_capacity, "Key=%d was not found in %s B+ Tree.", key, tree->name);
        return false;
    }

    if (!tree->root->is_leaf && tree->root->key_count == 0) {
        BPlusTreeNode *old_root = tree->root;
        tree->root = tree->root->children[0];
        free(old_root);
    }

    if (tree->root->is_leaf) {
        tree->first_leaf = tree->root;
    }

    return true;
}

static bool key_in_range(int key, const BPlusRange *range) {
    if (range == NULL) {
        return true;
    }

    if (range->has_lower) {
        if (key < range->lower) {
            return false;
        }
        if (!range->lower_inclusive && key == range->lower) {
            return false;
        }
    }

    if (range->has_upper) {
        if (key > range->upper) {
            return false;
        }
        if (!range->upper_inclusive && key == range->upper) {
            return false;
        }
    }

    return true;
}

bool bptree_range_scan(const BPlusTree *tree, const BPlusRange *range, BPlusTreeVisitor visitor, void *context, char *error, size_t error_capacity) {
    BPlusTreeNode *leaf;

    if (tree == NULL || tree->root == NULL || visitor == NULL) {
        snprintf(error, error_capacity, "B+ Tree range scan received a null pointer.");
        return false;
    }

    if (range != NULL && range->has_lower) {
        leaf = find_leaf(tree, range->lower);
    } else {
        leaf = tree->first_leaf;
    }

    while (leaf != NULL) {
        size_t index;
        for (index = 0; index < leaf->key_count; ++index) {
            BPlusValueNode *value;

            if (!key_in_range(leaf->keys[index], range)) {
                if (range != NULL && range->has_upper && leaf->keys[index] > range->upper) {
                    return true;
                }
                continue;
            }

            value = leaf->values[index];
            while (value != NULL) {
                if (!visitor(value->record, context)) {
                    return true;
                }
                value = value->next;
            }
        }
        leaf = leaf->next;
    }

    return true;
}

bool bptree_traverse_all(const BPlusTree *tree, BPlusTreeVisitor visitor, void *context, char *error, size_t error_capacity) {
    return bptree_range_scan(tree, NULL, visitor, context, error, error_capacity);
}

size_t bptree_estimate_memory(const BPlusTree *tree) {
    if (tree == NULL) {
        return 0;
    }

    return estimate_node_memory(tree->root);
}

static size_t count_value_list_nodes(const BPlusValueNode *node) {
    size_t count = 0;

    while (node != NULL) {
        count += 1;
        node = node->next;
    }

    return count;
}

static void collect_node_stats(const BPlusTreeNode *node, size_t depth, BPlusTreeStats *stats) {
    size_t index;

    if (node == NULL || stats == NULL) {
        return;
    }

    stats->node_count += 1;
    if (depth > stats->depth) {
        stats->depth = depth;
    }

    if (node->is_leaf) {
        stats->leaf_node_count += 1;
        stats->key_count += node->key_count;
        for (index = 0; index < node->key_count; ++index) {
            stats->value_count += count_value_list_nodes(node->values[index]);
        }
        return;
    }

    stats->internal_node_count += 1;
    stats->key_count += node->key_count;
    for (index = 0; index <= node->key_count; ++index) {
        collect_node_stats(node->children[index], depth + 1, stats);
    }
}

void bptree_collect_stats(const BPlusTree *tree, BPlusTreeStats *stats) {
    const BPlusTreeNode *leaf;

    if (stats == NULL) {
        return;
    }

    memset(stats, 0, sizeof(*stats));
    if (tree == NULL || tree->root == NULL) {
        return;
    }

    collect_node_stats(tree->root, 1, stats);

    leaf = tree->first_leaf;
    while (leaf != NULL) {
        stats->leaf_chain_length += 1;
        leaf = leaf->next;
    }
}
