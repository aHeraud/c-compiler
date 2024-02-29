#include <malloc.h>
#include "CUnit/Basic.h"
#include "ast.h"
#include "tests.h"
#include "parser.h"
#include "types.h"
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

token_t *token(token_kind_t kind, const char* value) {
    token_t *token = malloc(sizeof(token_t));
    *token = (token_t) {
            .kind = kind,
            .value = value,
            .position = dummy_position(),
    };
    return token;
}

statement_t *return_statement(expression_t *expression) {
    statement_t *stmt = malloc(sizeof(statement_t));
    *stmt = (statement_t) {
            .type = STATEMENT_RETURN,
            .return_ = {
                    .keyword = token(TK_RETURN, "return"),
                    .expression = expression,
            },
            .terminator = token(TK_SEMICOLON, ";"),
    };
    return stmt;
}

statement_t *expression_statement(expression_t *expression) {
    statement_t *stmt = malloc(sizeof(statement_t));
    *stmt = (statement_t) {
            .type = STATEMENT_EXPRESSION,
            .expression = expression,
            .terminator = token(TK_SEMICOLON, ";"),
    };
    return stmt;
}

statement_t *if_statement(expression_t *condition, statement_t *true_branch, statement_t *false_branch) {
    statement_t *stmt = malloc(sizeof(statement_t));
    *stmt = (statement_t) {
        .type = STATEMENT_IF,
        .if_ = {
            .keyword = token(TK_IF, "if"),
            .condition = condition,
            .true_branch = true_branch,
            .false_branch = false_branch,
        },
    };
    return stmt;
}

block_item_t *block_item_s(statement_t *statement) {
    block_item_t *item = malloc(sizeof(block_item_t));
    *item = (block_item_t) {
            .type = BLOCK_ITEM_STATEMENT,
            .statement = statement,
    };
    return item;
}

block_item_t *block_item_d(declaration_t *declaration) {
    block_item_t *item = malloc(sizeof(block_item_t));
    *item = (block_item_t) {
            .type = BLOCK_ITEM_DECLARATION,
            .declaration = declaration,
    };
    return item;
}

const type_t *pointer_to(const type_t *type) {
    type_t *pointer = malloc(sizeof(type_t));
    *pointer = (type_t) {
        .kind = TYPE_POINTER,
        .pointer = {
            .base = type,
            .is_const = false,
            .is_volatile = false,
            .is_restrict = false,
        },
    };
    return pointer;
}

void test_parse_primary_expression_ident() {
    lexer_global_context_t context = create_context();
    char* input = "bar";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_primary_expression(&parser, &node))
    expression_t *expected = primary((primary_expression_t) {
        .type = PE_IDENTIFIER,
        .token = (token_t) {
            .kind = TK_IDENTIFIER,
            .value = "bar",
            .position = dummy_position(),
        },
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_primary_expression_int() {
    lexer_global_context_t context = create_context();
    char* input = "42";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_primary_expression(&parser, &node))
    expression_t *expected = primary((primary_expression_t) {
        .type = PE_CONSTANT,
        .token = (token_t) {
            .kind = TK_INTEGER_CONSTANT,
            .value = "42",
            .position = dummy_position(),
        },
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_primary_expression_float() {
    lexer_global_context_t context = create_context();
    char* input = "42.0";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_primary_expression(&parser, &node))
    expression_t *expected = primary((primary_expression_t) {
        .type = PE_CONSTANT,
        .token = (token_t) {
            .kind = TK_FLOATING_CONSTANT,
            .value = "42.0",
            .position = dummy_position(),
        },
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_primary_expression_char() {
    lexer_global_context_t context = create_context();
    char* input = "'a'";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    bool matches = parse_primary_expression(&parser, &node);
    CU_ASSERT_TRUE_FATAL(matches)
    CU_ASSERT_EQUAL_FATAL(node.type, EXPRESSION_PRIMARY)
    CU_ASSERT_EQUAL_FATAL(node.primary.type, PE_CONSTANT)
    CU_ASSERT_EQUAL_FATAL(node.primary.token.kind, TK_CHAR_LITERAL)
    CU_ASSERT_STRING_EQUAL_FATAL(node.primary.token.value, "'a'")
}

void test_parse_primary_expression_parenthesized() {
    lexer_global_context_t context = create_context();
    char* input = "(42)";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t expr;
    CU_ASSERT_TRUE_FATAL(parse_primary_expression(&parser, &expr))
    expression_t *expected = primary((primary_expression_t) {
            .type = PE_EXPRESSION,
            .expression = integer_constant("42"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&expr, expected))
}

void test_parse_postfix_expression_function_call() {
    lexer_global_context_t context = create_context();
    char* input = "pow(4,2)";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t expr;
    CU_ASSERT_TRUE_FATAL(parse_postfix_expression(&parser, &expr))
    expression_t *arg_buffer[] = {
            integer_constant("4"),
            integer_constant("2"),
    };
    ptr_vector_t arguments = {
            .size = 2,
            .capacity = 2,
            .buffer = (void**)arg_buffer,
    };
    expression_t expected = (expression_t) {
        .span = dummy_span(),
        .type = EXPRESSION_CALL,
        .call = (call_expression_t) {
            .callee = make_identifier("pow"),
            .arguments = arguments,
        },
    };
    CU_ASSERT_TRUE_FATAL(expression_eq(&expr, &expected))
}

void test_parse_postfix_expression_array_subscript() {
    lexer_global_context_t context = create_context();
    char* input = "arr[1 + 1]";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t expr;
    CU_ASSERT_TRUE_FATAL(parse_postfix_expression(&parser, &expr))
    expression_t expected = (expression_t) {
            .span = dummy_span(),
            .type = EXPRESSION_ARRAY_SUBSCRIPT,
            .array_subscript = (array_subscript_expression_t) {
                .array = make_identifier("arr"),
                .index = binary((binary_expression_t) {
                    .type = BINARY_ARITHMETIC,
                    .left = integer_constant("1"),
                    .right = integer_constant("1"),
                    .operator = token(TK_PLUS, "+"),
                    .arithmetic_operator = BINARY_ARITHMETIC_ADD,
                }),
            },
    };
    CU_ASSERT_TRUE_FATAL(expression_eq(&expr, &expected))
}

void test_parse_postfix_expression_member_access() {
    lexer_global_context_t context = create_context();
    char* input = "foo.bar";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t expr;
    CU_ASSERT_TRUE_FATAL(parse_postfix_expression(&parser, &expr))
    expression_t expected = (expression_t) {
            .span = dummy_span(),
            .type = EXPRESSION_MEMBER_ACCESS,
            .member_access = (member_access_expression_t) {
                    .struct_or_union = make_identifier("foo"),
                    .operator = (token_t) {
                            .kind = TK_DOT,
                            .value = ".",
                            .position = dummy_position(),
                    },
                    .member = (token_t) {
                            .kind = TK_IDENTIFIER,
                            .value = "bar",
                            .position = dummy_position(),
                    },
            },
    };
    CU_ASSERT_TRUE_FATAL(expression_eq(&expr, &expected))
}

void test_parse_multiplicative_expression() {
    lexer_global_context_t context = create_context();
    char *input = "1 / 2 * 3 % 4";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_multiplicative_expression(&parser, &node))
    expression_t *expected = binary((binary_expression_t) {
            .left = binary((binary_expression_t) {
                    .left = binary((binary_expression_t) {
                            .type = BINARY_ARITHMETIC,
                            .arithmetic_operator = BINARY_ARITHMETIC_DIVIDE,
                            .left = integer_constant("1"),
                            .right = integer_constant("2"),
                            .operator = token(TK_SLASH, "/"),
                    }),
                    .right = integer_constant("3"),
                    .type = BINARY_ARITHMETIC,
                    .arithmetic_operator = BINARY_ARITHMETIC_MULTIPLY,
                    .operator = token(TK_STAR, "*"),
            }),
            .right = integer_constant("4"),
            .type = BINARY_ARITHMETIC,
            .arithmetic_operator = BINARY_ARITHMETIC_MODULO,
            .operator = token(TK_PERCENT, "%"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_additive_expression() {
    lexer_global_context_t context = create_context();
    char *input = "1 + 2 - 3";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_additive_expression(&parser, &node))
    expression_t *expected = binary((binary_expression_t) {
            .left = binary((binary_expression_t) {
                    .left = integer_constant("1"),
                    .right = integer_constant("2"),
                    .type = BINARY_ARITHMETIC,
                    .arithmetic_operator = BINARY_ARITHMETIC_ADD,
                    .operator = token(TK_PLUS, "+"),
            }),
            .right = integer_constant("3"),
            .type = BINARY_ARITHMETIC,
            .arithmetic_operator = BINARY_ARITHMETIC_SUBTRACT,
            .operator = token(TK_MINUS, "-"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_additive_expression_2() {
    lexer_global_context_t context = create_context();
    char *input = "1 + 2 * 3;";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t expr;

    CU_ASSERT_TRUE_FATAL(parse_additive_expression(&parser, &expr))

    expression_t *expected = binary((binary_expression_t) {
        .left = integer_constant("1"),
        .right = binary((binary_expression_t) {
            .left = integer_constant("2"),
            .right = integer_constant("3"),
            .type = BINARY_ARITHMETIC,
            .arithmetic_operator = BINARY_ARITHMETIC_MULTIPLY,
            .operator = token(TK_STAR, "*"),
        }),
        .operator = token(TK_PLUS, "+"),
        .type = BINARY_ARITHMETIC,
        .arithmetic_operator = BINARY_ARITHMETIC_ADD,
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&expr, expected))
}

void test_parse_shift_expression() {
    lexer_global_context_t context = create_context();
    char *input = "1 << 2 >> 3";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_shift_expression(&parser, &node))
    expression_t *expected = binary((binary_expression_t) {
            .left = binary((binary_expression_t) {
                    .left = integer_constant("1"),
                    .right = integer_constant("2"),
                    .type = BINARY_BITWISE,
                    .bitwise_operator = BINARY_BITWISE_SHIFT_LEFT,
                    .operator = token(TK_LSHIFT, "<<"),
            }),
            .right = integer_constant("3"),
            .type = BINARY_BITWISE,
            .bitwise_operator = BINARY_BITWISE_SHIFT_RIGHT,
            .operator = token(TK_RSHIFT, ">>"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_relational_expression() {
    lexer_global_context_t context = create_context();
    char *input = "1 < 2 > 3 <= 4 >= 5";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_relational_expression(&parser, &node))
    expression_t *expected = binary((binary_expression_t) {
            .left = binary((binary_expression_t) {
                    .left = binary((binary_expression_t) {
                            .left = binary((binary_expression_t) {
                                    .left = integer_constant("1"),
                                    .right = integer_constant("2"),
                                    .type = BINARY_COMPARISON,
                                    .comparison_operator = BINARY_COMPARISON_LESS_THAN,
                                    .operator = token(TK_LESS_THAN, "<"),
                            }),
                            .right = integer_constant("3"),
                            .type = BINARY_COMPARISON,
                            .comparison_operator = BINARY_COMPARISON_GREATER_THAN,
                            .operator = token(TK_GREATER_THAN, ">"),
                    }),
                    .right = integer_constant("4"),
                    .type = BINARY_COMPARISON,
                    .comparison_operator = BINARY_COMPARISON_LESS_THAN_OR_EQUAL,
                    .operator = token(TK_LESS_THAN_EQUAL, "<="),
            }),
            .right = integer_constant("5"),
            .type = BINARY_COMPARISON,
            .comparison_operator = BINARY_COMPARISON_GREATER_THAN_OR_EQUAL,
            .operator = token(TK_GREATER_THAN_EQUAL, ">="),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_equality_expression() {
    lexer_global_context_t context = create_context();
    char *input = "1 == 2 != 3";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_equality_expression(&parser, &node))
    expression_t *expected = binary((binary_expression_t) {
            .left = binary((binary_expression_t) {
                    .left = integer_constant("1"),
                    .right = integer_constant("2"),
                    .type = BINARY_COMPARISON,
                    .comparison_operator = BINARY_COMPARISON_EQUAL,
                    .operator = token(TK_EQUALS, "=="),
            }),
            .right = integer_constant("3"),
            .type = BINARY_COMPARISON,
            .comparison_operator = BINARY_COMPARISON_NOT_EQUAL,
            .operator = token(TK_NOT_EQUALS, "!="),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_and_expression() {
    lexer_global_context_t context = create_context();
    char *input = "1 & 2";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_and_expression(&parser, &node))
    expression_t *expected = binary((binary_expression_t) {
            .left = integer_constant("1"),
            .right = integer_constant("2"),
            .type = BINARY_BITWISE,
            .bitwise_operator = BINARY_BITWISE_AND,
            .operator = token(TK_AMPERSAND, "&"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_xor_expression() {
    lexer_global_context_t context = create_context();
    char *input = "1 ^ 2";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_exclusive_or_expression(&parser, &node))
    expression_t *expected = binary((binary_expression_t) {
            .left = integer_constant("1"),
            .right = integer_constant("2"),
            .type = BINARY_BITWISE,
            .bitwise_operator = BINARY_BITWISE_XOR,
            .operator = token(TK_BITWISE_XOR, "^"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_inclusive_or_expression() {
    lexer_global_context_t context = create_context();
    char *input = "1 | 2";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_inclusive_or_expression(&parser, &node))
    expression_t *expected = binary((binary_expression_t) {
            .left = integer_constant("1"),
            .right = integer_constant("2"),
            .type = BINARY_BITWISE,
            .bitwise_operator = BINARY_BITWISE_OR,
            .operator = token(TK_BITWISE_OR, "|"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_logical_and_expression() {
    lexer_global_context_t context = create_context();
    char *input = "1 && 2";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_logical_and_expression(&parser, &node))
    expression_t *expected = binary((binary_expression_t) {
            .left = integer_constant("1"),
            .right = integer_constant("2"),
            .type = BINARY_LOGICAL,
            .logical_operator = BINARY_LOGICAL_AND,
            .operator = token(TK_LOGICAL_AND, "&&"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_logical_or_expression() {
    lexer_global_context_t context = create_context();
    char *input = "1 || 2";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;

    CU_ASSERT_TRUE_FATAL(parse_logical_or_expression(&parser, &node))
    expression_t *expected = binary((binary_expression_t) {
            .left = integer_constant("1"),
            .right = integer_constant("2"),
            .type = BINARY_LOGICAL,
            .logical_operator = BINARY_LOGICAL_OR,
            .operator = token(TK_LOGICAL_OR, "||"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_conditional_expression() {
    lexer_global_context_t context = create_context();
    char *input = "1 ? 2 : 3";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;

    CU_ASSERT_TRUE_FATAL(parse_conditional_expression(&parser, &node))
    expression_t expected = (expression_t) {
            .span = dummy_span(),
            .type = EXPRESSION_TERNARY,
            .ternary = (ternary_expression_t) {
                    .condition = integer_constant("1"),
                    .true_expression = integer_constant("2"),
                    .false_expression = integer_constant("3"),
            },
    };
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, &expected))
}

void test_parse_assignment_expression() {
    lexer_global_context_t context = create_context();
    char *input = "val = 2";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;

    CU_ASSERT_TRUE_FATAL(parse_assignment_expression(&parser, &node))
    expression_t *expected = binary((binary_expression_t) {
            .left = make_identifier("val"),
            .right = integer_constant("2"),
            .type = BINARY_ASSIGNMENT,
            .assignment_operator = BINARY_ASSIGN,
            .operator = token(TK_ASSIGN, "="),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_int_declaration_specifiers() {
    lexer_global_context_t context = create_context();
    char *input = "int";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    type_t type;
    CU_ASSERT_TRUE_FATAL(parse_declaration_specifiers(&parser, &type))
    CU_ASSERT_TRUE_FATAL(types_equal(&type, &INT))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
}

void test_parse_invalid_declaration_specifiers() {
    lexer_global_context_t context = create_context();
    char *input = "signed float";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    type_t type;
    CU_ASSERT_TRUE_FATAL(parse_declaration_specifiers(&parser, &type))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 1)
    CU_ASSERT_TRUE_FATAL(types_equal(&type, &INT))
}

void test_parse_empty_declaration() {
    lexer_global_context_t context = create_context();
    char *input = "int;";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    ptr_vector_t declarations = {
            .size = 0,
            .capacity = 0,
            .buffer = NULL,
    };
    CU_ASSERT_TRUE_FATAL(parse_declaration(&parser, &declarations))
    CU_ASSERT_EQUAL_FATAL(declarations.size, 0)
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
}

void test_parse_simple_declaration() {
    lexer_global_context_t context = create_context();
    char *input = "int a;";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    ptr_vector_t declarations = {
            .size = 0,
            .capacity = 0,
            .buffer = NULL,
    };
    CU_ASSERT_TRUE_FATAL(parse_declaration(&parser, &declarations))
    CU_ASSERT_EQUAL_FATAL(declarations.size, 1)
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)

    declaration_t expected = (declaration_t) {
        .type = &INT,
        .identifier = token(TK_IDENTIFIER, "a"),
        .initializer = NULL,
    };
    CU_ASSERT_TRUE_FATAL(declaration_eq(declarations.buffer[0], &expected))
}

void test_parse_pointer_declaration() {
    lexer_global_context_t context = create_context();
    char *input = "void *a;";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    ptr_vector_t declarations = { .size = 0, .capacity = 0, .buffer = NULL, };
    CU_ASSERT_TRUE_FATAL(parse_declaration(&parser, &declarations))
    CU_ASSERT_EQUAL_FATAL(declarations.size, 1)
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
    declaration_t expected = (declaration_t) {
            .type = pointer_to(&VOID),
            .identifier = token(TK_IDENTIFIER, "a"),
            .initializer = NULL,
    };
    CU_ASSERT_TRUE_FATAL(declaration_eq(declarations.buffer[0], &expected))
}

void test_parse_compound_declaration() {
    lexer_global_context_t context = create_context();
    char *input = "int a, b = 0, c = d + 1;";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    ptr_vector_t declarations = { .size = 0, .capacity = 0, .buffer = NULL, };
    CU_ASSERT_TRUE_FATAL(parse_declaration(&parser, &declarations))
    CU_ASSERT_EQUAL_FATAL(declarations.size, 3)
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)

    declaration_t expected_a = (declaration_t) {
            .type = &INT,
            .identifier = token(TK_IDENTIFIER, "a"),
            .initializer = NULL,
    };
    CU_ASSERT_TRUE_FATAL(declaration_eq(declarations.buffer[0], &expected_a))

    declaration_t expected_b = (declaration_t) {
            .type = &INT,
            .identifier = token(TK_IDENTIFIER, "b"),
            .initializer = integer_constant("0"),
    };
    CU_ASSERT_TRUE_FATAL(declaration_eq(declarations.buffer[1], &expected_b))

    declaration_t expected_c = (declaration_t) {
            .type = &INT,
            .identifier = token(TK_IDENTIFIER, "c"),
            .initializer = binary((binary_expression_t) {
                    .type = BINARY_ARITHMETIC,
                    .arithmetic_operator = BINARY_ARITHMETIC_ADD,
                    .left = make_identifier("d"),
                    .right = integer_constant("1"),
                    .operator = token(TK_PLUS, "+"),
            }),
    };
    CU_ASSERT_TRUE_FATAL(declaration_eq(declarations.buffer[2], &expected_c))
}

void test_parse_function_declaration_no_parameters() {
    lexer_global_context_t context = create_context();
    char *input = "int foo();";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    ptr_vector_t declarations = { .size = 0, .capacity = 0, .buffer = NULL, };
    CU_ASSERT_TRUE_FATAL(parse_declaration(&parser, &declarations))
    CU_ASSERT_EQUAL_FATAL(declarations.size, 1)
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)

    parameter_type_list_t parameters = (parameter_type_list_t) {
        .variadic = false,
        .parameters = NULL,
        .length = 0,
    };

    type_t type = {
            .kind = TYPE_FUNCTION,
            .function = {
                    .return_type = &INT,
                    .parameter_list = &parameters,
            },
    };

    declaration_t expected = (declaration_t) {
            .type = &type,
            .identifier = token(TK_IDENTIFIER, "foo"),
            .initializer = NULL,
    };

    CU_ASSERT_TRUE_FATAL(declaration_eq(declarations.buffer[0], &expected))
}

void test_parse_function_declaration_returning_pointer() {
    lexer_global_context_t context = create_context();
    char *input = "int *foo();";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    ptr_vector_t declarations = { .size = 0, .capacity = 0, .buffer = NULL, };
    CU_ASSERT_TRUE_FATAL(parse_declaration(&parser, &declarations))
    CU_ASSERT_EQUAL_FATAL(declarations.size, 1)
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)

    parameter_type_list_t parameters = (parameter_type_list_t) {
            .variadic = false,
            .parameters = NULL,
            .length = 0,
    };

    type_t type = {
            .kind = TYPE_FUNCTION,
            .function = {
                    .return_type = pointer_to(&INT),
                    .parameter_list = &parameters,
            },
    };

    declaration_t expected = (declaration_t) {
            .type = &type,
            .identifier = token(TK_IDENTIFIER, "foo"),
            .initializer = NULL,
    };

    CU_ASSERT_TRUE_FATAL(declaration_eq(declarations.buffer[0], &expected))
}

void test_parse_array_declaration() {
    lexer_global_context_t context = create_context();
    char *input = "int foo[10];";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    ptr_vector_t declarations = { .size = 0, .capacity = 0, .buffer = NULL, };
    CU_ASSERT_TRUE_FATAL(parse_declaration(&parser, &declarations))
    CU_ASSERT_EQUAL_FATAL(declarations.size, 1)
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)

    declaration_t *declaration = declarations.buffer[0];
    type_t expected_type = {
            .kind = TYPE_ARRAY,
            .array = {
                    .element_type = &INT,
                    .size = integer_constant("10"),
            },
    };
    CU_ASSERT_STRING_EQUAL_FATAL(declaration->identifier->value, "foo")
    CU_ASSERT_TRUE_FATAL(types_equal(declaration->type, &expected_type))
}

void test_parse_2d_array_declaration() {
    lexer_global_context_t context = create_context();
    char *input = "int bar[1][2];";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    ptr_vector_t declarations = { .size = 0, .capacity = 0, .buffer = NULL, };
    CU_ASSERT_TRUE_FATAL(parse_declaration(&parser, &declarations))
    CU_ASSERT_EQUAL_FATAL(declarations.size, 1)
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)

    declaration_t *declaration = declarations.buffer[0];
    type_t inner_type = {
            .kind = TYPE_ARRAY,
            .array = {
                    .element_type = &INT,
                    .size = integer_constant("2"),
            },
    };
    type_t expected_type = {
            .kind = TYPE_ARRAY,
            .array = {
                    .element_type = &inner_type,
                    .size = integer_constant("1"),
            },
    };
    CU_ASSERT_STRING_EQUAL_FATAL(declaration->identifier->value, "bar")
    CU_ASSERT_TRUE_FATAL(types_equal(declaration->type, &expected_type))
}

void test_parse_array_of_functions_declaration() {
    lexer_global_context_t context = create_context();
    char* input = "int foo[](void);";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    ptr_vector_t declarations = { .size = 0, .capacity = 0, .buffer = NULL, };
    CU_ASSERT_TRUE_FATAL(parse_declaration(&parser, &declarations))
    CU_ASSERT_EQUAL_FATAL(declarations.size, 1)
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)

    parameter_declaration_t **parameters = malloc(sizeof(void*));
    parameter_declaration_t void_param = {
            .type = &VOID,
            .identifier = NULL,
    };
    parameters[0] = &void_param;

    parameter_type_list_t parameter_list = (parameter_type_list_t) {
            .variadic = false,
            .parameters = parameters,
            .length = 1,
    };

    type_t fn = {
        .kind = TYPE_FUNCTION,
        .function = {
            .return_type = &INT,
            .parameter_list = &parameter_list,
        },
    };

    type_t type = {
        .kind = TYPE_ARRAY,
        .array = {
            .element_type = &fn,
            .size = NULL,
        }
    };

    declaration_t expected = (declaration_t) {
        .type = &type,
        .identifier = token(TK_IDENTIFIER, "foo"),
        .initializer = NULL,
    };

    CU_ASSERT_TRUE_FATAL(declaration_eq(declarations.buffer[0], &expected))
}

void test_parse_function_prototype_void() {
    lexer_global_context_t context = create_context();
    char *input = "float foo(void);";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    ptr_vector_t declarations = { .size = 0, .capacity = 0, .buffer = NULL, };
    CU_ASSERT_TRUE_FATAL(parse_declaration(&parser, &declarations))
    CU_ASSERT_EQUAL_FATAL(declarations.size, 1)
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)

    parameter_declaration_t **parameters = malloc(sizeof(void*));
    parameter_declaration_t void_param = {
            .type = &VOID,
            .identifier = NULL,
    };
    parameters[0] = &void_param;

    parameter_type_list_t parameter_list = (parameter_type_list_t) {
            .variadic = false,
            .parameters = parameters,
            .length = 1,
    };

    type_t type = {
            .kind = TYPE_FUNCTION,
            .function = {
                    .return_type = &FLOAT,
                    .parameter_list = &parameter_list,
            },
    };

    declaration_t expected = (declaration_t) {
            .type = &type,
            .identifier = token(TK_IDENTIFIER, "foo"),
            .initializer = NULL,
    };

    CU_ASSERT_TRUE_FATAL(declaration_eq(declarations.buffer[0], &expected))
}

void test_parse_function_prototype() {
    lexer_global_context_t context = create_context();
    char *input = "double pow(float a, short b);";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    ptr_vector_t declarations = { .size = 0, .capacity = 0, .buffer = NULL, };
    CU_ASSERT_TRUE_FATAL(parse_declaration(&parser, &declarations))
    CU_ASSERT_EQUAL_FATAL(declarations.size, 1)
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)

    parameter_declaration_t **parameters = malloc(sizeof(void*) * 2);
    parameters[0] = malloc(sizeof(parameter_declaration_t));
    *parameters[0] = (parameter_declaration_t) {
        .type = &FLOAT,
        .identifier = token(TK_IDENTIFIER, "a"),
    };
    parameters[1] = malloc(sizeof(parameter_declaration_t));
    *parameters[1] = (parameter_declaration_t) {
        .type = &SHORT,
        .identifier = token(TK_IDENTIFIER, "b"),
    };

    parameter_type_list_t parameter_list = (parameter_type_list_t) {
            .variadic = false,
            .parameters = parameters,
            .length = 2,
    };

    type_t type = {
            .kind = TYPE_FUNCTION,
            .function = {
                    .return_type = &DOUBLE,
                    .parameter_list = &parameter_list,
            },
    };

    declaration_t expected = (declaration_t) {
            .type = &type,
            .identifier = token(TK_IDENTIFIER, "pow"),
            .initializer = NULL,
    };

    CU_ASSERT_TRUE_FATAL(declaration_eq(declarations.buffer[0], &expected))
}

void test_parse_empty_statement() {
    lexer_global_context_t context = create_context();
    char *input = ";";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    statement_t node;

    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &node))
    statement_t *expected = malloc(sizeof(statement_t));
    *expected = (statement_t) {
            .type = STATEMENT_EMPTY,
            .terminator = token(TK_SEMICOLON, ";"),
    };
    CU_ASSERT_TRUE_FATAL(statement_eq(&node, expected))
}

void test_parse_expression_statement() {
    lexer_global_context_t context = create_context();
    char *input = "1;";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    statement_t node;

    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &node))
    statement_t *expected = malloc(sizeof(statement_t));
    *expected = (statement_t) {
            .type = STATEMENT_EXPRESSION,
            .expression = integer_constant("1"),
            .terminator = token(TK_SEMICOLON, ";"),
    };
    CU_ASSERT_TRUE_FATAL(statement_eq(&node, expected))
}

void test_parse_compound_statement() {
    lexer_global_context_t context = create_context();
    char *input = "{ 1; 'a'; 1.0; }";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    statement_t node;

    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &node))

    statement_t *expected = malloc(sizeof(statement_t));
    statement_t *statements[3] = {
            expression_statement(integer_constant("1")),
            expression_statement(primary((primary_expression_t) {
                    .type = PE_CONSTANT,
                    .token = (token_t) {
                            .kind = TK_CHAR_LITERAL,
                            .value = "'a'",
                            .position = dummy_position(),
                    },
            })),
            expression_statement(primary((primary_expression_t) {
                    .type = PE_CONSTANT,
                    .token = (token_t) {
                            .kind = TK_FLOATING_CONSTANT,
                            .value = "1.0",
                            .position = dummy_position(),
                    },
            })),
    };
    block_item_t *block_items[3] = {
            block_item_s(statements[0]),
            block_item_s(statements[1]),
            block_item_s(statements[2]),
    };

    *expected = (statement_t) {
        .type = STATEMENT_COMPOUND,
        .compound = {
            .block_items = {
                .size = 3,
                .capacity = 3,
                .buffer = (void**) block_items,
            },
        },
        .terminator = token(TK_RBRACE, "}"),
    };
    CU_ASSERT_TRUE_FATAL(statement_eq(&node, expected))
}

void test_parse_compound_statement_with_error() {
    // The parser should recover, and continue parsing the rest of the statements.
    lexer_global_context_t context = create_context();
    char *input = "{ a-; 1; }";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    statement_t node;

    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &node))
    statement_t *expected = malloc(sizeof(statement_t));
    block_item_t *block_items[1] = {
            block_item_s(expression_statement(integer_constant("1"))),
    };
    *expected = (statement_t) {
            .type = STATEMENT_COMPOUND,
            .compound = {
                    .block_items = {
                            .size = 1,
                            .capacity = 1,
                            .buffer = (void**) block_items,
                    },
            },
            .terminator = token(TK_RBRACE, "}"),
    };
    CU_ASSERT_TRUE_FATAL(statement_eq(&node, expected))
    CU_ASSERT_TRUE_FATAL(parser.errors.size == 1)
}

void test_parse_if_statement() {
    lexer_global_context_t context = create_context();
    char *input = "if (1) 2;";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    statement_t node;

    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &node))
    statement_t *expected = malloc(sizeof(statement_t));
    *expected = (statement_t) {
        .type = STATEMENT_IF,
        .if_ = {
            .keyword = token(TK_IF, "if"),
            .condition = integer_constant("1"),
            .true_branch = expression_statement(integer_constant("2")),
            .false_branch = NULL,
        },
    };
    CU_ASSERT_TRUE_FATAL(statement_eq(&node, expected))
}

void test_parse_if_else_statement() {
    lexer_global_context_t context = create_context();
    char *input = "if (1) 2; else 3;";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    statement_t node;

    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &node))
    statement_t *expected = malloc(sizeof(statement_t));
    *expected = (statement_t) {
        .type = STATEMENT_IF,
        .if_ = {
            .keyword = token(TK_IF, "if"),
            .condition = integer_constant("1"),
            .true_branch = expression_statement(integer_constant("2")),
            .false_branch = expression_statement(integer_constant("3")),
        },
    };
    CU_ASSERT_TRUE_FATAL(statement_eq(&node, expected))
}

void test_parse_if_else_if_else_statement() {
    lexer_global_context_t context = create_context();
    char *input = "if (1) 2; else if (3) 4; else 5;";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    statement_t node;

    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &node))
    statement_t *expected = malloc(sizeof(statement_t));
    *expected = (statement_t) {
            .type = STATEMENT_IF,
            .if_ = {
                    .keyword = token(TK_IF, "if"),
                    .condition = integer_constant("1"),
                    .true_branch = expression_statement(integer_constant("2")),
                    .false_branch = if_statement(integer_constant("3"), expression_statement(integer_constant("4")), expression_statement(integer_constant("5"))),
            },
    };
    CU_ASSERT_TRUE_FATAL(statement_eq(&node, expected))
}

void test_parse_return_statement() {
    lexer_global_context_t context = create_context();
    char *input = "return 1;";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    statement_t node;

    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &node))
    statement_t *expected = malloc(sizeof(statement_t));
    *expected = (statement_t) {
            .type = STATEMENT_RETURN,
            .return_ = {
                    .keyword = token(TK_RETURN, "return"),
                    .expression = integer_constant("1"),
            },
            .terminator = token(TK_SEMICOLON, ";"),
    };
    CU_ASSERT_TRUE_FATAL(statement_eq(&node, expected))
}

void parse_external_declaration_declaration() {
    lexer_global_context_t context = create_context();
    char *input = "int a = 4;";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    external_declaration_t node;
    CU_ASSERT_TRUE_FATAL(parse_external_declaration(&parser, &node))
    declaration_t expected = (declaration_t ) {
        .type = &INT,
        .identifier = token(TK_IDENTIFIER, "a"),
        .initializer = integer_constant("4"),
    };

    CU_ASSERT_TRUE_FATAL(node.type == EXTERNAL_DECLARATION_DECLARATION)
    CU_ASSERT_EQUAL_FATAL(node.declaration.length, 1)
    CU_ASSERT_TRUE_FATAL(declaration_eq(node.declaration.declarations[0], &expected))
}

void parse_external_declaration_function_definition() {
    lexer_global_context_t context = create_context();
    char *input = "float square(float val) { return val * val; }";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);

    external_declaration_t node;
    CU_ASSERT_TRUE_FATAL(parse_external_declaration(&parser, &node))
    CU_ASSERT_TRUE_FATAL(node.type == EXTERNAL_DECLARATION_FUNCTION_DEFINITION)

    CU_ASSERT_TRUE_FATAL(types_equal(node.function_definition->return_type, &FLOAT))
    CU_ASSERT_TRUE_FATAL(strcmp(node.function_definition->identifier->value, "square") == 0)

    // validate the argument list
    CU_ASSERT_EQUAL_FATAL(node.function_definition->parameter_list->length, 1)
    CU_ASSERT_TRUE_FATAL(types_equal(node.function_definition->parameter_list->parameters[0]->type, &FLOAT))
    CU_ASSERT_TRUE_FATAL(strcmp(node.function_definition->parameter_list->parameters[0]->identifier->value, "val") == 0)

    // validate the body is parsed correctly
    statement_t *ret = {
        return_statement(binary((binary_expression_t) {
            .type = BINARY_ARITHMETIC,
            .arithmetic_operator = BINARY_ARITHMETIC_MULTIPLY,
            .left = make_identifier("val"),
            .right = make_identifier("val"),
            .operator = token(TK_STAR, "*"),
        })),
    };
    block_item_t *block_item = block_item_s(ret);
    statement_t body = {
        .type = STATEMENT_COMPOUND,
        .terminator = token(TK_RBRACE, "}"),
        .compound = {
            .block_items = {
                .size = 1,
                .capacity = 1,
                .buffer = (void**) &block_item,
            },
        },
    };
    CU_ASSERT_TRUE_FATAL(statement_eq(node.function_definition->body, &body))
}

void test_parse_program() {
    lexer_global_context_t context = create_context();
    char* input = "float square(float);\nfloat square(float val) {\n\treturn val * val;\n}\nint main() {\n\treturn square(2.0);\n}";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);

    translation_unit_t program;
    CU_ASSERT_TRUE_FATAL(parse(&parser, &program))
}

int parser_tests_init_suite() {
    CU_pSuite pSuite = CU_add_suite("parser", NULL, NULL);
    if (NULL == CU_add_test(pSuite, "primary expression - identifier", test_parse_primary_expression_ident) ||
        NULL == CU_add_test(pSuite, "primary expression - integer", test_parse_primary_expression_int) ||
        NULL == CU_add_test(pSuite, "primary expression - float", test_parse_primary_expression_float) ||
        NULL == CU_add_test(pSuite, "primary expression - char", test_parse_primary_expression_char) ||
        NULL == CU_add_test(pSuite, "primary expression - parenthesized", test_parse_primary_expression_parenthesized) ||
        NULL == CU_add_test(pSuite, "postfix expression - function call", test_parse_postfix_expression_function_call) ||
        NULL == CU_add_test(pSuite, "postfix expression - array subscript", test_parse_postfix_expression_array_subscript) ||
        NULL == CU_add_test(pSuite, "postfix expression - member access", test_parse_postfix_expression_member_access) ||
        NULL == CU_add_test(pSuite, "multiplicative expression", test_parse_multiplicative_expression) ||
        NULL == CU_add_test(pSuite, "additive expression", test_parse_additive_expression) ||
        NULL == CU_add_test(pSuite, "additive expression 2", test_parse_additive_expression_2) ||
        NULL == CU_add_test(pSuite, "shift expression", test_parse_shift_expression) ||
        NULL == CU_add_test(pSuite, "relational expression", test_parse_relational_expression) ||
        NULL == CU_add_test(pSuite, "equality expression", test_parse_equality_expression) ||
        NULL == CU_add_test(pSuite, "equality expression", test_parse_and_expression) ||
        NULL == CU_add_test(pSuite, "exclusive or expression", test_parse_xor_expression) ||
        NULL == CU_add_test(pSuite, "inclusive or expression", test_parse_inclusive_or_expression) ||
        NULL == CU_add_test(pSuite, "logical and expression", test_parse_logical_and_expression) ||
        NULL == CU_add_test(pSuite, "logical or expression", test_parse_logical_or_expression) ||
        NULL == CU_add_test(pSuite, "conditional expression", test_parse_conditional_expression) ||
        NULL == CU_add_test(pSuite, "assignment expression", test_parse_assignment_expression) ||
        NULL == CU_add_test(pSuite, "int declaration specifiers", test_parse_int_declaration_specifiers) ||
        NULL == CU_add_test(pSuite, "invalid declaration specifiers", test_parse_invalid_declaration_specifiers) ||
        NULL == CU_add_test(pSuite, "declaration - empty", test_parse_empty_declaration) ||
        NULL == CU_add_test(pSuite, "declaration - simple", test_parse_simple_declaration) ||
        NULL == CU_add_test(pSuite, "declaration - pointer", test_parse_pointer_declaration) ||
        NULL == CU_add_test(pSuite, "declaration - compound", test_parse_compound_declaration) ||
        NULL == CU_add_test(pSuite, "declaration - function (no parameters)", test_parse_function_declaration_no_parameters) ||
        NULL == CU_add_test(pSuite, "declaration - function (returning pointer)", test_parse_function_declaration_returning_pointer) ||
        NULL == CU_add_test(pSuite, "declaration - array", test_parse_array_declaration) ||
        NULL == CU_add_test(pSuite, "declaration - 2d array", test_parse_2d_array_declaration) ||
        NULL == CU_add_test(pSuite, "declaration - array of functions", test_parse_array_of_functions_declaration) ||
        NULL == CU_add_test(pSuite, "function prototype (void)", test_parse_function_prototype_void) ||
        NULL == CU_add_test(pSuite, "function prototype", test_parse_function_prototype) ||
        NULL == CU_add_test(pSuite, "empty statement", test_parse_empty_statement) ||
        NULL == CU_add_test(pSuite, "expression statement", test_parse_expression_statement) ||
        NULL == CU_add_test(pSuite, "compound statement", test_parse_compound_statement) ||
        NULL == CU_add_test(pSuite, "compound statement with parse error", test_parse_compound_statement_with_error) ||
        NULL == CU_add_test(pSuite, "if statement", test_parse_if_statement) ||
        NULL == CU_add_test(pSuite, "if else statement", test_parse_if_else_statement) ||
        NULL == CU_add_test(pSuite, "if else if else statement", test_parse_if_else_if_else_statement) ||
        NULL == CU_add_test(pSuite, "return statement", test_parse_return_statement) ||
        NULL == CU_add_test(pSuite, "external declaration - declaration", parse_external_declaration_declaration) ||
        NULL == CU_add_test(pSuite, "external declaration - function definition", parse_external_declaration_function_definition) ||
        NULL == CU_add_test(pSuite, "program", test_parse_program)
    ) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    return 0;
}
