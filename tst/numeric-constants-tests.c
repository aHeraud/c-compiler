#include <limits.h>
#include "CUnit/CUnit.h"
#include "parser/lexer.h"
#include "types.h"
#include "parser/numeric-constants.h"

#include "tests.h"

token_t create_token(token_kind_t kind, const char* value) {
    return (token_t) {
        .kind = kind,
        .value = value,
        .position = {
            .path = "path/to/file",
            .line = 1,
            .column = 1,
        }
    };
}

void test_decode_simple_integer_constant() {
    unsigned long long value = 0;
    const type_t *type = NULL;
    token_t token = create_token(TK_INTEGER_CONSTANT, "123");
    decode_integer_constant(&token, &value, &type);

    CU_ASSERT_EQUAL_FATAL(value, 123)
    CU_ASSERT_TRUE_FATAL(types_equal(type, &INT))
}

void test_decode_integer_constant_with_size_suffix() {
    unsigned long long value = 0;
    const type_t *type = NULL;
    token_t token = create_token(TK_INTEGER_CONSTANT, "50l");
    decode_integer_constant(&token, &value, &type);

    CU_ASSERT_EQUAL(value, 50)
    CU_ASSERT_TRUE_FATAL(types_equal(type, &LONG))
}

void test_decode_integer_constant_with_unsigned_suffix() {
    unsigned long long value = 0;
    const type_t *type = NULL;
    token_t token = create_token(TK_INTEGER_CONSTANT, "50u");
    decode_integer_constant(&token, &value, &type);

    CU_ASSERT_EQUAL(value, 50)
    CU_ASSERT_TRUE_FATAL(types_equal(type, &UNSIGNED_INT))
}

void test_decode_integer_constant_larger_than_int() {
    unsigned long long value = 0;
    const type_t *type = NULL;
    char str[128];
    unsigned long long expected_value = (unsigned long long) UINT_MAX + 1ull;
    sprintf(str, "%llu", expected_value);
    token_t token = create_token(TK_INTEGER_CONSTANT, str);
    decode_integer_constant(&token, &value, &type);

    CU_ASSERT_EQUAL(value, expected_value)
    CU_ASSERT_TRUE_FATAL(types_equal(type, &LONG))
}

void test_decode_hex_integer_constant() {
    unsigned long long value = 0;
    const type_t *type = NULL;
    token_t token = create_token(TK_INTEGER_CONSTANT, "0xFF");
    decode_integer_constant(&token, &value, &type);

    CU_ASSERT_EQUAL(value, 255)
    CU_ASSERT_TRUE_FATAL(types_equal(type, &INT))
}

void test_decode_simple_float_constant() {
    long double value = 0;
    const type_t *type = NULL;
    token_t token = create_token(TK_FLOATING_CONSTANT, "2.5");
    decode_float_constant(&token, &value, &type);

    CU_ASSERT_EQUAL(value, 2.5)
    CU_ASSERT_TRUE_FATAL(types_equal(type, &DOUBLE))
}

void test_decode_float_constant_with_no_whole_part() {
    long double value = 0;
    const type_t *type = NULL;
    token_t token = create_token(TK_FLOATING_CONSTANT, ".5");
    decode_float_constant(&token, &value, &type);

    CU_ASSERT_EQUAL(value, 0.5)
    CU_ASSERT_TRUE_FATAL(types_equal(type, &DOUBLE))
}

void test_decode_float_constant_with_no_fractional_part() {
    long double value = 0;
    const type_t *type = NULL;
    token_t token = create_token(TK_FLOATING_CONSTANT, "1.");
    decode_float_constant(&token, &value, &type);

    CU_ASSERT_EQUAL(value, 1.0)
    CU_ASSERT_TRUE_FATAL(types_equal(type, &DOUBLE))

}

void test_decode_float_constant_with_size_suffix() {
    long double value = 0;
    const type_t *type = NULL;
    token_t token = create_token(TK_FLOATING_CONSTANT, "2.5f");
    decode_float_constant(&token, &value, &type);

    CU_ASSERT_EQUAL(value, 2.5)
    CU_ASSERT_TRUE_FATAL(types_equal(type, &FLOAT))
}

void test_decode_float_constant_with_exponent() {
    long double value = 0;
    const type_t *type = NULL;
    token_t token = create_token(TK_FLOATING_CONSTANT, "2e-3");
    decode_float_constant(&token, &value, &type);

    CU_ASSERT_EQUAL(value, 0.002)
    CU_ASSERT_TRUE_FATAL(types_equal(type, &DOUBLE))
}

void test_decode_hex_float() {
    long double value = 0;
    const type_t *type = NULL;
    token_t token = create_token(TK_FLOATING_CONSTANT, "0x1.0p-2");
    decode_float_constant(&token, &value, &type);

    CU_ASSERT_EQUAL(value, 0.25)
    CU_ASSERT_TRUE_FATAL(types_equal(type, &DOUBLE))

}

int numeric_constants_tests_init_suite() {
    CU_pSuite pSuite = CU_add_suite("numeric-constants", NULL, NULL);
    if (
        NULL == CU_add_test(pSuite, "decode simple integer constant", test_decode_simple_integer_constant) ||
        NULL == CU_add_test(pSuite, "decode integer constant with size suffix", test_decode_integer_constant_with_size_suffix) ||
        NULL == CU_add_test(pSuite, "decode integer constant with unsigned suffix", test_decode_integer_constant_with_unsigned_suffix) ||
        NULL == CU_add_test(pSuite, "decode integer constant larger than int", test_decode_integer_constant_larger_than_int) ||
        NULL == CU_add_test(pSuite, "decode hex integer constant", test_decode_hex_integer_constant) ||
        NULL == CU_add_test(pSuite, "decode simple float constant", test_decode_simple_float_constant) ||
        NULL == CU_add_test(pSuite, "decode float constant with no whole part", test_decode_float_constant_with_no_whole_part) ||
        NULL == CU_add_test(pSuite, "decode float constant with no fractional part", test_decode_float_constant_with_no_fractional_part) ||
        NULL == CU_add_test(pSuite, "decode float constant with size suffix", test_decode_float_constant_with_size_suffix) ||
        NULL == CU_add_test(pSuite, "decode float constant with exponent", test_decode_float_constant_with_exponent) ||
        NULL == CU_add_test(pSuite, "decode hex float constant", test_decode_hex_float)
    ) {
        CU_cleanup_registry();
        return CU_get_error();
    }
    return 0;
}
