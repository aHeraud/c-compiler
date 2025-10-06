#include <assert.h>
#include <stdbool.h>
#include <malloc.h>
#include <string.h>
#include "utils/hashtable.h"

struct HashTableEntry {
    // Doubly linked list of all entries in the table
    struct HashTableEntry *prev_entry, *next_entry;
    // Doubly linked list of entries in the same bucket (all share the same hash code)
    struct HashTableEntry *prev_bucket, *next_bucket;
    size_t hashcode;
    const void *key;
    void *value;
};

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
        .buckets = calloc(num_buckets, sizeof(hash_table_entry_t*)),
        .head = NULL,
        .tail = NULL,
    };
}

void hash_table_destroy(hash_table_t* table) {
    for (size_t i = 0; i < table->num_buckets; i++) {
        hash_table_entry_t* entry = table->buckets[i];
        while (entry != NULL) {
            hash_table_entry_t* next = entry->next_bucket;
            free(entry);
            entry = next;
        }
    }
    free(table->buckets);
    table->buckets = NULL;
    table->num_buckets = 0;
    table->size = 0;
}

void hash_table_insert(hash_table_t* table, const void* key, void* value) {
    assert(table != NULL && table->num_buckets > 0);
    size_t hashcode = table->hash_fn(key);
    size_t bucket_index = hashcode % table->num_buckets;
    hash_table_entry_t* bucket_head = table->buckets[bucket_index];

    hash_table_entry_t *entry = NULL;

    if (bucket_head == NULL) {
        bucket_head = malloc(sizeof(hash_table_entry_t));
        *bucket_head = (hash_table_entry_t) {
            .prev_bucket = NULL,
            .next_bucket = NULL,
            .prev_entry = NULL,
            .next_entry = NULL,
            .hashcode = hashcode,
            .key = key,
            .value = value,
        };
        table->buckets[bucket_index] = bucket_head;
        table->size += 1;
        entry = bucket_head;
    } else {
        hash_table_entry_t* prev = NULL;
        entry = bucket_head;
        while (entry != NULL) {
            if (entry->hashcode == hashcode && table->equals_fn(entry->key, key)) {
                // update in place
                entry->value = value;
                return;
            }
            prev = entry;
            entry = entry->next_bucket;
        }
        entry = malloc(sizeof(hash_table_entry_t));
        *entry = (hash_table_entry_t) {
            .prev_bucket = prev,
            .next_bucket = NULL,
            .prev_entry = NULL,
            .next_entry = NULL,
            .hashcode = hashcode,
            .key = key,
            .value = value,
        };
        prev->next_bucket = entry;
        table->size += 1;
    }

    // Update the linked list of all entries
    if (table->tail == NULL) {
        table->head = entry;
        table->tail = entry;
    } else {
        entry->prev_entry = table->tail;
        table->tail->next_entry = entry;
        table->tail = entry;
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
    if (table->num_buckets == 0) return false;
    size_t hashcode = table->hash_fn(key);
    size_t index = hashcode % table->num_buckets;
    hash_table_entry_t* entry = table->buckets[index];
    while (entry != NULL) {
        if (entry->hashcode == hashcode && table->equals_fn(entry->key, key)) {
            if (value != NULL) {
                *value = entry->value;
            }
            return true;
        }
        entry = entry->next_bucket;
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
    if (table->num_buckets == 0) return false;
    size_t hashcode = table->hash_fn(key);
    size_t index = hashcode % table->num_buckets;
    hash_table_entry_t* entry = table->buckets[index];
    while (entry != NULL) {
        if (entry->hashcode == hashcode && table->equals_fn(entry->key, key)) {
            if (entry->prev_bucket != NULL) {
                entry->prev_bucket->next_bucket = entry->next_bucket;
            } else {
                table->buckets[index] = entry->next_bucket;
            }

            if (entry->next_bucket != NULL) {
                entry->next_bucket->prev_bucket = entry->prev_bucket;
            }

            if (value != NULL) {
                *value = entry->value;
            }

            // Update the linked list of all entries
            if (entry->prev_entry != NULL) {
                entry->prev_entry->next_entry = entry->next_entry;
            } else {
                table->head = entry->next_entry;
            }
            if (entry->next_entry != NULL) {
                entry->next_entry->prev_entry = entry->prev_entry;
            } else {
                table->tail = entry->prev_entry;
            }

            free(entry);
            entry = NULL;
            table->size -= 1;
            return true;
        } else {
            entry = entry->next_bucket;
        }
    }

    return false;
}

const hash_table_entry_t *hash_table_get_iterator(const hash_table_t *table) {
    assert(table != NULL);
    return table->head;
}

const hash_table_entry_t *hash_table_iterator_next(const hash_table_entry_t *entry) {
    assert(entry != NULL);
    return entry->next_entry;
}

const void* hash_table_entry_get_key(const hash_table_entry_t *entry) {
    assert(entry != NULL);
    return entry->key;
}

void* hash_table_entry_get_value(const hash_table_entry_t *entry) {
    assert(entry != NULL);
    return entry->value;
}
