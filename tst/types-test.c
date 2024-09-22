#include "CUnit/Basic.h"

#include "types.h"
#include "tests.h"
#include "test-common.h"

void test_small_integer_promotion() {
    const type_t *types[] = {
            &CHAR,
            &UNSIGNED_CHAR,
            &SHORT,
            &UNSIGNED_SHORT,
            &INT,
    };

    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        const type_t *type = types[i];
        const type_t *promoted = type_after_integer_promotion(type);
        CU_ASSERT_TRUE(is_integer_type(promoted));
        CU_ASSERT_TRUE(types_equal(promoted, &INT));
    }
}

void test_get_common_type_int_int() {
    const type_t *common = get_common_type(&INT, &INT);
    CU_ASSERT_TRUE(types_equal(common, &INT));
}

void test_get_common_type_int_long() {
    const type_t *common = get_common_type(&INT, &LONG);
    CU_ASSERT_TRUE(types_equal(common, &LONG));
}

void test_get_common_type_unsigned_int_int() {
    const type_t *common = get_common_type(&UNSIGNED_INT, &INT);
    CU_ASSERT_TRUE(types_equal(common, &UNSIGNED_INT));
}

void test_get_common_type_short_char() {
    const type_t *common = get_common_type(&SHORT, &CHAR);
    CU_ASSERT_TRUE(types_equal(common, &INT));
}

void test_get_common_type_float_double() {
    const type_t *common = get_common_type(&FLOAT, &DOUBLE);
    CU_ASSERT_TRUE(types_equal(common, &DOUBLE));
}

void test_get_common_type_double_long_double() {
    const type_t *common = get_common_type(&DOUBLE, &LONG_DOUBLE);
    CU_ASSERT_TRUE(types_equal(common, &LONG_DOUBLE));
}

void test_get_common_type_int_float() {
    const type_t *common = get_common_type(&INT, &FLOAT);
    CU_ASSERT_TRUE(types_equal(common, &FLOAT));
}

void test_type_equality_int_int() {
    CU_ASSERT_TRUE(types_equal(&INT, &INT));
}

void test_type_equality_int_unsigned_int() {
    CU_ASSERT_FALSE(types_equal(&INT, &UNSIGNED_INT));
}

void test_type_equality_int_long() {
    CU_ASSERT_FALSE(types_equal(&INT, &LONG));
}

void test_type_equality_float_float() {
    CU_ASSERT_TRUE(types_equal(&FLOAT, &FLOAT));
}

void test_type_equality_float_double() {
    CU_ASSERT_FALSE(types_equal(&FLOAT, &DOUBLE));
}

void test_type_equality_int_float() {
    CU_ASSERT_FALSE(types_equal(&INT, &FLOAT));
}

void test_type_equality_int_ptr_int_ptr() {
    const type_t *int_ptr = ptr_to(&INT);
    const type_t *int_ptr2 = ptr_to(&INT);
    CU_ASSERT_TRUE(types_equal(int_ptr, int_ptr2));
}

void test_type_equality_int_ptr_float_ptr() {
    const type_t *int_ptr = ptr_to(&INT);
    const type_t *float_ptr = ptr_to(&FLOAT);
    CU_ASSERT_FALSE(types_equal(int_ptr, float_ptr));
}

void test_type_equality_int_array_10_int_array_10() {
    // same size and type = equal
    const type_t *int_array = array_of(&INT, integer_constant("10"));
    const type_t *int_array2 = array_of(&INT, integer_constant("10"));
    CU_ASSERT_TRUE(types_equal(int_array, int_array2));
}

void test_type_equality_int_array_1_int_array_2() {
    // different sizes = not equal
    const type_t *int_array = array_of(&INT, integer_constant("1"));
    const type_t *int_array2 = array_of(&INT, integer_constant("2"));
    CU_ASSERT_FALSE(types_equal(int_array, int_array2));
}

void test_type_equality_int_array_10_float_array_10() {
    // different element types = not equal
    const type_t *int_array = array_of(&INT, integer_constant("10"));
    const type_t *float_array = array_of(&FLOAT, integer_constant("10"));
    CU_ASSERT_FALSE(types_equal(int_array, float_array));
}

int types_tests_init_suite() {
    CU_pSuite pSuite = CU_add_suite("types", NULL, NULL);
    if (NULL == CU_add_test(pSuite, "small integer promotion", test_small_integer_promotion) ||
        NULL == CU_add_test(pSuite, "get common type (int, int)", test_get_common_type_int_int) ||
        NULL == CU_add_test(pSuite, "get common type (int, long)", test_get_common_type_int_long) ||
        NULL == CU_add_test(pSuite, "get common type (unsigned int, int)", test_get_common_type_unsigned_int_int) ||
        NULL == CU_add_test(pSuite, "get common type (short, char)", test_get_common_type_short_char) ||
        NULL == CU_add_test(pSuite, "get common type (float, double)", test_get_common_type_float_double) ||
        NULL == CU_add_test(pSuite, "get common type (double, long double)", test_get_common_type_double_long_double) ||
        NULL == CU_add_test(pSuite, "get common type (int, float)", test_get_common_type_int_float) ||
        NULL == CU_add_test(pSuite, "type equality (int, int)", test_type_equality_int_int) ||
        NULL == CU_add_test(pSuite, "type equality (int, unsigned int)", test_type_equality_int_unsigned_int) ||
        NULL == CU_add_test(pSuite, "type equality (int, long)", test_type_equality_int_long) ||
        NULL == CU_add_test(pSuite, "type equality (float, float)", test_type_equality_float_float) ||
        NULL == CU_add_test(pSuite, "type equality (float, double)", test_type_equality_float_double) ||
        NULL == CU_add_test(pSuite, "type equality (int, float)", test_type_equality_int_float) ||
        NULL == CU_add_test(pSuite, "type equality (int*, int*)", test_type_equality_int_ptr_int_ptr) ||
        NULL == CU_add_test(pSuite, "type equality (int*, float*)", test_type_equality_int_ptr_float_ptr) ||
        NULL == CU_add_test(pSuite, "type equality (int[10], int[10])", test_type_equality_int_array_10_int_array_10) ||
        NULL == CU_add_test(pSuite, "type equality (int[1], int[2])", test_type_equality_int_array_1_int_array_2) ||
        NULL == CU_add_test(pSuite, "type equality (int[10], float[10])", test_type_equality_int_array_10_float_array_10)
    ) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    return 0;
}
