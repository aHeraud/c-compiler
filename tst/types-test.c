#include "CUnit/Basic.h"

#include "types.h"
#include "tests.h"

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


int types_tests_init_suite() {
    CU_pSuite pSuite = CU_add_suite("types", NULL, NULL);
    if (NULL == CU_add_test(pSuite, "small integer promotion", test_small_integer_promotion) ||
        NULL == CU_add_test(pSuite, "get common type (int, int)", test_get_common_type_int_int) ||
        NULL == CU_add_test(pSuite, "get common type (int, long)", test_get_common_type_int_long) ||
        NULL == CU_add_test(pSuite, "get common type (unsigned int, int)", test_get_common_type_unsigned_int_int) ||
        NULL == CU_add_test(pSuite, "get common type (short, char)", test_get_common_type_short_char) ||
        NULL == CU_add_test(pSuite, "get common type (float, double)", test_get_common_type_float_double) ||
        NULL == CU_add_test(pSuite, "get common type (double, long double)", test_get_common_type_double_long_double) ||
        NULL == CU_add_test(pSuite, "get common type (int, float)", test_get_common_type_int_float)
    ) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    return 0;
}