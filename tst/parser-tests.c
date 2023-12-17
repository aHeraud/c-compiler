#include <malloc.h>
#include "CUnit/Basic.h"
#include "tests.h"
#include "parser.h"
#include "test-common.h"

static lexer_global_context_t create_context() {
    return (lexer_global_context_t) {
            .user_include_paths = NULL,
            .system_include_paths = NULL,
            .macro_definitions = {
                    .size = 0,
                    .num_buckets = 10,
                    .buckets = calloc(10, sizeof(hashtable_entry_t *)),
            }
    };
}

source_position_t dummy_position() {
    return (source_position_t) {
            .path = "path/to/file",
            .line = 0,
            .column = 0,
    };
}

source_span_t dummy_span() {
    return (source_span_t) {
            .start = {.path = "path/to/file", .line = 0, .column = 0},
            .end = {.path = "path/to/file", .line = 0, .column = 0},
    };
}

expression_t *primary(primary_expression_t primary) {
    expression_t *expr = malloc(sizeof(expression_t));
    *expr = (expression_t) {
            .type = EXPRESSION_PRIMARY,
            .span = dummy_span(),
            .primary = primary,
    };
    return expr;
}

expression_t *integer_constant(char* value) {
    return primary((primary_expression_t) {
            .type = PE_CONSTANT,
            .token = (token_t) {
                    .kind = TK_INTEGER_CONSTANT,
                    .value = value,
                    .position = dummy_position(),
            },
    });
}

expression_t *make_identifier(char* value) {
    return primary((primary_expression_t) {
            .type = PE_IDENTIFIER,
            .token = (token_t) {
                    .kind = TK_IDENTIFIER,
                    .value = value,
                    .position = dummy_position(),
            },
    });
}

expression_t *binary(binary_expression_t binary) {
    expression_t *expr = malloc(sizeof(expression_t));
    *expr = (expression_t) {
            .type = EXPRESSION_BINARY,
            .span = dummy_span(),
            .binary = binary,
    };
    return expr;
}

void test_parse_primary_expression_ident() {
    lexer_global_context_t context = create_context();
    char* input = "bar";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(primary_expression(&parser, &node));
    expression_t *expected = primary((primary_expression_t) {
        .type = PE_IDENTIFIER,
        .token = (token_t) {
            .kind = TK_IDENTIFIER,
            .value = "bar",
            .position = dummy_position(),
        },
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected));
}

void test_parse_primary_expression_int() {
    lexer_global_context_t context = create_context();
    char* input = "42";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(primary_expression(&parser, &node));
    expression_t *expected = primary((primary_expression_t) {
        .type = PE_CONSTANT,
        .token = (token_t) {
            .kind = TK_INTEGER_CONSTANT,
            .value = "42",
            .position = dummy_position(),
        },
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected));
}

void test_parse_primary_expression_float() {
    lexer_global_context_t context = create_context();
    char* input = "42.0";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(primary_expression(&parser, &node));
    expression_t *expected = primary((primary_expression_t) {
        .type = PE_CONSTANT,
        .token = (token_t) {
            .kind = TK_FLOATING_CONSTANT,
            .value = "42.0",
            .position = dummy_position(),
        },
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected));
}

void test_parse_primary_expression_char() {
    lexer_global_context_t context = create_context();
    char* input = "'a'";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    bool matches = primary_expression(&parser, &node);
    CU_ASSERT_TRUE_FATAL(matches);
    CU_ASSERT_EQUAL_FATAL(node.type, EXPRESSION_PRIMARY);
    CU_ASSERT_EQUAL_FATAL(node.primary.type, PE_CONSTANT);
    CU_ASSERT_EQUAL_FATAL(node.primary.token.kind, TK_CHAR_LITERAL);
    CU_ASSERT_STRING_EQUAL_FATAL(node.primary.token.value, "'a'");
}

void test_parse_primary_expression_parenthesized() {
    lexer_global_context_t context = create_context();
    char* input = "(42)";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t expr;
    CU_ASSERT_TRUE_FATAL(primary_expression(&parser, &expr));
    expression_t *expected = primary((primary_expression_t) {
            .type = PE_EXPRESSION,
            .expression = integer_constant("42"),
    });
}

void test_parse_multiplicative_expression() {
    lexer_global_context_t context = create_context();
    char *input = "1 / 2 * 3 % 4";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(multiplicative_expression(&parser, &node));
    expression_t *expected = binary((binary_expression_t) {
            .operator = BINARY_MODULO,
            .left = binary((binary_expression_t) {
                    .operator = BINARY_MULTIPLY,
                    .left = binary((binary_expression_t) {
                            .operator = BINARY_DIVIDE,
                            .left = integer_constant("1"),
                            .right = integer_constant("2"),
                    }),
                    .right = integer_constant("3"),
            }),
            .right = integer_constant("4"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected));
}

void test_parse_additive_expression() {
    lexer_global_context_t context = create_context();
    char *input = "1 + 2 - 3";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(additive_expression(&parser, &node));
    expression_t *expected = binary((binary_expression_t) {
            .operator = BINARY_SUBTRACT,
            .left = binary((binary_expression_t) {
                    .operator = BINARY_ADD,
                    .left = integer_constant("1"),
                    .right = integer_constant("2"),
            }),
            .right = integer_constant("3"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected));
}

void test_parse_shift_expression() {
    lexer_global_context_t context = create_context();
    char *input = "1 << 2 >> 3";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(shift_expression(&parser, &node));
    expression_t *expected = binary((binary_expression_t) {
            .operator = BINARY_SHIFT_RIGHT,
            .left = binary((binary_expression_t) {
                    .operator = BINARY_SHIFT_LEFT,
                    .left = integer_constant("1"),
                    .right = integer_constant("2"),
            }),
            .right = integer_constant("3"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected));
}

void test_parse_relational_expression() {
    lexer_global_context_t context = create_context();
    char *input = "1 < 2 > 3 <= 4 >= 5";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(relational_expression(&parser, &node));
    expression_t *expected = binary((binary_expression_t) {
            .operator = BINARY_GREATER_THAN_OR_EQUAL,
            .left = binary((binary_expression_t) {
                    .operator = BINARY_LESS_THAN_OR_EQUAL,
                    .left = binary((binary_expression_t) {
                            .operator = BINARY_GREATER_THAN,
                            .left = binary((binary_expression_t) {
                                    .operator = BINARY_LESS_THAN,
                                    .left = integer_constant("1"),
                                    .right = integer_constant("2"),
                            }),
                            .right = integer_constant("3"),
                    }),
                    .right = integer_constant("4"),
            }),
            .right = integer_constant("5"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected));
}

void test_parse_equality_expression() {
    lexer_global_context_t context = create_context();
    char *input = "1 == 2 != 3";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(equality_expression(&parser, &node));
    expression_t *expected = binary((binary_expression_t) {
            .operator = BINARY_NOT_EQUAL,
            .left = binary((binary_expression_t) {
                    .operator = BINARY_EQUAL,
                    .left = integer_constant("1"),
                    .right = integer_constant("2"),
            }),
            .right = integer_constant("3"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected));
}

void test_parse_and_expression() {
    lexer_global_context_t context = create_context();
    char *input = "1 & 2";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(and_expression(&parser, &node));
    expression_t *expected = binary((binary_expression_t) {
            .operator = BINARY_BITWISE_AND,
            .left = integer_constant("1"),
            .right = integer_constant("2"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected));
}

void test_parse_xor_expression() {
    lexer_global_context_t context = create_context();
    char *input = "1 ^ 2";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(exclusive_or_expression(&parser, &node));
    expression_t *expected = binary((binary_expression_t) {
            .operator = BINARY_BITWISE_XOR,
            .left = integer_constant("1"),
            .right = integer_constant("2"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected));
}

void test_parse_inclusive_or_expression() {
    lexer_global_context_t context = create_context();
    char *input = "1 | 2";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(inclusive_or_expression(&parser, &node));
    expression_t *expected = binary((binary_expression_t) {
            .operator = BINARY_BITWISE_OR,
            .left = integer_constant("1"),
            .right = integer_constant("2"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected));
}

void test_parse_logical_and_expression() {
    lexer_global_context_t context = create_context();
    char *input = "1 && 2";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(logical_and_expression(&parser, &node));
    expression_t *expected = binary((binary_expression_t) {
            .operator = BINARY_LOGICAL_AND,
            .left = integer_constant("1"),
            .right = integer_constant("2"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected));
}

void test_parse_logical_or_expression() {
    lexer_global_context_t context = create_context();
    char *input = "1 || 2";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;

    CU_ASSERT_TRUE_FATAL(logical_or_expression(&parser, &node));
    expression_t *expected = binary((binary_expression_t) {
            .operator = BINARY_LOGICAL_OR,
            .left = integer_constant("1"),
            .right = integer_constant("2"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected));
}

void test_parse_conditional_expression() {
    lexer_global_context_t context = create_context();
    char *input = "1 ? 2 : 3";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;

    CU_ASSERT_TRUE_FATAL(conditional_expression(&parser, &node));
    expression_t expected = (expression_t) {
            .span = dummy_span(),
            .type = EXPRESSION_TERNARY,
            .ternary = (ternary_expression_t) {
                    .condition = integer_constant("1"),
                    .true_expression = integer_constant("2"),
                    .false_expression = integer_constant("3"),
            },
    };
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, &expected));
}

void test_parse_assignment_expression() {
    lexer_global_context_t context = create_context();
    char *input = "val = 2";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;

    CU_ASSERT_TRUE_FATAL(assignment_expression(&parser, &node));
    expression_t *expected = binary((binary_expression_t) {
            .operator = BINARY_ASSIGN,
            .left = make_identifier("val"),
            .right = integer_constant("2"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected));
}

int parser_tests_init_suite() {
    CU_pSuite pSuite = CU_add_suite("parser", NULL, NULL);
    if (NULL == CU_add_test(pSuite, "primary expression - identifier", test_parse_primary_expression_ident) ||
        NULL == CU_add_test(pSuite, "primary expression - integer", test_parse_primary_expression_int) ||
        NULL == CU_add_test(pSuite, "primary expression - float", test_parse_primary_expression_float) ||
        NULL == CU_add_test(pSuite, "primary expression - char", test_parse_primary_expression_char) ||
        NULL == CU_add_test(pSuite, "primary expression - parenthesized", test_parse_primary_expression_parenthesized) ||
        NULL == CU_add_test(pSuite, "multiplicative expression", test_parse_multiplicative_expression) ||
        NULL == CU_add_test(pSuite, "additive expression", test_parse_additive_expression) ||
        NULL == CU_add_test(pSuite, "shift expression", test_parse_shift_expression) ||
        NULL == CU_add_test(pSuite, "relational expression", test_parse_relational_expression) ||
        NULL == CU_add_test(pSuite, "equality expression", test_parse_equality_expression) ||
        NULL == CU_add_test(pSuite, "equality expression", test_parse_and_expression) ||
        NULL == CU_add_test(pSuite, "exclusive or expression", test_parse_xor_expression) ||
        NULL == CU_add_test(pSuite, "inclusive or expression", test_parse_inclusive_or_expression) ||
        NULL == CU_add_test(pSuite, "logical and expression", test_parse_logical_and_expression) ||
        NULL == CU_add_test(pSuite, "logical or expression", test_parse_logical_or_expression) ||
        NULL == CU_add_test(pSuite, "conditional expression", test_parse_conditional_expression) ||
        NULL == CU_add_test(pSuite, "assignment expression", test_parse_assignment_expression)
    ) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    return 0;
}
