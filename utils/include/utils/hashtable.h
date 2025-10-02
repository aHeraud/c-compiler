#ifndef C_COMPILER_HASHTABLE_H
#define C_COMPILER_HASHTABLE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct HashTableEntry hash_table_entry_t;

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
    hash_table_entry_t** buckets;
    /**
     * Linked list of entries, for easy iteration over all entries in the table.
     */
    hash_table_entry_t *head, *tail;
} hash_table_t;

/**
 * Create a hash table whose keys are strings.
 * @param num_buckets number of buckets in the hash table
 * @return An empty hash table
 */
hash_table_t hash_table_create_string_keys(size_t num_buckets);

/**
 * Create a hash table whose keys are pointers.
 * @param num_buckets number of buckets in the hash table
 * @return An empty hash table
 */
hash_table_t hash_table_create_pointer_keys(size_t num_buckets);

/**
 * Create a hash table with the specified number of buckets, hash function, and equals function.
 * @param num_buckets number of buckets in the hash table
 * @param hash_fn     key hash function
 * @param equals_fn   key equality function (returns true if two keys are equal)
 * @return An empty hash table
 */
hash_table_t hash_table_create(size_t num_buckets, hashtable_hash_fn_t hash_fn, hashtable_equals_fn_t equals_fn);

/**
 * Destroy a hash table, freeing all memory associated with it.
 * This does not free the keys or values stored in the table, or the memory allocated to the hash table struct itself.
 * @param table pointer to a hash table to destroy
 */
void hash_table_destroy(hash_table_t* table);

/**
 * Insert a key-value pair into a hashtable. If the key is already present, the existing value will be replaced.
 *
 * @param table
 * @param key
 * @param value
 */
void hash_table_insert(hash_table_t* table, const void* key, void* value);

bool hash_table_lookup(const hash_table_t* table, const void* key, void** value);

bool hash_table_remove(hash_table_t* table, const void* key, void** value);

/**
 * Get an iterator that points to the first entry in the hash table.
 * If the table is modified after creating the iterator, the behavior of the functions that use it is undefined.
 * @param table pointer to a hash table to get an iterator for
 * @return pointer to the first entry in the hash table, or NULL if the table is empty
 */
const hash_table_entry_t *hash_table_get_iterator(const hash_table_t *table);

/**
 * Advance the iterator to the next entry in the hash table.
 * @param entry iterator pointing to the current entry
 * @return pointer to the next entry in the hash table, or NULL if there are no more entries
 */
const hash_table_entry_t *hash_table_iterator_next(const hash_table_entry_t *entry);

const void* hash_table_entry_get_key(const hash_table_entry_t *entry);
void* hash_table_entry_get_value(const hash_table_entry_t *entry);

#endif //C_COMPILER_HASHTABLE_H
