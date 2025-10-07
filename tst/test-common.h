#ifndef C_COMPILER_TEST_COMMON_H
#define C_COMPILER_TEST_COMMON_H

#include <stdio.h>
#include <string.h>
#include "CUnit/Basic.h"

#include "parser/lexer.h"
#include "parser/parser.h"

token_kind_t* token_kind_array(token_t* tokens, size_t size);
const char** token_value_array(token_t* tokens, size_t size);
char* format_string_array(const char** array, size_t size);
char* format_token_kind_array(const token_kind_t* array, size_t size);

// AST helpers
source_position_t dummy_position(void);
source_span_t dummy_span(void);
expression_t *primary(primary_expression_t primary);
expression_t *integer_constant(char* value);
expression_t *float_constant(char* value);

// Type helpers
type_t *ptr_to(const type_t *type);
type_t *array_of(const type_t *type, expression_t *size);

#define TEST_ASSERT_ARRAYS_EQUAL(expected, expected_size, actual, actual_size, format) \
    do { \
        if (expected_size != actual_size) { \
            char* message = malloc(4096); \
            snprintf(message, 4096, "ARRAYS_NOT_EQUAL - expected size %zu, actual size %zu\nExpected: %s\nActual: %s\n", \
                     (size_t) expected_size, (size_t) actual_size, format(expected, expected_size), format(actual, actual_size)); \
            CU_assertImplementation(CU_FALSE, __LINE__, message, __FILE__, CU_FUNC, CU_TRUE); \
        } else { \
            for (size_t i = 0; i < expected_size; i++) { \
                if (expected[i] != actual[i]) { \
                    char* message = malloc(4096); \
                    snprintf(message, 4096, "ARRAYS_NOT_EQUAL - elements at index %zu not equal\nExpected: %s\nActual: %s\n", \
                             i, format(expected, expected_size), format(actual, actual_size)); \
                    CU_assertImplementation(CU_FALSE, __LINE__, message, __FILE__, CU_FUNC, CU_TRUE); \
                } \
            } \
        } \
    } while (0);

#define TEST_ASSERT_STRING_ARRAYS_EQUAL(expected, expected_size, actual, actual_size) \
    do { \
        if (expected_size != actual_size) { \
            char* message = malloc(4096); \
            snprintf(message, 4096, "ARRAYS_NOT_EQUAL - expected size %zu, actual size %zu\nExpected: %s\nActual: %s\n", \
                     (size_t) expected_size, (size_t) actual_size, format_string_array(expected, expected_size), format_string_array(actual, actual_size)); \
            CU_assertImplementation(CU_FALSE, __LINE__, message, __FILE__, CU_FUNC, CU_TRUE); \
        } else { \
            for (size_t i = 0; i < expected_size; i++) { \
                if (strcmp(expected[i], actual[i]) != 0) { \
                    char* message = malloc(4096); \
                    snprintf(message, 4096, "ARRAYS_NOT_EQUAL - elements at index %zu not equal\nExpected: %s\nActual: %s\n", \
                             i, format_string_array(expected, expected_size), format_string_array(actual, actual_size)); \
                    CU_assertImplementation(CU_FALSE, __LINE__, message, __FILE__, CU_FUNC, CU_TRUE); \
                } \
            } \
        } \
    } while (0);

#endif //C_COMPILER_TEST_COMMON_H
