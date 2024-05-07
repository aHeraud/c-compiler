#ifndef C_COMPILER_HASHTABLE_H
#define C_COMPILER_HASHTABLE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct HashTableEntry {
    struct HashTableEntry* prev;
    struct HashTableEntry* next;
    const char* key;
    void* value;
} hashtable_entry_t;

/**
 * A simple hashtable.
 * Keys are strings, values are void pointers.
 * Each bucket is a linked list of entries.
 */
typedef struct HashTable {
    size_t num_buckets;
    size_t size;
    hashtable_entry_t** buckets;
} hash_table_t;

hash_table_t hash_table_create(size_t num_buckets);
void hash_table_destroy(hash_table_t* table);
bool hash_table_insert(hash_table_t* table, const char* key, void* value);
bool hash_table_lookup(const hash_table_t* table, const char* key, void** value);
bool hash_table_remove(hash_table_t* table, const char* key, void** value);

#endif //C_COMPILER_HASHTABLE_H
