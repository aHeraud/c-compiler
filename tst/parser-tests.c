#include <malloc.h>
#include "CUnit/Basic.h"
#include "ast.h"
#include "tests.h"
#include "parser.h"
#include "types.h"
#include "test-common.h"

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
    lexer_global_context_t context = create_lexer_context();
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
    lexer_global_context_t context = create_lexer_context();
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
    lexer_global_context_t context = create_lexer_context();
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
    lexer_global_context_t context = create_lexer_context();
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
    lexer_global_context_t context = create_lexer_context();
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
    lexer_global_context_t context = create_lexer_context();
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
    lexer_global_context_t context = create_lexer_context();
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

void test_parse_postfix_expression_2d_array_subscript() {
    lexer_global_context_t context = create_lexer_context();
    char* input = "arr[i][j]";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t expr;
    CU_ASSERT_TRUE_FATAL(parse_postfix_expression(&parser, &expr))
    expression_t expected = (expression_t) {
        .span = dummy_span(),
        .type = EXPRESSION_ARRAY_SUBSCRIPT,
        .array_subscript = (array_subscript_expression_t) {
            .array = &(expression_t) {
                .span = dummy_span(),
                .type = EXPRESSION_ARRAY_SUBSCRIPT,
                .array_subscript = (array_subscript_expression_t) {
                    .array = make_identifier("arr"),
                    .index = make_identifier("i"),
                },
            },
            .index = make_identifier("j"),
        },
    };
    CU_ASSERT_TRUE_FATAL(expression_eq(&expr, &expected))
}

void test_parse_postfix_expression_member_access() {
    lexer_global_context_t context = create_lexer_context();
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

void test_parse_unary_sizeof_constant() {
    lexer_global_context_t context = create_lexer_context();
    char* input = "sizeof 1";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t expr;
    CU_ASSERT_TRUE_FATAL(parse_unary_expression(&parser, &expr))
    expression_t expected = (expression_t) {
            .span = dummy_span(),
            .type = EXPRESSION_UNARY,
            .unary = (unary_expression_t) {
                    .operator = UNARY_SIZEOF,
                    .operand = integer_constant("1"),
            }
    };
    CU_ASSERT_TRUE_FATAL(expression_eq(&expr, &expected))
}

void test_parse_unary_sizeof_type() {
    lexer_global_context_t context = create_lexer_context();
    char* input = "sizeof(int)";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t expr;
    CU_ASSERT_TRUE_FATAL(parse_unary_expression(&parser, &expr))
    expression_t expected = (expression_t) {
            .span = dummy_span(),
            .type = EXPRESSION_SIZEOF,
            .sizeof_type = &INT,
    };
    CU_ASSERT_TRUE_FATAL(expression_eq(&expr, &expected))
}

void test_parse_unary_sizeof_function_pointer_type() {
    lexer_global_context_t context = create_lexer_context();
    char* input = "sizeof(int (*)(void))";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t expr;
    CU_ASSERT_TRUE_FATAL(parse_unary_expression(&parser, &expr))
    expression_t expected = (expression_t) {
        .span = dummy_span(),
        .type = EXPRESSION_SIZEOF,
        .sizeof_type = pointer_to(&(type_t) {
            .kind = TYPE_FUNCTION,
            .function = {
                .return_type = &INT,
                .parameter_list = &(parameter_type_list_t) {
                    .length = 0,
                    .variadic = false,
                    .parameters = NULL
                },
            },
        })
    };
    CU_ASSERT_TRUE_FATAL(expression_eq(&expr, &expected))
}

void test_parse_unary_sizeof_parenthesized_expression() {
    lexer_global_context_t context = create_lexer_context();
    char* input = "sizeof(1+1)";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t expr;
    CU_ASSERT_TRUE_FATAL(parse_unary_expression(&parser, &expr))
    expression_t expected = (expression_t) {
            .span = dummy_span(),
            .type = EXPRESSION_UNARY,
            .unary = (unary_expression_t) {
                .operator = UNARY_SIZEOF,
                .operand = binary((binary_expression_t) {
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

void test_parse_cast_expression() {
    lexer_global_context_t context = create_lexer_context();
    char* input = "(float) 14";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t expr;
    CU_ASSERT_TRUE_FATAL(parse_cast_expression(&parser, &expr))
    expression_t expected = (expression_t) {
            .span = dummy_span(),
            .type = EXPRESSION_CAST,
            .cast = (cast_expression_t) {
                    .type = &FLOAT,
                    .expression = integer_constant("14"),
            },
    };
    CU_ASSERT_TRUE_FATAL(expression_eq(&expr, &expected))
}

void test_parse_multiplicative_expression() {
    lexer_global_context_t context = create_lexer_context();
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
    lexer_global_context_t context = create_lexer_context();
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
    lexer_global_context_t context = create_lexer_context();
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
    lexer_global_context_t context = create_lexer_context();
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
    lexer_global_context_t context = create_lexer_context();
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
    lexer_global_context_t context = create_lexer_context();
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
    lexer_global_context_t context = create_lexer_context();
    char *input = "1 & 2";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_and_expression(&parser, &node))
    CU_ASSERT_TRUE(lscan(&parser.lexer).kind == TK_EOF)
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
    lexer_global_context_t context = create_lexer_context();
    char *input = "1 ^ 2";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_exclusive_or_expression(&parser, &node))
    CU_ASSERT_TRUE(lscan(&parser.lexer).kind == TK_EOF)
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
    lexer_global_context_t context = create_lexer_context();
    char *input = "1 | 2";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_inclusive_or_expression(&parser, &node))
    CU_ASSERT_TRUE(lscan(&parser.lexer).kind == TK_EOF)
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
    lexer_global_context_t context = create_lexer_context();
    char *input = "1 && 2";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_logical_and_expression(&parser, &node))
    CU_ASSERT_TRUE(lscan(&parser.lexer).kind == TK_EOF)
    expression_t *expected = binary((binary_expression_t) {
            .left = integer_constant("1"),
            .right = integer_constant("2"),
            .type = BINARY_LOGICAL,
            .logical_operator = BINARY_LOGICAL_AND,
            .operator = token(TK_LOGICAL_AND, "&&"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_logical_and_expression_float_operands() {
    lexer_global_context_t context = create_lexer_context();
    char *input = "0.0 && 1.0";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_logical_and_expression(&parser, &node))
    CU_ASSERT_TRUE(lscan(&parser.lexer).kind == TK_EOF)
    expression_t *expected = binary((binary_expression_t) {
        .left = float_constant("0.0"),
        .right = float_constant("1.0"),
        .type = BINARY_LOGICAL,
        .logical_operator = BINARY_LOGICAL_AND,
        .operator = token(TK_LOGICAL_AND, "&&"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_logical_or_expression() {
    lexer_global_context_t context = create_lexer_context();
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
    lexer_global_context_t context = create_lexer_context();
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
    lexer_global_context_t context = create_lexer_context();
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
    lexer_global_context_t context = create_lexer_context();
    char *input = "int";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    type_t type;
    CU_ASSERT_TRUE_FATAL(parse_declaration_specifiers(&parser, &type))
    CU_ASSERT_TRUE_FATAL(types_equal(&type, &INT))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
}

void test_parse_invalid_declaration_specifiers() {
    lexer_global_context_t context = create_lexer_context();
    char *input = "signed float";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    type_t type;
    CU_ASSERT_TRUE_FATAL(parse_declaration_specifiers(&parser, &type))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 1)
    CU_ASSERT_TRUE_FATAL(types_equal(&type, &INT))
}

void test_parse_initializer_expression_simple() {
    lexer_global_context_t context = create_lexer_context();
    char *input = "14;";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    initializer_t initializer;
    CU_ASSERT_TRUE_FATAL(parse_initializer(&parser, &initializer))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
    CU_ASSERT_TRUE_FATAL(initializer.kind == INITIALIZER_EXPRESSION)
    CU_ASSERT_TRUE_FATAL(expression_eq(initializer.expression, integer_constant("14")))
}

void test_parse_initializer_list_array() {
    lexer_global_context_t context = create_lexer_context();
    char *input = "{0, 1, 2}";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    initializer_t initializer;
    CU_ASSERT_TRUE_FATAL(parse_initializer(&parser, &initializer))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
    CU_ASSERT_TRUE_FATAL(initializer.kind == INITIALIZER_LIST)
    CU_ASSERT_EQUAL_FATAL(initializer.list->size, 3)
    for (int i = 0; i < 3; i += 1) {
        initializer_list_element_t element = initializer.list->buffer[i];
        CU_ASSERT_PTR_NULL_FATAL(element.designation)
        CU_ASSERT_TRUE_FATAL(element.initializer->kind == INITIALIZER_EXPRESSION)
        char buffer[8];
        sprintf(buffer, "%d", i);
        CU_ASSERT_TRUE_FATAL(expression_eq(element.initializer->expression, integer_constant(buffer)))
    }
}

void test_parse_initializer_list_array_trailing_comma() {
    lexer_global_context_t context = create_lexer_context();
    char *input = "{0, 1, 2,}";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    initializer_t initializer;
    CU_ASSERT_TRUE_FATAL(parse_initializer(&parser, &initializer))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
    CU_ASSERT_TRUE_FATAL(initializer.kind == INITIALIZER_LIST)
    CU_ASSERT_EQUAL_FATAL(initializer.list->size, 3)
    for (int i = 0; i < 3; i += 1) {
        initializer_list_element_t element = initializer.list->buffer[i];
        CU_ASSERT_PTR_NULL_FATAL(element.designation)
        CU_ASSERT_TRUE_FATAL(element.initializer->kind == INITIALIZER_EXPRESSION)
        char buffer[8];
        sprintf(buffer, "%d", i);
        CU_ASSERT_TRUE_FATAL(expression_eq(element.initializer->expression, integer_constant(buffer)))
    }
}

void test_parse_initializer_list_array_index_designator() {
    lexer_global_context_t context = create_lexer_context();
    char *input = "{[0] = 0}";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    initializer_t initializer;
    CU_ASSERT_TRUE_FATAL(parse_initializer(&parser, &initializer))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
    CU_ASSERT_TRUE_FATAL(initializer.kind == INITIALIZER_LIST)
    CU_ASSERT_EQUAL_FATAL(initializer.list->size, 1)
    initializer_list_element_t element = initializer.list->buffer[0];
    CU_ASSERT_PTR_NOT_NULL_FATAL(element.designation)
    CU_ASSERT_EQUAL_FATAL(element.designation->size, 1)
    designator_t designator = element.designation->buffer[0];
    CU_ASSERT_TRUE_FATAL(designator.kind == DESIGNATOR_INDEX)
    CU_ASSERT_TRUE_FATAL(expression_eq(designator.index, integer_constant("0")))
    CU_ASSERT_TRUE_FATAL(element.initializer->kind == INITIALIZER_EXPRESSION)
    CU_ASSERT_TRUE_FATAL(expression_eq(element.initializer->expression, integer_constant("0")))
}

void test_parse_initializer_list_struct() {
    lexer_global_context_t context = create_lexer_context();
    char *input = "{.a = 0, .b = { .c = 1 }}";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    initializer_t initializer;
    CU_ASSERT_TRUE_FATAL(parse_initializer(&parser, &initializer))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
    CU_ASSERT_TRUE_FATAL(initializer.kind == INITIALIZER_LIST)
    CU_ASSERT_EQUAL_FATAL(initializer.list->size, 2)

    initializer_list_element_t element_a = initializer.list->buffer[0];
    CU_ASSERT_PTR_NOT_NULL_FATAL(element_a.designation)
    CU_ASSERT_EQUAL_FATAL(element_a.designation->size, 1)
    designator_t designator_a = element_a.designation->buffer[0];
    CU_ASSERT_TRUE_FATAL(designator_a.kind == DESIGNATOR_FIELD)
    CU_ASSERT_STRING_EQUAL_FATAL(designator_a.field->value, "a")
    CU_ASSERT_TRUE_FATAL(element_a.initializer->kind == INITIALIZER_EXPRESSION)
    CU_ASSERT_TRUE_FATAL(expression_eq(element_a.initializer->expression, integer_constant("0")))

    initializer_list_element_t element_b = initializer.list->buffer[1];
    CU_ASSERT_PTR_NOT_NULL_FATAL(element_b.designation)
    CU_ASSERT_EQUAL_FATAL(element_b.designation->size, 1)
    designator_t designator_b = element_b.designation->buffer[0];
    CU_ASSERT_TRUE_FATAL(designator_b.kind == DESIGNATOR_FIELD)
    CU_ASSERT_STRING_EQUAL_FATAL(designator_b.field->value, "b")
    CU_ASSERT_TRUE_FATAL(element_b.initializer->kind == INITIALIZER_LIST)
    CU_ASSERT_EQUAL_FATAL(element_b.initializer->list->size, 1)

    initializer_list_element_t element_c = element_b.initializer->list->buffer[0];
    CU_ASSERT_PTR_NOT_NULL_FATAL(element_c.designation)
    CU_ASSERT_EQUAL_FATAL(element_c.designation->size, 1)
    designator_t designator_c = element_c.designation->buffer[0];
    CU_ASSERT_TRUE_FATAL(designator_c.kind == DESIGNATOR_FIELD)
    CU_ASSERT_STRING_EQUAL_FATAL(designator_c.field->value, "c")
}

void test_parse_empty_declaration() {
    lexer_global_context_t context = create_lexer_context();
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
    lexer_global_context_t context = create_lexer_context();
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

void test_parse_simple_declaration_with_initializer() {
    lexer_global_context_t context = create_lexer_context();
    char *input = "int a = 1 & 1;";
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
    CU_ASSERT_TRUE_FATAL(lscan(&parser.lexer).kind == TK_EOF)
    declaration_t expected = (declaration_t) {
            .type = &INT,
            .identifier = token(TK_IDENTIFIER, "a"),
            .initializer = &(initializer_t) {
                .kind = INITIALIZER_EXPRESSION,
                .expression = binary((binary_expression_t) {
                    .type = BINARY_BITWISE,
                    .bitwise_operator = BINARY_BITWISE_AND,
                    .left = integer_constant("1"),
                    .right = integer_constant("1"),
                    .operator = token(TK_AMPERSAND, "&"),
                }),
            },
    };
    CU_ASSERT_TRUE_FATAL(declaration_eq(declarations.buffer[0], &expected))
}

void test_parse_declaration_boolean() {
    lexer_global_context_t context = create_lexer_context();
    char *input = "_Bool a = 1;";
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
            .type = &BOOL,
            .identifier = token(TK_IDENTIFIER, "a"),
            .initializer = & (initializer_t) {
                .kind = INITIALIZER_EXPRESSION,
                .expression = integer_constant("1"),
            },
    };
    CU_ASSERT_TRUE_FATAL(declaration_eq(declarations.buffer[0], &expected))
}

void test_parse_pointer_declaration() {
    lexer_global_context_t context = create_lexer_context();
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
    lexer_global_context_t context = create_lexer_context();
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
            .initializer = & (initializer_t) {
                .kind = INITIALIZER_EXPRESSION,
                .expression = integer_constant("0"),
            },
    };
    CU_ASSERT_TRUE_FATAL(declaration_eq(declarations.buffer[1], &expected_b))

    declaration_t expected_c = (declaration_t) {
            .type = &INT,
            .identifier = token(TK_IDENTIFIER, "c"),
            .initializer = & (initializer_t) {
                .kind = INITIALIZER_EXPRESSION,
                .expression = binary((binary_expression_t) {
                    .type = BINARY_ARITHMETIC,
                    .arithmetic_operator = BINARY_ARITHMETIC_ADD,
                    .left = make_identifier("d"),
                    .right = integer_constant("1"),
                    .operator = token(TK_PLUS, "+"),
                }),
            },
    };
    CU_ASSERT_TRUE_FATAL(declaration_eq(declarations.buffer[2], &expected_c))
}

void test_parse_function_declaration_no_parameters() {
    lexer_global_context_t context = create_lexer_context();
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

void test_parse_function_declaration_with_parameters() {
    lexer_global_context_t context = create_lexer_context();
    // combination of abstract declarator and direct declarator parameters
    char *input = "int foo(int a, float (*)(void), ...);";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    ptr_vector_t declarations = { .size = 0, .capacity = 0, .buffer = NULL, };
    CU_ASSERT_TRUE_FATAL(parse_declaration(&parser, &declarations))
    CU_ASSERT_EQUAL_FATAL(declarations.size, 1)
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)

    parameter_type_list_t parameter_type_list = {
        .variadic = true,
        .length = 2,
        .parameters = (parameter_declaration_t*[2]) {
            &(parameter_declaration_t) {
                .type = &INT,
                .identifier = token(TK_IDENTIFIER, "a"),
            },
            &(parameter_declaration_t) {
                .type = pointer_to(&(type_t) {
                    .kind = TYPE_FUNCTION,
                    .function = {
                        .return_type = &FLOAT,
                        .parameter_list = &(parameter_type_list_t) {
                            .variadic = false,
                            .length = 0,
                            .parameters = NULL,
                        },
                    },
                }),
            },
        }
    };

    declaration_t expected = (declaration_t) {
        .type = &(type_t) {
            .kind = TYPE_FUNCTION,
            .function = {
                .return_type = &INT,
                .parameter_list = &parameter_type_list,
            },
        },
        .identifier = token(TK_IDENTIFIER, "foo"),
        .initializer = NULL,
    };

    CU_ASSERT_TRUE_FATAL(declaration_eq(declarations.buffer[0], &expected))
}

void test_parse_function_declaration_returning_pointer() {
    lexer_global_context_t context = create_lexer_context();
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
    lexer_global_context_t context = create_lexer_context();
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

void test_parse_array_declaration_with_initializer() {
    lexer_global_context_t context = create_lexer_context();
    char *input = "int arr[3] = { 1, 2, 3 };";
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
            .size = integer_constant("3"),
        },
    };
    CU_ASSERT_STRING_EQUAL_FATAL(declaration->identifier->value, "arr")
    CU_ASSERT_TRUE_FATAL(types_equal(declaration->type, &expected_type))
}

void test_parse_2d_array_declaration() {
    lexer_global_context_t context = create_lexer_context();
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
    lexer_global_context_t context = create_lexer_context();
    char* input = "int foo[](void);";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    ptr_vector_t declarations = { .size = 0, .capacity = 0, .buffer = NULL, };
    CU_ASSERT_TRUE_FATAL(parse_declaration(&parser, &declarations))
    CU_ASSERT_EQUAL_FATAL(declarations.size, 1)
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)

    parameter_type_list_t parameter_list = (parameter_type_list_t) {
            .variadic = false,
            .parameters = NULL,
            .length = 0,
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

void test_parse_function_pointer() {
    lexer_global_context_t context = create_lexer_context();
    char* input = "int (*foo)(void);";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    ptr_vector_t declarations = { .size = 0, .capacity = 0, .buffer = NULL, };
    CU_ASSERT_TRUE_FATAL(parse_declaration(&parser, &declarations))
    CU_ASSERT_EQUAL_FATAL(declarations.size, 1)
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)

    type_t expected = {
        .storage_class = STORAGE_CLASS_AUTO,
        .is_volatile = false,
        .is_const = false,
        .kind = TYPE_POINTER,
        .pointer = {
            .is_const = false,
            .is_volatile = false,
            .is_restrict = false,
            .base = &(type_t) {
                .storage_class = STORAGE_CLASS_AUTO,
                .is_volatile = false,
                .is_const = false,
                .kind = TYPE_FUNCTION,
                .function = {
                    .return_type = &INT,
                    .parameter_list = &(parameter_type_list_t) {
                        .variadic = false,
                        .parameters = NULL,
                        .length = 0,
                    },
                },
            }
        }
    };

    declaration_t *declaration = declarations.buffer[0];
    CU_ASSERT_TRUE_FATAL(types_equal(declaration->type, &expected));
    CU_ASSERT_STRING_EQUAL_FATAL(declaration->identifier->value, "foo");
}

void test_parse_complex_declaration() {
    lexer_global_context_t context = create_lexer_context();
    char* input = "float *(*(*bar[1][2])(void))(int);";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    ptr_vector_t declarations = { .size = 0, .capacity = 0, .buffer = NULL, };
    CU_ASSERT_TRUE_FATAL(parse_declaration(&parser, &declarations))
    CU_ASSERT_EQUAL_FATAL(declarations.size, 1)
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)

    type_t expected = {
        .storage_class = STORAGE_CLASS_AUTO,
        .is_const = false,
        .is_volatile = false,
        .kind = TYPE_ARRAY,
        .array = {
            .size = integer_constant("1"),
            .element_type = &(type_t) {
                .storage_class = STORAGE_CLASS_AUTO,
                .is_const = false,
                .is_volatile = false,
                .kind = TYPE_ARRAY,
                .array = {
                    .size = integer_constant("2"),
                    .element_type = ptr_to(&(type_t) {
                        .storage_class = STORAGE_CLASS_AUTO,
                        .is_const = false,
                        .is_volatile = false,
                        .kind = TYPE_FUNCTION,
                        .function = {
                            .parameter_list = &(parameter_type_list_t) {
                                .variadic = false,
                                .parameters = NULL,
                                .length = 0,
                            },
                            .return_type = ptr_to(&(type_t) {
                                .storage_class = STORAGE_CLASS_AUTO,
                                .is_const = false,
                                .is_volatile = false,
                                .kind = TYPE_FUNCTION,
                                .function = {
                                    .return_type = ptr_to(&FLOAT),
                                    .parameter_list = &(parameter_type_list_t) {
                                        .variadic = false,
                                        .parameters = (parameter_declaration_t*[]) {
                                            &(parameter_declaration_t) {
                                                .type = &INT,
                                                .identifier = NULL,
                                            },
                                        },
                                        .length = 1,
                                    },
                                }
                            }),
                        }
                    })
                }
            }
        }
    };

    declaration_t *declaration = declarations.buffer[0];
    CU_ASSERT_TRUE_FATAL(types_equal(declaration->type, &expected));
    CU_ASSERT_STRING_EQUAL_FATAL(declaration->identifier->value, "bar");
}

void test_parse_function_prototype_void() {
    lexer_global_context_t context = create_lexer_context();
    char *input = "float foo(void);";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    ptr_vector_t declarations = { .size = 0, .capacity = 0, .buffer = NULL, };
    CU_ASSERT_TRUE_FATAL(parse_declaration(&parser, &declarations))
    CU_ASSERT_EQUAL_FATAL(declarations.size, 1)
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)

    type_t type = {
        .kind = TYPE_FUNCTION,
        .function = {
            .return_type = &FLOAT,
            .parameter_list = &(parameter_type_list_t) {
                .variadic = false,
                .parameters = NULL,
                .length = 0,
            },
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
    lexer_global_context_t context = create_lexer_context();
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
    lexer_global_context_t context = create_lexer_context();
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
    lexer_global_context_t context = create_lexer_context();
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
    lexer_global_context_t context = create_lexer_context();
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
    lexer_global_context_t context = create_lexer_context();
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
    lexer_global_context_t context = create_lexer_context();
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
    lexer_global_context_t context = create_lexer_context();
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
    lexer_global_context_t context = create_lexer_context();
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
    lexer_global_context_t context = create_lexer_context();
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

void test_parse_while_statement() {
    lexer_global_context_t context = create_lexer_context();
    char *input = "while (cond > 0) { cond = cond - 1; }";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    statement_t node;

    // Assert that it was parsed successfully, and that the parser consumed all of the input.
    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &node))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
    CU_ASSERT_TRUE_FATAL(lscan(&parser.lexer).kind == TK_EOF)

    // Make sure the statement is parsed correctly
    // We have other tests to validate the condition and body, so just make sure
    // they're present and have the expected types.
    CU_ASSERT_EQUAL_FATAL(node.type, STATEMENT_WHILE)
    CU_ASSERT_EQUAL_FATAL(node.while_.condition->type, EXPRESSION_BINARY)
    CU_ASSERT_EQUAL_FATAL(node.while_.body->type, STATEMENT_COMPOUND)
}

void test_parse_while_statement_with_empty_body() {
    lexer_global_context_t context = create_lexer_context();
    char *input = "while (1);";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    statement_t node;

    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &node))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
    CU_ASSERT_TRUE_FATAL(lscan(&parser.lexer).kind == TK_EOF)

    CU_ASSERT_EQUAL_FATAL(node.type, STATEMENT_WHILE)
    CU_ASSERT_EQUAL_FATAL(node.while_.condition->type, EXPRESSION_PRIMARY)
    CU_ASSERT_EQUAL_FATAL(node.while_.body->type, STATEMENT_EMPTY)
}

void test_parse_for_statement() {
    lexer_global_context_t context = create_lexer_context();
    const char *input =
        "for (int i = 0; i < 10; i = i + 1) {\n"
        "    a = a + i;\n"
        "}";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    statement_t node;

    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &node))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
    CU_ASSERT_TRUE_FATAL(lscan(&parser.lexer).kind == TK_EOF)

    CU_ASSERT_EQUAL_FATAL(node.type, STATEMENT_FOR)
    CU_ASSERT_EQUAL_FATAL(node.for_.initializer.kind, FOR_INIT_DECLARATION)
    CU_ASSERT_PTR_NOT_NULL_FATAL(node.for_.initializer.declarations)

    CU_ASSERT_PTR_NOT_NULL_FATAL(node.for_.condition)
    CU_ASSERT_EQUAL_FATAL(node.for_.condition->type, EXPRESSION_BINARY)

    CU_ASSERT_PTR_NOT_NULL_FATAL(node.for_.post)
    CU_ASSERT_EQUAL_FATAL(node.for_.post->type, EXPRESSION_BINARY)

    CU_ASSERT_EQUAL_FATAL(node.for_.body->type, STATEMENT_COMPOUND)
}

void test_parse_for_statement_no_optional_parts() {
    lexer_global_context_t context = create_lexer_context();
    const char *input = "for (;;);";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    statement_t node;

    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &node))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
    CU_ASSERT_TRUE_FATAL(lscan(&parser.lexer).kind == TK_EOF)

    CU_ASSERT_EQUAL_FATAL(node.type, STATEMENT_FOR)
    CU_ASSERT_EQUAL_FATAL(node.for_.initializer.kind, FOR_INIT_EMPTY)
    CU_ASSERT_PTR_NULL_FATAL(node.for_.condition)
    CU_ASSERT_PTR_NULL_FATAL(node.for_.post)
    CU_ASSERT_EQUAL_FATAL(node.for_.body->type, STATEMENT_EMPTY)
}

void test_parse_for_statement_expr_initializer() {
    lexer_global_context_t context = create_lexer_context();
    const char *input = "for (i = 0;;);";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    statement_t node;

    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &node))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
    CU_ASSERT_TRUE_FATAL(lscan(&parser.lexer).kind == TK_EOF)

    CU_ASSERT_EQUAL_FATAL(node.type, STATEMENT_FOR)
    CU_ASSERT_EQUAL_FATAL(node.for_.initializer.kind, FOR_INIT_EXPRESSION)
    CU_ASSERT_PTR_NOT_NULL_FATAL(node.for_.initializer.expression)
}

void parse_external_declaration_declaration() {
    lexer_global_context_t context = create_lexer_context();
    char *input = "int a = 4;";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    external_declaration_t node;
    CU_ASSERT_TRUE_FATAL(parse_external_declaration(&parser, &node))
    declaration_t expected = (declaration_t ) {
        .type = &INT,
        .identifier = token(TK_IDENTIFIER, "a"),
        .initializer = & (initializer_t) {
            .kind = INITIALIZER_EXPRESSION,
            .expression = integer_constant("4"),
        },
    };

    CU_ASSERT_TRUE_FATAL(node.type == EXTERNAL_DECLARATION_DECLARATION)
    CU_ASSERT_EQUAL_FATAL(node.declaration.length, 1)
    CU_ASSERT_TRUE_FATAL(declaration_eq(node.declaration.declarations[0], &expected))
}

void parse_external_definition_prototype_var_args() {
    lexer_global_context_t context = create_lexer_context();
    char *input = "int printf(const char *format, ...);";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    external_declaration_t node;
    CU_ASSERT_TRUE_FATAL(parse_external_declaration(&parser, &node))
    CU_ASSERT_TRUE_FATAL(node.type == EXTERNAL_DECLARATION_DECLARATION)

    declaration_t *declaration = node.declaration.declarations[0];

    type_t *expected_type = &(type_t) {
        .kind = TYPE_FUNCTION,
        .is_const = false,
        .is_volatile = false,
        .function = {
            .return_type = &INT,
            .parameter_list = &(parameter_type_list_t) {
                .variadic = true,
                .parameters = (parameter_declaration_t*[]) {
                    &(parameter_declaration_t) {
                        .type = pointer_to(&CHAR),
                        .identifier = token(TK_IDENTIFIER, "format"),
                    },
                },
                .length = 1,
            },
        }
    };

    CU_ASSERT_TRUE_FATAL(types_equal(declaration->type, expected_type))
    CU_ASSERT_TRUE_FATAL(strcmp(declaration->identifier->value, "printf") == 0)
}

void parse_external_declaration_function_definition() {
    lexer_global_context_t context = create_lexer_context();
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

void parse_external_definition_function_taking_void() {
    lexer_global_context_t context = create_lexer_context();
    char *input = "int main(void) { return 0; }";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);

    external_declaration_t node;
    CU_ASSERT_TRUE_FATAL(parse_external_declaration(&parser, &node))
    CU_ASSERT_TRUE_FATAL(node.type == EXTERNAL_DECLARATION_FUNCTION_DEFINITION)

    CU_ASSERT_TRUE_FATAL(types_equal(node.function_definition->return_type, &INT))
    CU_ASSERT_TRUE_FATAL(strcmp(node.function_definition->identifier->value, "main") == 0)

    // validate the argument list
    CU_ASSERT_EQUAL_FATAL(node.function_definition->parameter_list->length, 0)

    // validate the body is parsed correctly
    statement_t *ret = {
        return_statement(integer_constant("0")),
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
    lexer_global_context_t context = create_lexer_context();
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
        NULL == CU_add_test(pSuite, "postfix expression - multiple postfix expressions", test_parse_postfix_expression_2d_array_subscript) ||
        NULL == CU_add_test(pSuite, "postfix expression - member access", test_parse_postfix_expression_member_access) ||
        NULL == CU_add_test(pSuite, "unary expression - sizeof constant", test_parse_unary_sizeof_constant) ||
        NULL == CU_add_test(pSuite, "unary expression - sizeof (type)", test_parse_unary_sizeof_type) ||
        NULL == CU_add_test(pSuite, "unary expression - sizeof (more complicated type)", test_parse_unary_sizeof_function_pointer_type) ||
        NULL == CU_add_test(pSuite, "unary expression - sizeof (expression)", test_parse_unary_sizeof_parenthesized_expression) ||
        NULL == CU_add_test(pSuite, "cast expression", test_parse_cast_expression) ||
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
        NULL == CU_add_test(pSuite, "logical and expression (float operands)", test_parse_logical_and_expression_float_operands) ||
        NULL == CU_add_test(pSuite, "logical or expression", test_parse_logical_or_expression) ||
        NULL == CU_add_test(pSuite, "conditional expression", test_parse_conditional_expression) ||
        NULL == CU_add_test(pSuite, "assignment expression", test_parse_assignment_expression) ||
        NULL == CU_add_test(pSuite, "int declaration specifiers", test_parse_int_declaration_specifiers) ||
        NULL == CU_add_test(pSuite, "invalid declaration specifiers", test_parse_invalid_declaration_specifiers) ||
        NULL == CU_add_test(pSuite, "initializer - expression simple", test_parse_initializer_expression_simple) ||
        NULL == CU_add_test(pSuite, "initializer - array of integers", test_parse_initializer_list_array) ||
        NULL == CU_add_test(pSuite, "initializer - array of integers with trailing comma", test_parse_initializer_list_array_trailing_comma) ||
        NULL == CU_add_test(pSuite, "initializer - array index designator", test_parse_initializer_list_array_index_designator) ||
        NULL == CU_add_test(pSuite, "initializer - struct", test_parse_initializer_list_struct),
        NULL == CU_add_test(pSuite, "declaration - empty", test_parse_empty_declaration) ||
        NULL == CU_add_test(pSuite, "declaration - simple", test_parse_simple_declaration) ||
        NULL == CU_add_test(pSuite, "declaration - simple with initializer", test_parse_simple_declaration_with_initializer) ||
        NULL == CU_add_test(pSuite, "declaration - boolean", test_parse_declaration_boolean) ||
        NULL == CU_add_test(pSuite, "declaration - pointer", test_parse_pointer_declaration) ||
        NULL == CU_add_test(pSuite, "declaration - compound", test_parse_compound_declaration) ||
        NULL == CU_add_test(pSuite, "declaration - function (no parameters)", test_parse_function_declaration_no_parameters) ||
        NULL == CU_add_test(pSuite, "declaration - function (with parameters)", test_parse_function_declaration_with_parameters) ||
        NULL == CU_add_test(pSuite, "declaration - function (returning pointer)", test_parse_function_declaration_returning_pointer) ||
        NULL == CU_add_test(pSuite, "declaration - array", test_parse_array_declaration) ||
        NULL == CU_add_test(pSuite, "declaration - array with initializer", test_parse_array_declaration_with_initializer) ||
        NULL == CU_add_test(pSuite, "declaration - 2d array", test_parse_2d_array_declaration) ||
        NULL == CU_add_test(pSuite, "declaration - array of functions", test_parse_array_of_functions_declaration) ||
        NULL == CU_add_test(pSuite, "declaration - function pointer", test_parse_function_pointer) ||
        NULL == CU_add_test(pSuite, "declaration - complex", test_parse_complex_declaration) ||
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
        NULL == CU_add_test(pSuite, "while statement", test_parse_while_statement) ||
        NULL == CU_add_test(pSuite, "while statement with empty body", test_parse_while_statement_with_empty_body) ||
        NULL == CU_add_test(pSuite, "for statement", test_parse_for_statement) ||
        NULL == CU_add_test(pSuite, "for statement with no optional parts", test_parse_for_statement_no_optional_parts) ||
        NULL == CU_add_test(pSuite, "for statement with expression initializer", test_parse_for_statement_expr_initializer) ||
        NULL == CU_add_test(pSuite, "external declaration - declaration", parse_external_declaration_declaration) ||
        NULL == CU_add_test(pSuite, "external declaration - prototype (var args)", parse_external_definition_prototype_var_args) ||
        NULL == CU_add_test(pSuite, "external declaration - function definition", parse_external_declaration_function_definition) ||
        NULL == CU_add_test(pSuite, "external declaration - function (void) definition", parse_external_definition_function_taking_void) ||
        NULL == CU_add_test(pSuite, "program", test_parse_program)
    ) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    return 0;
}
