#include "CUnit/Basic.h"
#include "utils/hashtable.h"

void test_hashtable_insert(void) {
    // base case, insert into empty hashtable that has one bucket
    hash_table_t table = hash_table_create_string_keys(1);

    int value = 42;
    hash_table_insert(&table, "key1", (void *) &value);

    // verify that the entry was inserted correctly
    CU_ASSERT_PTR_NOT_NULL(table.buckets[0]);
    CU_ASSERT_EQUAL_FATAL(table.size, 1);
    int *res = NULL;
    hash_table_lookup(&table, "key1", (void *) &res);
    CU_ASSERT_EQUAL_FATAL(value, *res);

    // insert another entry into the same bucket
    int value2 = 43;
    hash_table_insert(&table, "key2", (void *) &value2);

    // verify that the first entry hasn't been modified, other than the next pointer
    CU_ASSERT_PTR_NOT_NULL_FATAL(table.buckets[0]);
    hash_table_lookup(&table, "key1", (void *) &res);
    CU_ASSERT_EQUAL_FATAL(value, *res);

    // verify that the second entry was inserted correctly
    CU_ASSERT_EQUAL_FATAL(table.size, 2);
    hash_table_lookup(&table, "key2", (void *) &res);
    CU_ASSERT_EQUAL_FATAL(value2, *res);

    // attempt to insert an entry with a duplicate key
    int value3 = 44;
    hash_table_insert(&table, "key1", (void *) &value3);
    hash_table_lookup(&table, "key1", (void *) &res);
    CU_ASSERT_EQUAL_FATAL(value3, *res);
    CU_ASSERT_EQUAL_FATAL(table.size, 2);
}

void test_hashtable_lookup(void) {
    hash_table_t table = hash_table_create_string_keys(1);

    // base case, lookup in empty hashtable
    int* value;
    CU_ASSERT_FALSE(hash_table_lookup(&table, "key", (void **) &value));

    // insert an entry into the hashtable
    int value1 = 1;
    hash_table_insert(&table, "key", (void *) &value1);

    // lookup the entry
    CU_ASSERT_TRUE(hash_table_lookup(&table, "key", (void **) &value));
    CU_ASSERT_EQUAL(*value, value1);

    // insert another entry into the same bucket
    int value2 = 2;
    hash_table_insert(&table, "key2", (void *) &value2);

    // lookup the second entry
    CU_ASSERT_TRUE(hash_table_lookup(&table, "key2", (void **) &value));
    CU_ASSERT_EQUAL(*value, value2);
}

void test_hashtable_remove(void) {
    hash_table_t table = hash_table_create_string_keys(1);

    // base case, remove from empty hashtable
    int* value;
    CU_ASSERT_FALSE(hash_table_remove(&table, "key", (void **) &value));
    CU_ASSERT_EQUAL(table.size, 0);

    // insert an entry into the hashtable
    int value1 = 1;
    hash_table_insert(&table, "key", (void *) &value1);

    // remove the entry
    CU_ASSERT_TRUE(hash_table_remove(&table, "key", (void **) &value));
    CU_ASSERT_EQUAL(*value, value1);
    // verify that the entry was removed
    CU_ASSERT_PTR_NULL(table.buckets[0]);
    CU_ASSERT_EQUAL(table.size, 0);

    // insert the entry again
    hash_table_insert(&table, "key", (void *) &value1);
    CU_ASSERT_EQUAL(table.size, 1);

    // insert another entry into the same bucket
    int value2 = 2;
    hash_table_insert(&table, "key2", (void *) &value2);
    CU_ASSERT_EQUAL(table.size, 2);

    // remove the second entry
    CU_ASSERT_TRUE(hash_table_remove(&table, "key2", (void **) &value));
    CU_ASSERT_EQUAL(*value, value2);
    CU_ASSERT_EQUAL(table.size, 1);

    // verify that the first entry hasn't been modified
    CU_ASSERT_PTR_NOT_NULL(table.buckets[0]);
    hash_table_lookup(&table, "key", (void*) &value);
    CU_ASSERT_EQUAL(value1, *value);
}

void test_hashtable_iterator(void) {
    // Create a hash table and populate it with some values
    hash_table_t table = hash_table_create_string_keys(32);
    const char *keys[7] = {"one", "two", "three", "four", "five"};
    int values[7] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++) {
        hash_table_insert(&table, keys[i], &values[i]);
    }
    CU_ASSERT_EQUAL_FATAL(table.size, 5);

    // Iterate over the hash table and verify that all entries are present
    const hash_table_entry_t *iterator = hash_table_get_iterator(&table);
    for (int i = 0; i < 5; i += 1) {
        CU_ASSERT_PTR_NOT_NULL_FATAL(iterator);
        CU_ASSERT_STRING_EQUAL(keys[i], (const char *) hash_table_entry_get_key(iterator));
        int *value = (int *) hash_table_entry_get_value(iterator);
        CU_ASSERT_PTR_NOT_NULL_FATAL(value);
        CU_ASSERT_EQUAL_FATAL(values[i], *value);
        iterator = hash_table_iterator_next(iterator);
    }
    CU_ASSERT_PTR_NULL(iterator);

    // Remove the first, last, and middle node
    hash_table_remove(&table, keys[0], NULL);
    hash_table_remove(&table, keys[2], NULL);
    hash_table_remove(&table, keys[4], NULL);

    // Verify that the iterator still works correctly
    iterator = hash_table_get_iterator(&table);
    CU_ASSERT_PTR_NOT_NULL_FATAL(iterator);
    CU_ASSERT_STRING_EQUAL(keys[1], (const char *) hash_table_entry_get_key(iterator));
    int *value = (int *) hash_table_entry_get_value(iterator);
    CU_ASSERT_PTR_NOT_NULL_FATAL(value);
    CU_ASSERT_EQUAL_FATAL(values[1], *value);

    iterator = hash_table_iterator_next(iterator);
    CU_ASSERT_PTR_NOT_NULL_FATAL(iterator);
    CU_ASSERT_STRING_EQUAL(keys[3], (const char *) hash_table_entry_get_key(iterator));
    value = (int *) hash_table_entry_get_value(iterator);
    CU_ASSERT_PTR_NOT_NULL_FATAL(value);
    CU_ASSERT_EQUAL_FATAL(values[3], *value);

    CU_ASSERT_PTR_NULL_FATAL(hash_table_iterator_next(iterator));
}

int hashtable_tests_init_suite(void) {
    CU_pSuite pSuite = CU_add_suite("hashtable", NULL, NULL);
    if (NULL == CU_add_test(pSuite, "insert", test_hashtable_insert) ||
        NULL == CU_add_test(pSuite, "lookup", test_hashtable_lookup) ||
        NULL == CU_add_test(pSuite, "remove", test_hashtable_remove) ||
        NULL == CU_add_test(pSuite, "iterator", test_hashtable_iterator)) {
        CU_cleanup_registry();
        return CU_get_error();
    }
    return 0;
}
