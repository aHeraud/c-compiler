#include <assert.h>
#include <stdbool.h>
#include <malloc.h>
#include <string.h>
#include "hashtable.h"

size_t hash_key(const char* key) {
    size_t hash = 0;
    for (size_t i = 0; key[i] != '\0'; i++) {
        char c = key[i];
        hash = hash * 31 + c;
    }
    return hash;
}

hash_table_t hash_table_create(size_t num_buckets) {
    return (hash_table_t) {
            .num_buckets = num_buckets,
            .size = 0,
            .buckets = calloc(num_buckets, sizeof(hashtable_entry_t*)),
    };
}

void hash_table_destroy(hash_table_t* table) {
    for (size_t i = 0; i < table->num_buckets; i++) {
        hashtable_entry_t* entry = table->buckets[i];
        while (entry != NULL) {
            hashtable_entry_t* next = entry->next;
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
bool hash_table_insert(hash_table_t* table, const char* key, void* value) {
    assert(table != NULL && table->num_buckets > 0);
    size_t bucket_index = hash_key(key) % table->num_buckets;
    hashtable_entry_t* bucket_head = table->buckets[bucket_index];
    if (bucket_head == NULL) {
        bucket_head = malloc(sizeof(hashtable_entry_t));
        *bucket_head = (hashtable_entry_t) {
                .prev = NULL,
                .next = NULL,
                .key = key,
                .value = value,
        };
        table->buckets[bucket_index] = bucket_head;
        table->size += 1;
        return true;
    } else {
        hashtable_entry_t* prev = NULL;
        hashtable_entry_t* entry = bucket_head;
        while (entry != NULL) {
            if (strcmp(entry->key, key) == 0) {
                return false;
            }
            prev = entry;
            entry = entry->next;
        }
        entry = malloc(sizeof(hashtable_entry_t));
        *entry = (hashtable_entry_t) {
                .prev = prev,
                .next = NULL,
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
bool hash_table_lookup(const hash_table_t* table, const char* key, void** value) {
    size_t index = hash_key(key) % table->num_buckets;
    hashtable_entry_t* entry = table->buckets[index];
    while (entry != NULL) {
        if (strcmp(entry->key, key) == 0) {
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
bool hash_table_remove(hash_table_t* table, const char* key, void** value) {
    size_t index = hash_key(key) % table->num_buckets;
    hashtable_entry_t* entry = table->buckets[index];
    while (entry != NULL) {
        if (strcmp(entry->key, key) == 0) {
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
