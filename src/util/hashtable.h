#ifndef C_COMPILER_HASHTABLE_H
#define C_COMPILER_HASHTABLE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct HashTableBucket {
    struct HashTableBucket* prev;
    struct HashTableBucket* next;
    size_t hashcode;
    const void* key;
    void* value;
} hash_table_bucket_t;

typedef size_t (*hashtable_hash_fn_t)(const void* key);
typedef bool (*hashtable_equals_fn_t)(const void* a, const void* b);

/**
 * A simple hashtable.
 * Keys and values are void pointers, and the keys are compared using the provided hash and equals functions.
 * Each bucket is a linked list of entries.
 */
typedef struct HashTable {
    size_t num_buckets;
    size_t size;
    hashtable_hash_fn_t hash_fn;
    hashtable_equals_fn_t equals_fn;
    hash_table_bucket_t** buckets;
} hash_table_t;

/**
 * Create a hash table whose keys are strings.
 * @param num_buckets number of buckets in the hash table
 * @return An empty hash table
 */
hash_table_t hash_table_create_string_keys(size_t num_buckets);

/**
 * Create a hash table with the specified number of buckets, hash function, and equals function.
 * @param num_buckets number of buckets in the hash table
 * @param hash_fn     key hash function
 * @param equals_fn   key equality function (returns true if two keys are equal)
 * @return An empty hash table
 */
hash_table_t hash_table_create(size_t num_buckets, hashtable_hash_fn_t hash_fn, hashtable_equals_fn_t equals_fn);
void hash_table_destroy(hash_table_t* table);
bool hash_table_insert(hash_table_t* table, const void* key, void* value);
bool hash_table_lookup(const hash_table_t* table, const void* key, void** value);
bool hash_table_remove(hash_table_t* table, const void* key, void** value);

#endif //C_COMPILER_HASHTABLE_H
