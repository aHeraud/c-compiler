#include "CUnit/Basic.h"
#include "util/hashtable.h"

void test_hashtable_insert() {
    // base case, insert into empty hashtable that has one bucket
    hash_table_t table = hash_table_create_string_keys(1);

    int value = 42;
    CU_ASSERT_TRUE(hash_table_insert(&table, "key", (void *) &value));

    // verify that the entry was inserted correctly
    CU_ASSERT_PTR_NOT_NULL(table.buckets[0]);
    CU_ASSERT_PTR_NULL(table.buckets[0]->prev);
    CU_ASSERT_PTR_NULL(table.buckets[0]->next);
    CU_ASSERT_STRING_EQUAL(table.buckets[0]->key, "key");
    CU_ASSERT_PTR_EQUAL(table.buckets[0]->value, &value);
    CU_ASSERT_EQUAL(table.size, 1);

    // insert another entry into the same bucket
    int value2 = 43;
    CU_ASSERT_TRUE(hash_table_insert(&table, "key2", (void *) &value2));

    // verify that the first entry hasn't been modified, other than the next pointer
    CU_ASSERT_PTR_NOT_NULL(table.buckets[0]);
    CU_ASSERT_PTR_NULL(table.buckets[0]->prev);
    CU_ASSERT_PTR_NOT_NULL(table.buckets[0]->next);
    CU_ASSERT_STRING_EQUAL(table.buckets[0]->key, "key");
    CU_ASSERT_PTR_EQUAL(table.buckets[0]->value, &value);

    // verify that the second entry was inserted correctly
    CU_ASSERT_PTR_EQUAL(table.buckets[0]->next->prev, table.buckets[0]);
    CU_ASSERT_PTR_NULL(table.buckets[0]->next->next);
    CU_ASSERT_STRING_EQUAL(table.buckets[0]->next->key, "key2");
    CU_ASSERT_PTR_EQUAL(table.buckets[0]->next->value, &value2);
    CU_ASSERT_EQUAL(table.size, 2);

    // attempt to insert an entry with a duplicate key
    int value3 = 44;
    CU_ASSERT_FALSE(hash_table_insert(&table, "key", (void *) &value3));
    CU_ASSERT_EQUAL(table.buckets[0]->value, &value);
    CU_ASSERT_EQUAL(table.size, 2);
}

void test_hashtable_lookup() {
    hash_table_t table = hash_table_create_string_keys(1);

    // base case, lookup in empty hashtable
    int* value;
    CU_ASSERT_FALSE(hash_table_lookup(&table, "key", (void **) &value));

    // insert an entry into the hashtable
    int value1 = 1;
    CU_ASSERT_TRUE(hash_table_insert(&table, "key", (void *) &value1));

    // lookup the entry
    CU_ASSERT_TRUE(hash_table_lookup(&table, "key", (void **) &value));
    CU_ASSERT_EQUAL(*value, value1);

    // insert another entry into the same bucket
    int value2 = 2;
    CU_ASSERT_TRUE(hash_table_insert(&table, "key2", (void *) &value2));

    // lookup the second entry
    CU_ASSERT_TRUE(hash_table_lookup(&table, "key2", (void **) &value));
    CU_ASSERT_EQUAL(*value, value2);
}

void test_hashtable_remove() {
    hash_table_t table = hash_table_create_string_keys(1);

    // base case, remove from empty hashtable
    int* value;
    CU_ASSERT_FALSE(hash_table_remove(&table, "key", (void **) &value));
    CU_ASSERT_EQUAL(table.size, 0);

    // insert an entry into the hashtable
    int value1 = 1;
    CU_ASSERT_TRUE(hash_table_insert(&table, "key", (void *) &value1));

    // remove the entry
    CU_ASSERT_TRUE(hash_table_remove(&table, "key", (void **) &value));
    CU_ASSERT_EQUAL(*value, value1);
    // verify that the entry was removed
    CU_ASSERT_PTR_NULL(table.buckets[0]);
    CU_ASSERT_EQUAL(table.size, 0);

    // insert the entry again
    CU_ASSERT_TRUE(hash_table_insert(&table, "key", (void *) &value1));
    CU_ASSERT_EQUAL(table.size, 1);

    // insert another entry into the same bucket
    int value2 = 2;
    CU_ASSERT_TRUE(hash_table_insert(&table, "key2", (void *) &value2));
    CU_ASSERT_EQUAL(table.size, 2);

    // remove the second entry
    CU_ASSERT_TRUE(hash_table_remove(&table, "key2", (void **) &value));
    CU_ASSERT_EQUAL(*value, value2);
    CU_ASSERT_EQUAL(table.size, 1);

    // verify that the first entry hasn't been modified, other than the next pointer
    CU_ASSERT_PTR_NOT_NULL(table.buckets[0]);
    CU_ASSERT_PTR_NULL(table.buckets[0]->prev);
    CU_ASSERT_PTR_NULL(table.buckets[0]->next);
    CU_ASSERT_STRING_EQUAL(table.buckets[0]->key, "key");
    CU_ASSERT_PTR_EQUAL(table.buckets[0]->value, &value1);
}

int hashtable_tests_init_suite() {
    CU_pSuite pSuite = CU_add_suite("hashtable", NULL, NULL);
    if (NULL == CU_add_test(pSuite, "insert", test_hashtable_insert) ||
        NULL == CU_add_test(pSuite, "lookup", test_hashtable_lookup) ||
        NULL == CU_add_test(pSuite, "remove", test_hashtable_remove)) {
        CU_cleanup_registry();
        return CU_get_error();
    }
    return 0;
}
