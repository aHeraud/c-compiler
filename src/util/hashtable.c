#include <assert.h>
#include <stdbool.h>
#include <malloc.h>
#include <string.h>
#include "hashtable.h"

size_t hashtable_string_hash_key(const char* key) {
    size_t hash = 0;
    for (size_t i = 0; key[i] != '\0'; i++) {
        char c = key[i];
        hash = hash * 31 + c;
    }
    return hash;
}

bool hashtable_string_equals(const char* a, const char* b) {
    return strcmp(a, b) == 0;
}

size_t hashtable_ptr_hash_key(const void* key) {
    return (size_t) key;
}

bool hashtable_ptr_equals(const void* a, const void* b) {
    return a == b;
}

hash_table_t hash_table_create_string_keys(size_t num_buckets) {
    return hash_table_create(num_buckets, (hashtable_hash_fn_t) hashtable_string_hash_key,
                             (hashtable_equals_fn_t) hashtable_string_equals);
}

hash_table_t hash_table_create_pointer_keys(size_t num_buckets) {
    return hash_table_create(num_buckets, (hashtable_hash_fn_t) hashtable_ptr_hash_key,
                             (hashtable_equals_fn_t) hashtable_ptr_equals);
}

hash_table_t hash_table_create(size_t num_buckets, hashtable_hash_fn_t hash_fn, hashtable_equals_fn_t equals_fn) {
    return (hash_table_t) {
            .num_buckets = num_buckets,
            .size = 0,
            .hash_fn = hash_fn,
            .equals_fn = equals_fn,
            .buckets = calloc(num_buckets, sizeof(hash_table_bucket_t*)),
    };
}

void hash_table_destroy(hash_table_t* table) {
    for (size_t i = 0; i < table->num_buckets; i++) {
        hash_table_bucket_t* entry = table->buckets[i];
        while (entry != NULL) {
            hash_table_bucket_t* next = entry->next;
            free(entry);
            entry = next;
        }
    }
    free(table->buckets);
    table->buckets = NULL;
    table->num_buckets = 0;
    table->size = 0;
}

/**
 * Insert a key-value pair into a hashtable.
 * If the key already exists, the value is not changed, and the function returns false.
 *
 * @param table
 * @param key
 * @param value
 * @return true if the key-value pair was inserted, false if the key already exists
 */
bool hash_table_insert(hash_table_t* table, const void* key, void* value) {
    assert(table != NULL && table->num_buckets > 0);
    size_t hashcode = table->hash_fn(key);
    size_t bucket_index = hashcode % table->num_buckets;
    hash_table_bucket_t* bucket_head = table->buckets[bucket_index];
    if (bucket_head == NULL) {
        bucket_head = malloc(sizeof(hash_table_bucket_t));
        *bucket_head = (hash_table_bucket_t) {
                .prev = NULL,
                .next = NULL,
                .hashcode = hashcode,
                .key = key,
                .value = value,
        };
        table->buckets[bucket_index] = bucket_head;
        table->size += 1;
        return true;
    } else {
        hash_table_bucket_t* prev = NULL;
        hash_table_bucket_t* entry = bucket_head;
        while (entry != NULL) {
            if (entry->hashcode == hashcode && table->equals_fn(entry->key, key)) {
                return false;
            }
            prev = entry;
            entry = entry->next;
        }
        entry = malloc(sizeof(hash_table_bucket_t));
        *entry = (hash_table_bucket_t) {
                .prev = prev,
                .next = NULL,
                .hashcode = hashcode,
                .key = key,
                .value = value,
        };
        prev->next = entry;
        table->size += 1;
        return true;
    }
}

/**
 * Lookup an entry in a hashtable.
 * @param table hashtable
 * @param key key to lookup
 * @param value if not NULL, the value associated with the key will be written to this pointer if present in the table
 * @return true if the value was found, false otherwise
 */
bool hash_table_lookup(const hash_table_t* table, const void* key, void** value) {
    size_t hashcode = table->hash_fn(key);
    size_t index = hashcode % table->num_buckets;
    hash_table_bucket_t* entry = table->buckets[index];
    while (entry != NULL) {
        if (entry->hashcode == hashcode && table->equals_fn(entry->key, key)) {
            if (value != NULL) {
                *value = entry->value;
            }
            return true;
        }
        entry = entry->next;
    }
    return false;
}

/**
 * Remove an entry from a hashtable.
 * @param table hashtable
 * @param key key identifying the entry to remove
 * @param value if not NULL, the value associated with the key will be written to this pointer if present in the table
 * @return true if the value was found, false otherwise
 */
bool hash_table_remove(hash_table_t* table, const void* key, void** value) {
    size_t hashcode = table->hash_fn(key);
    size_t index = hashcode % table->num_buckets;
    hash_table_bucket_t* entry = table->buckets[index];
    while (entry != NULL) {
        if (entry->hashcode == hashcode && table->equals_fn(entry->key, key)) {
            if (entry->prev != NULL) {
                entry->prev->next = entry->next;
            } else {
                table->buckets[index] = entry->next;
            }

            if (entry->next != NULL) {
                entry->next->prev = entry->prev;
            }

            if (value != NULL) {
                *value = entry->value;
            }

            free(entry);
            entry = NULL;
            table->size -= 1;
            return true;
        } else {
            entry = entry->next;
        }
    }

    return false;
}
