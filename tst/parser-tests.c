#include <malloc.h>
#include "CUnit/Basic.h"
#include "ast.h"
#include "tests.h"
#include "parser/parser.h"
#include "types.h"
#include "test-common.h"

expression_t *make_identifier(char* value) {
    return primary((primary_expression_t) {
        .kind = PE_IDENTIFIER,
        .value.token = (token_t) {
            .kind = TK_IDENTIFIER,
            .value = value,
            .position = dummy_position(),
        },
    });
}

expression_t *binary(binary_expression_t binary) {
    expression_t *expr = malloc(sizeof(expression_t));
    *expr = (expression_t) {
        .kind = EXPRESSION_BINARY,
        .span = dummy_span(),
        .value.binary = binary,
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
        .kind = STATEMENT_RETURN,
        .value.return_ = {
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
        .kind = STATEMENT_EXPRESSION,
        .value.expression = expression,
        .terminator = token(TK_SEMICOLON, ";"),
    };
    return stmt;
}

statement_t *if_statement(expression_t *condition, statement_t *true_branch, statement_t *false_branch) {
    statement_t *stmt = malloc(sizeof(statement_t));
    *stmt = (statement_t) {
        .kind = STATEMENT_IF,
        .value.if_ = {
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
        .kind = BLOCK_ITEM_STATEMENT,
        .value.statement = statement,
    };
    return item;
}

block_item_t *block_item_d(declaration_t *declaration) {
    block_item_t *item = malloc(sizeof(block_item_t));
    *item = (block_item_t) {
        .kind = BLOCK_ITEM_DECLARATION,
        .value.declaration = declaration,
    };
    return item;
}

const type_t *pointer_to(const type_t *type) {
    type_t *pointer = malloc(sizeof(type_t));
    *pointer = (type_t) {
        .kind = TYPE_POINTER,
        .value.pointer = {
            .base = type,
            .is_const = false,
            .is_volatile = false,
            .is_restrict = false,
        },
    };
    return pointer;
}

void test_parse_primary_expression_ident(void) {
        char* input = "bar";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_primary_expression(&parser, &node))
    expression_t *expected = primary((primary_expression_t) {
        .kind = PE_IDENTIFIER,
        .value.token = (token_t) {
            .kind = TK_IDENTIFIER,
            .value = "bar",
            .position = dummy_position(),
        },
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_primary_expression_int(void) {
        char* input = "42";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_primary_expression(&parser, &node))
    expression_t *expected = primary((primary_expression_t) {
        .kind = PE_CONSTANT,
        .value.token = (token_t) {
            .kind = TK_INTEGER_CONSTANT,
            .value = "42",
            .position = dummy_position(),
        },
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_primary_expression_float(void) {
        char* input = "42.0";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_primary_expression(&parser, &node))
    expression_t *expected = primary((primary_expression_t) {
        .kind = PE_CONSTANT,
        .value.token = (token_t) {
            .kind = TK_FLOATING_CONSTANT,
            .value = "42.0",
            .position = dummy_position(),
        },
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_primary_expression_char(void) {
        char* input = "'a'";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t node;
    bool matches = parse_primary_expression(&parser, &node);
    CU_ASSERT_TRUE_FATAL(matches)
    CU_ASSERT_EQUAL_FATAL(node.kind, EXPRESSION_PRIMARY)
    CU_ASSERT_EQUAL_FATAL(node.value.primary.kind, PE_CONSTANT)
    CU_ASSERT_EQUAL_FATAL(node.value.primary.value.token.kind, TK_CHAR_LITERAL)
    CU_ASSERT_STRING_EQUAL_FATAL(node.value.primary.value.token.value, "'a'")
}

void test_parse_primary_expression_parenthesized(void) {
        char* input = "(42)";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t expr;
    CU_ASSERT_TRUE_FATAL(parse_primary_expression(&parser, &expr))
    expression_t *expected = primary((primary_expression_t) {
        .kind = PE_EXPRESSION,
        .value.expression = integer_constant("42"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&expr, expected))
}

void test_parse_primary_expression_parenthesized_identifier(void) {
        char* input = "(count)";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t expr;
    CU_ASSERT_TRUE_FATAL(parse_expression(&parser, &expr))
    expression_t *expected = primary((primary_expression_t) {
        .kind = PE_EXPRESSION,
        .value.expression = make_identifier("count"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&expr, expected))
}

void test_parse_postfix_expression_function_call(void) {
        char* input = "pow(4,2)";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
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
        .kind = EXPRESSION_CALL,
        .value.call = (call_expression_t) {
            .callee = make_identifier("pow"),
            .arguments = arguments,
        },
    };
    CU_ASSERT_TRUE_FATAL(expression_eq(&expr, &expected))
}

void test_parse_postfix_expression_array_subscript(void) {
        char* input = "arr[1 + 1]";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t expr;
    CU_ASSERT_TRUE_FATAL(parse_postfix_expression(&parser, &expr))
    expression_t expected = (expression_t) {
            .span = dummy_span(),
            .kind = EXPRESSION_ARRAY_SUBSCRIPT,
            .value.array_subscript = (array_subscript_expression_t) {
                .array = make_identifier("arr"),
                .index = binary((binary_expression_t) {
                    .kind = BINARY_ARITHMETIC,
                    .left = integer_constant("1"),
                    .right = integer_constant("1"),
                    .operator_token = token(TK_PLUS, "+"),
                    .operator.arithmetic = BINARY_ARITHMETIC_ADD,
                }),
            },
    };
    CU_ASSERT_TRUE_FATAL(expression_eq(&expr, &expected))
}

void test_parse_postfix_expression_2d_array_subscript(void) {
        char* input = "arr[i][j]";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t expr;
    CU_ASSERT_TRUE_FATAL(parse_postfix_expression(&parser, &expr))
    expression_t expected = (expression_t) {
        .span = dummy_span(),
        .kind = EXPRESSION_ARRAY_SUBSCRIPT,
        .value.array_subscript = (array_subscript_expression_t) {
            .array = &(expression_t) {
                .span = dummy_span(),
                .kind = EXPRESSION_ARRAY_SUBSCRIPT,
                .value.array_subscript = (array_subscript_expression_t) {
                    .array = make_identifier("arr"),
                    .index = make_identifier("i"),
                },
            },
            .index = make_identifier("j"),
        },
    };
    CU_ASSERT_TRUE_FATAL(expression_eq(&expr, &expected))
}

void test_parse_postfix_expression_member_access(void) {
        char* input = "foo.bar";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t expr;
    CU_ASSERT_TRUE_FATAL(parse_postfix_expression(&parser, &expr))
    expression_t expected = (expression_t) {
        .span = dummy_span(),
        .kind = EXPRESSION_MEMBER_ACCESS,
        .value.member_access = (member_access_expression_t) {
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

void test_parse_type_name_function_pointer(void) {
        char* input = "int (*)(void)";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);

    type_t *type;
    CU_ASSERT_TRUE_FATAL(parse_type_name(&parser, &type))
}

void test_parse_unary_sizeof_constant(void) {
        char* input = "sizeof 1";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t expr;
    CU_ASSERT_TRUE_FATAL(parse_unary_expression(&parser, &expr))
    expression_t expected = (expression_t) {
            .span = dummy_span(),
            .kind = EXPRESSION_UNARY,
            .value.unary = (unary_expression_t) {
                    .operator = UNARY_SIZEOF,
                    .operand = integer_constant("1"),
            }
    };
    CU_ASSERT_TRUE_FATAL(expression_eq(&expr, &expected))
}

void test_parse_unary_sizeof_type(void) {
        char* input = "sizeof(int)";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t expr;
    CU_ASSERT_TRUE_FATAL(parse_unary_expression(&parser, &expr))
    expression_t expected = (expression_t) {
            .span = dummy_span(),
            .kind = EXPRESSION_SIZEOF,
            .value.type = &INT,
    };
    CU_ASSERT_TRUE_FATAL(expression_eq(&expr, &expected))
}

void test_parse_unary_sizeof_function_pointer_type(void) {
        char* input = "sizeof(int (*)(void))";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t expr;
    CU_ASSERT_TRUE_FATAL(parse_unary_expression(&parser, &expr))
    expression_t expected = (expression_t) {
        .span = dummy_span(),
        .kind = EXPRESSION_SIZEOF,
        .value.type = pointer_to(&(type_t) {
            .kind = TYPE_FUNCTION,
            .value.function = {
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

void test_parse_unary_sizeof_parenthesized_expression(void) {
        char* input = "sizeof(1+1)";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t expr;
    CU_ASSERT_TRUE_FATAL(parse_unary_expression(&parser, &expr))
    expression_t expected = (expression_t) {
            .span = dummy_span(),
            .kind = EXPRESSION_UNARY,
            .value.unary = (unary_expression_t) {
                .operator = UNARY_SIZEOF,
                .operand = binary((binary_expression_t) {
                    .kind = BINARY_ARITHMETIC,
                    .left = integer_constant("1"),
                    .right = integer_constant("1"),
                    .operator_token = token(TK_PLUS, "+"),
                    .operator.arithmetic = BINARY_ARITHMETIC_ADD,
                }),
            },
    };
    CU_ASSERT_TRUE_FATAL(expression_eq(&expr, &expected))
}

void test_parse_prefix_increment(void) {
        char *input = "++a";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t expr;
    CU_ASSERT_TRUE_FATAL(parse_expression(&parser, &expr))
    expression_t expected = (expression_t) {
        .span = dummy_span(),
        .kind = EXPRESSION_UNARY,
        .value.unary = {
            .operator = UNARY_PRE_INCREMENT,
            .operand = primary((primary_expression_t) {
                .kind = PE_IDENTIFIER,
                .value.token = *token(TK_IDENTIFIER, "a"),
            }),
        },
    };
    CU_ASSERT_TRUE_FATAL(expression_eq(&expected,  &expr))
}

void test_parse_prefix_decrement(void) {
        char *input = "--b";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t expr;
    CU_ASSERT_TRUE_FATAL(parse_expression(&parser, &expr))
    expression_t expected = (expression_t) {
        .span = dummy_span(),
        .kind = EXPRESSION_UNARY,
        .value.unary = {
            .operator = UNARY_PRE_DECREMENT,
            .operand = primary((primary_expression_t) {
                .kind = PE_IDENTIFIER,
                .value.token = *token(TK_IDENTIFIER, "b"),
            }),
        },
    };
    CU_ASSERT_TRUE_FATAL(expression_eq(&expected,  &expr))
}

void test_parse_postfix_increment(void) {
        char *input = "a++";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t expr;
    CU_ASSERT_TRUE_FATAL(parse_expression(&parser, &expr))
    expression_t expected = (expression_t) {
        .span = dummy_span(),
        .kind = EXPRESSION_UNARY,
        .value.unary = {
            .operator = UNARY_POST_INCREMENT,
            .operand = primary((primary_expression_t) {
                .kind = PE_IDENTIFIER,
                .value.token = *token(TK_IDENTIFIER, "a"),
            }),
        },
    };
    CU_ASSERT_TRUE_FATAL(expression_eq(&expected,  &expr))
}

void test_parse_postfix_decrement(void) {
        char *input = "b--";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t expr;
    CU_ASSERT_TRUE_FATAL(parse_expression(&parser, &expr))
    expression_t expected = (expression_t) {
        .span = dummy_span(),
        .kind = EXPRESSION_UNARY,
        .value.unary = {
            .operator = UNARY_POST_DECREMENT,
            .operand = primary((primary_expression_t) {
                .kind = PE_IDENTIFIER,
                .value.token = *token(TK_IDENTIFIER, "b"),
            }),
        },
    };
    CU_ASSERT_TRUE_FATAL(expression_eq(&expected,  &expr))
}

void test_parse_cast_expression(void) {
        char* input = "(float) 14";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t expr;
    CU_ASSERT_TRUE_FATAL(parse_cast_expression(&parser, &expr))
    expression_t expected = (expression_t) {
            .span = dummy_span(),
            .kind = EXPRESSION_CAST,
            .value.cast = (cast_expression_t) {
                    .type = &FLOAT,
                    .expression = integer_constant("14"),
            },
    };
    CU_ASSERT_TRUE_FATAL(expression_eq(&expr, &expected))
}

void test_parse_multiplicative_expression(void) {
        char *input = "1 / 2 * 3 % 4";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_multiplicative_expression(&parser, &node))
    expression_t *expected = binary((binary_expression_t) {
        .left = binary((binary_expression_t) {
            .left = binary((binary_expression_t) {
                .kind = BINARY_ARITHMETIC,
                .operator.arithmetic = BINARY_ARITHMETIC_DIVIDE,
                .left = integer_constant("1"),
                .right = integer_constant("2"),
                .operator_token = token(TK_SLASH, "/"),
            }),
            .right = integer_constant("3"),
            .kind = BINARY_ARITHMETIC,
            .operator.arithmetic = BINARY_ARITHMETIC_MULTIPLY,
            .operator_token = token(TK_STAR, "*"),
        }),
        .right = integer_constant("4"),
        .kind = BINARY_ARITHMETIC,
        .operator.arithmetic = BINARY_ARITHMETIC_MODULO,
        .operator_token = token(TK_PERCENT, "%"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_additive_expression(void) {
        char *input = "1 + 2 - 3";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_additive_expression(&parser, &node))
    expression_t *expected = binary((binary_expression_t) {
        .left = binary((binary_expression_t) {
            .left = integer_constant("1"),
            .right = integer_constant("2"),
            .kind = BINARY_ARITHMETIC,
            .operator.arithmetic = BINARY_ARITHMETIC_ADD,
            .operator_token = token(TK_PLUS, "+"),
        }),
        .right = integer_constant("3"),
        .kind = BINARY_ARITHMETIC,
        .operator.arithmetic = BINARY_ARITHMETIC_SUBTRACT,
        .operator_token = token(TK_MINUS, "-"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_additive_expression_2(void) {
        char *input = "1 + 2 * 3;";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t expr;

    CU_ASSERT_TRUE_FATAL(parse_additive_expression(&parser, &expr))

    expression_t *expected = binary((binary_expression_t) {
        .left = integer_constant("1"),
        .right = binary((binary_expression_t) {
            .left = integer_constant("2"),
            .right = integer_constant("3"),
            .kind = BINARY_ARITHMETIC,
            .operator.arithmetic = BINARY_ARITHMETIC_MULTIPLY,
            .operator_token = token(TK_STAR, "*"),
        }),
        .operator_token = token(TK_PLUS, "+"),
        .kind = BINARY_ARITHMETIC,
        .operator.arithmetic = BINARY_ARITHMETIC_ADD,
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&expr, expected))
}

void test_parse_shift_expression(void) {
        char *input = "1 << 2 >> 3";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_shift_expression(&parser, &node))
    expression_t *expected = binary((binary_expression_t) {
        .left = binary((binary_expression_t) {
            .left = integer_constant("1"),
            .right = integer_constant("2"),
            .kind = BINARY_BITWISE,
            .operator.bitwise = BINARY_BITWISE_SHIFT_LEFT,
            .operator_token = token(TK_LSHIFT, "<<"),
        }),
        .right = integer_constant("3"),
        .kind = BINARY_BITWISE,
        .operator.bitwise = BINARY_BITWISE_SHIFT_RIGHT,
        .operator_token = token(TK_RSHIFT, ">>"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_relational_expression(void) {
        char *input = "1 < 2 > 3 <= 4 >= 5";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_relational_expression(&parser, &node))
    expression_t *expected = binary((binary_expression_t) {
        .left = binary((binary_expression_t) {
            .left = binary((binary_expression_t) {
                .left = binary((binary_expression_t) {
                    .left = integer_constant("1"),
                    .right = integer_constant("2"),
                    .kind = BINARY_COMPARISON,
                    .operator.comparison = BINARY_COMPARISON_LESS_THAN,
                    .operator_token = token(TK_LESS_THAN, "<"),
                }),
                .right = integer_constant("3"),
                .kind = BINARY_COMPARISON,
                .operator.comparison = BINARY_COMPARISON_GREATER_THAN,
                .operator_token = token(TK_GREATER_THAN, ">"),
            }),
            .right = integer_constant("4"),
            .kind = BINARY_COMPARISON,
            .operator.comparison = BINARY_COMPARISON_LESS_THAN_OR_EQUAL,
            .operator_token = token(TK_LESS_THAN_EQUAL, "<="),
        }),
        .right = integer_constant("5"),
        .kind = BINARY_COMPARISON,
        .operator.comparison = BINARY_COMPARISON_GREATER_THAN_OR_EQUAL,
        .operator_token = token(TK_GREATER_THAN_EQUAL, ">="),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_equality_expression(void) {
        char *input = "1 == 2 != 3";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_equality_expression(&parser, &node))
    expression_t *expected = binary((binary_expression_t) {
        .left = binary((binary_expression_t) {
            .left = integer_constant("1"),
            .right = integer_constant("2"),
            .kind = BINARY_COMPARISON,
            .operator.comparison = BINARY_COMPARISON_EQUAL,
            .operator_token = token(TK_EQUALS, "=="),
        }),
        .right = integer_constant("3"),
        .kind = BINARY_COMPARISON,
        .operator.comparison = BINARY_COMPARISON_NOT_EQUAL,
        .operator_token = token(TK_NOT_EQUALS, "!="),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_and_expression(void) {
        char *input = "1 & 2";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_and_expression(&parser, &node))
    CU_ASSERT_TRUE(lscan(&parser.lexer).kind == TK_EOF)
    expression_t *expected = binary((binary_expression_t) {
        .left = integer_constant("1"),
        .right = integer_constant("2"),
        .kind = BINARY_BITWISE,
        .operator.bitwise = BINARY_BITWISE_AND,
        .operator_token = token(TK_AMPERSAND, "&"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_xor_expression(void) {
        char *input = "1 ^ 2";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_exclusive_or_expression(&parser, &node))
    CU_ASSERT_TRUE(lscan(&parser.lexer).kind == TK_EOF)
    expression_t *expected = binary((binary_expression_t) {
        .left = integer_constant("1"),
        .right = integer_constant("2"),
        .kind = BINARY_BITWISE,
        .operator.bitwise = BINARY_BITWISE_XOR,
        .operator_token = token(TK_BITWISE_XOR, "^"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_inclusive_or_expression(void) {
        char *input = "1 | 2";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_inclusive_or_expression(&parser, &node))
    CU_ASSERT_TRUE(lscan(&parser.lexer).kind == TK_EOF)
    expression_t *expected = binary((binary_expression_t) {
        .left = integer_constant("1"),
        .right = integer_constant("2"),
        .kind = BINARY_BITWISE,
        .operator.bitwise = BINARY_BITWISE_OR,
        .operator_token = token(TK_BITWISE_OR, "|"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_logical_and_expression(void) {
        char *input = "1 && 2";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_logical_and_expression(&parser, &node))
    CU_ASSERT_TRUE(lscan(&parser.lexer).kind == TK_EOF)
    expression_t *expected = binary((binary_expression_t) {
        .left = integer_constant("1"),
        .right = integer_constant("2"),
        .kind = BINARY_LOGICAL,
        .operator.logical = BINARY_LOGICAL_AND,
        .operator_token = token(TK_LOGICAL_AND, "&&"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_logical_and_expression_float_operands(void) {
        char *input = "0.0 && 1.0";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_logical_and_expression(&parser, &node))
    CU_ASSERT_TRUE(lscan(&parser.lexer).kind == TK_EOF)
    expression_t *expected = binary((binary_expression_t) {
        .left = float_constant("0.0"),
        .right = float_constant("1.0"),
        .kind = BINARY_LOGICAL,
        .operator.logical = BINARY_LOGICAL_AND,
        .operator_token = token(TK_LOGICAL_AND, "&&"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_logical_or_expression(void) {
        char *input = "1 || 2";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t node;

    CU_ASSERT_TRUE_FATAL(parse_logical_or_expression(&parser, &node))
    expression_t *expected = binary((binary_expression_t) {
        .left = integer_constant("1"),
        .right = integer_constant("2"),
        .kind = BINARY_LOGICAL,
        .operator.logical = BINARY_LOGICAL_OR,
        .operator_token = token(TK_LOGICAL_OR, "||"),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_conditional_expression(void) {
        char *input = "1 ? 2 : 3";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t node;

    CU_ASSERT_TRUE_FATAL(parse_conditional_expression(&parser, &node))
    expression_t expected = (expression_t) {
        .span = dummy_span(),
        .kind = EXPRESSION_TERNARY,
        .value.ternary = (ternary_expression_t) {
            .condition = integer_constant("1"),
            .true_expression = integer_constant("2"),
            .false_expression = integer_constant("3"),
        },
    };
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, &expected))
}

void test_parse_assignment_expression(void) {
        char *input = "val = 2";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t node;

    CU_ASSERT_TRUE_FATAL(parse_assignment_expression(&parser, &node))
    expression_t *expected = binary((binary_expression_t) {
        .left = make_identifier("val"),
        .right = integer_constant("2"),
        .kind = BINARY_ASSIGNMENT,
        .operator.assignment = BINARY_ASSIGN,
        .operator_token = token(TK_ASSIGN, "="),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_compound_assignment_expression_add(void) {
        char *input = "val += 3";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_assignment_expression(&parser, &node))
    expression_t *expected = binary((binary_expression_t) {
        .left = make_identifier("val"),
        .right = integer_constant("3"),
        .kind = BINARY_ASSIGNMENT,
        .operator.assignment = BINARY_ADD_ASSIGN,
        .operator_token = token(TK_PLUS_ASSIGN, "+="),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_compound_assignment_expression_div(void) {
        char *input = "val /= 3";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_assignment_expression(&parser, &node))
    expression_t *expected = binary((binary_expression_t) {
        .left = make_identifier("val"),
        .right = integer_constant("3"),
        .kind = BINARY_ASSIGNMENT,
        .operator.assignment = BINARY_DIVIDE_ASSIGN,
        .operator_token = token(TK_DIVIDE_ASSIGN, "/="),
    });
    CU_ASSERT_TRUE_FATAL(expression_eq(&node, expected))
}

void test_parse_compound_literal_expression(void) {
        char *input = "(struct Foo) { 1, }";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t node;
    CU_ASSERT_TRUE_FATAL(parse_expression(&parser, &node))
    CU_ASSERT_TRUE_FATAL(node.kind == EXPRESSION_COMPOUND_LITERAL)
    CU_ASSERT_TRUE_FATAL(node.value.compound_literal.type->kind == TYPE_STRUCT_OR_UNION)
    CU_ASSERT_TRUE_FATAL(node.value.compound_literal.initializer_list.size == 1);
}

void test_parse_int_declaration_specifiers(void) {
        char *input = "int";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    type_t type;
    CU_ASSERT_TRUE_FATAL(parse_declaration_specifiers(&parser, &type))
    CU_ASSERT_TRUE_FATAL(types_equal(&type, &INT))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
}

void test_parse_invalid_declaration_specifiers(void) {
        char *input = "signed float";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    type_t type;
    CU_ASSERT_TRUE_FATAL(parse_declaration_specifiers(&parser, &type))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 1)
    CU_ASSERT_TRUE_FATAL(types_equal(&type, &INT))
}

void test_parse_struct_definition(void) {
        char *input = "struct Foo { int a; float b[10]; }";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    type_t type;
    CU_ASSERT_TRUE_FATAL(parse_declaration_specifiers(&parser, &type))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)

    CU_ASSERT_EQUAL_FATAL(type.kind, TYPE_STRUCT_OR_UNION)
    CU_ASSERT_EQUAL_FATAL(type.value.struct_or_union.is_union, false)
    CU_ASSERT_STRING_EQUAL_FATAL(type.value.struct_or_union.identifier->value, "Foo")
    CU_ASSERT_EQUAL_FATAL(type.value.struct_or_union.fields.size, 2)

    struct_field_t *field = type.value.struct_or_union.fields.buffer[0];
    CU_ASSERT_STRING_EQUAL_FATAL(field->identifier->value, "a")
    CU_ASSERT_TRUE_FATAL(types_equal(field->type, &INT))
    CU_ASSERT_TRUE_FATAL(field->bitfield_width == NULL)

    field = type.value.struct_or_union.fields.buffer[1];
    CU_ASSERT_STRING_EQUAL_FATAL(field->identifier->value, "b")
    CU_ASSERT_TRUE_FATAL(types_equal(field->type, array_of(&FLOAT, integer_constant("10"))))
    CU_ASSERT_TRUE_FATAL(field->bitfield_width == NULL)
}

void test_parse_union_definition(void) {
        char *input = "union Foo { int i; float f; }";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);

    type_t type;
    CU_ASSERT_TRUE_FATAL(parse_declaration_specifiers(&parser, &type))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)

    CU_ASSERT_EQUAL_FATAL(type.kind, TYPE_STRUCT_OR_UNION)
    CU_ASSERT_EQUAL_FATAL(type.value.struct_or_union.is_union, true)
    CU_ASSERT_STRING_EQUAL_FATAL(type.value.struct_or_union.identifier->value, "Foo")
    CU_ASSERT_EQUAL_FATAL(type.value.struct_or_union.fields.size, 2)
}

void test_parse_struct_definition_with_bitfields(void) {
        // both named and anonymous bitfields
    char *input = "struct Foo { int a : 1; int : 7; };";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);

    type_t type;
    CU_ASSERT_TRUE_FATAL(parse_declaration_specifiers(&parser, &type))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)

    CU_ASSERT_EQUAL_FATAL(type.kind, TYPE_STRUCT_OR_UNION)
    CU_ASSERT_EQUAL_FATAL(type.value.struct_or_union.is_union, false)
    CU_ASSERT_STRING_EQUAL_FATAL(type.value.struct_or_union.identifier->value, "Foo")
    CU_ASSERT_EQUAL_FATAL(type.value.struct_or_union.fields.size, 2)

    struct_field_t *field = type.value.struct_or_union.fields.buffer[0];
    CU_ASSERT_STRING_EQUAL_FATAL(field->identifier->value, "a")
    CU_ASSERT_TRUE_FATAL(types_equal(field->type, &INT))
    CU_ASSERT_TRUE_FATAL(field->bitfield_width != NULL)
    CU_ASSERT_TRUE_FATAL(expression_eq(field->bitfield_width, integer_constant("1")))

    field = type.value.struct_or_union.fields.buffer[1];
    CU_ASSERT_PTR_NULL_FATAL(field->identifier)
    CU_ASSERT_TRUE_FATAL(types_equal(field->type, &INT))
    CU_ASSERT_TRUE_FATAL(field->bitfield_width != NULL)
    CU_ASSERT_TRUE_FATAL(expression_eq(field->bitfield_width, integer_constant("7")))
}

void test_parse_initializer_expression_simple(void) {
        char *input = "14;";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    initializer_t initializer;
    CU_ASSERT_TRUE_FATAL(parse_initializer(&parser, &initializer))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
    CU_ASSERT_TRUE_FATAL(initializer.kind == INITIALIZER_EXPRESSION)
    CU_ASSERT_TRUE_FATAL(expression_eq(initializer.value.expression, integer_constant("14")))
}

void test_parse_initializer_list_array(void) {
        char *input = "{0, 1, 2}";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    initializer_t initializer;
    CU_ASSERT_TRUE_FATAL(parse_initializer(&parser, &initializer))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
    CU_ASSERT_TRUE_FATAL(initializer.kind == INITIALIZER_LIST)
    CU_ASSERT_EQUAL_FATAL(initializer.value.list->size, 3)
    for (int i = 0; i < 3; i += 1) {
        initializer_list_element_t element = initializer.value.list->buffer[i];
        CU_ASSERT_PTR_NULL_FATAL(element.designation)
        CU_ASSERT_TRUE_FATAL(element.initializer->kind == INITIALIZER_EXPRESSION)
        char buffer[8];
        sprintf(buffer, "%d", i);
        CU_ASSERT_TRUE_FATAL(expression_eq(element.initializer->value.expression, integer_constant(buffer)))
    }
}

void test_parse_initializer_list_array_trailing_comma(void) {
        char *input = "{0, 1, 2,}";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    initializer_t initializer;
    CU_ASSERT_TRUE_FATAL(parse_initializer(&parser, &initializer))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
    CU_ASSERT_TRUE_FATAL(initializer.kind == INITIALIZER_LIST)
    CU_ASSERT_EQUAL_FATAL(initializer.value.list->size, 3)
    for (int i = 0; i < 3; i += 1) {
        initializer_list_element_t element = initializer.value.list->buffer[i];
        CU_ASSERT_PTR_NULL_FATAL(element.designation)
        CU_ASSERT_TRUE_FATAL(element.initializer->kind == INITIALIZER_EXPRESSION)
        char buffer[8];
        sprintf(buffer, "%d", i);
        CU_ASSERT_TRUE_FATAL(expression_eq(element.initializer->value.expression, integer_constant(buffer)))
    }
}

void test_parse_initializer_list_array_index_designator(void) {
        char *input = "{[0] = 0}";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    initializer_t initializer;
    CU_ASSERT_TRUE_FATAL(parse_initializer(&parser, &initializer))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
    CU_ASSERT_TRUE_FATAL(initializer.kind == INITIALIZER_LIST)
    CU_ASSERT_EQUAL_FATAL(initializer.value.list->size, 1)
    initializer_list_element_t element = initializer.value.list->buffer[0];
    CU_ASSERT_PTR_NOT_NULL_FATAL(element.designation)
    CU_ASSERT_EQUAL_FATAL(element.designation->size, 1)
    designator_t designator = element.designation->buffer[0];
    CU_ASSERT_TRUE_FATAL(designator.kind == DESIGNATOR_INDEX)
    CU_ASSERT_TRUE_FATAL(expression_eq(designator.value.index, integer_constant("0")))
    CU_ASSERT_TRUE_FATAL(element.initializer->kind == INITIALIZER_EXPRESSION)
    CU_ASSERT_TRUE_FATAL(expression_eq(element.initializer->value.expression, integer_constant("0")))
}

void test_parse_initializer_list_struct(void) {
        char *input = "{.a = 0, .b = { .c = 1 }}";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    initializer_t initializer;
    CU_ASSERT_TRUE_FATAL(parse_initializer(&parser, &initializer))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
    CU_ASSERT_TRUE_FATAL(initializer.kind == INITIALIZER_LIST)
    CU_ASSERT_EQUAL_FATAL(initializer.value.list->size, 2)

    initializer_list_element_t element_a = initializer.value.list->buffer[0];
    CU_ASSERT_PTR_NOT_NULL_FATAL(element_a.designation)
    CU_ASSERT_EQUAL_FATAL(element_a.designation->size, 1)
    designator_t designator_a = element_a.designation->buffer[0];
    CU_ASSERT_TRUE_FATAL(designator_a.kind == DESIGNATOR_FIELD)
    CU_ASSERT_STRING_EQUAL_FATAL(designator_a.value.field->value, "a")
    CU_ASSERT_TRUE_FATAL(element_a.initializer->kind == INITIALIZER_EXPRESSION)
    CU_ASSERT_TRUE_FATAL(expression_eq(element_a.initializer->value.expression, integer_constant("0")))

    initializer_list_element_t element_b = initializer.value.list->buffer[1];
    CU_ASSERT_PTR_NOT_NULL_FATAL(element_b.designation)
    CU_ASSERT_EQUAL_FATAL(element_b.designation->size, 1)
    designator_t designator_b = element_b.designation->buffer[0];
    CU_ASSERT_TRUE_FATAL(designator_b.kind == DESIGNATOR_FIELD)
    CU_ASSERT_STRING_EQUAL_FATAL(designator_b.value.field->value, "b")
    CU_ASSERT_TRUE_FATAL(element_b.initializer->kind == INITIALIZER_LIST)
    CU_ASSERT_EQUAL_FATAL(element_b.initializer->value.list->size, 1)

    initializer_list_element_t element_c = element_b.initializer->value.list->buffer[0];
    CU_ASSERT_PTR_NOT_NULL_FATAL(element_c.designation)
    CU_ASSERT_EQUAL_FATAL(element_c.designation->size, 1)
    designator_t designator_c = element_c.designation->buffer[0];
    CU_ASSERT_TRUE_FATAL(designator_c.kind == DESIGNATOR_FIELD)
    CU_ASSERT_STRING_EQUAL_FATAL(designator_c.value.field->value, "c")
}

void test_parse_empty_declaration(void) {
        char *input = "int;";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
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

void test_parse_simple_declaration(void) {
        char *input = "int a;";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
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

void test_parse_simple_declaration_with_initializer(void) {
        char *input = "int a = 1 & 1;";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
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
            .value.expression = binary((binary_expression_t) {
                .kind = BINARY_BITWISE,
                .operator.bitwise = BINARY_BITWISE_AND,
                .left = integer_constant("1"),
                .right = integer_constant("1"),
                .operator_token = token(TK_AMPERSAND, "&"),
            }),
        },
    };
    CU_ASSERT_TRUE_FATAL(declaration_eq(declarations.buffer[0], &expected))
}

void test_parse_declaration_boolean(void) {
        char *input = "_Bool a = 1;";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
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
            .value.expression = integer_constant("1"),
        },
    };
    CU_ASSERT_TRUE_FATAL(declaration_eq(declarations.buffer[0], &expected))
}

void test_parse_pointer_declaration(void) {
        char *input = "void *a;";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
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

void test_parse_compound_declaration(void) {
        char *input = "int a, b = 0, c = d + 1;";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
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
            .value.expression = integer_constant("0"),
        },
    };
    CU_ASSERT_TRUE_FATAL(declaration_eq(declarations.buffer[1], &expected_b))

    declaration_t expected_c = (declaration_t) {
        .type = &INT,
        .identifier = token(TK_IDENTIFIER, "c"),
        .initializer = & (initializer_t) {
            .kind = INITIALIZER_EXPRESSION,
            .value.expression = binary((binary_expression_t) {
                .kind = BINARY_ARITHMETIC,
                .operator.arithmetic = BINARY_ARITHMETIC_ADD,
                .left = make_identifier("d"),
                .right = integer_constant("1"),
                .operator_token = token(TK_PLUS, "+"),
            }),
        },
    };
    CU_ASSERT_TRUE_FATAL(declaration_eq(declarations.buffer[2], &expected_c))
}

void test_parse_function_declaration_no_parameters(void) {
        char *input = "int foo();";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
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
        .value.function = {
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

void test_parse_function_declaration_with_parameters(void) {
        // combination of abstract declarator and direct declarator parameters
    char *input = "int foo(int a, float (*)(void), ...);";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
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
                    .value.function = {
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
            .value.function = {
                .return_type = &INT,
                .parameter_list = &parameter_type_list,
            },
        },
        .identifier = token(TK_IDENTIFIER, "foo"),
        .initializer = NULL,
    };

    CU_ASSERT_TRUE_FATAL(declaration_eq(declarations.buffer[0], &expected))
}

void test_parse_function_declaration_returning_pointer(void) {
        char *input = "int *foo();";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
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
        .value.function = {
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

void test_parse_array_declaration(void) {
        char *input = "int foo[10];";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    ptr_vector_t declarations = { .size = 0, .capacity = 0, .buffer = NULL, };
    CU_ASSERT_TRUE_FATAL(parse_declaration(&parser, &declarations))
    CU_ASSERT_EQUAL_FATAL(declarations.size, 1)
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)

    declaration_t *declaration = declarations.buffer[0];
    type_t expected_type = {
        .kind = TYPE_ARRAY,
        .value.array = {
            .element_type = &INT,
            .size = integer_constant("10"),
        },
    };
    CU_ASSERT_STRING_EQUAL_FATAL(declaration->identifier->value, "foo")
    CU_ASSERT_TRUE_FATAL(types_equal(declaration->type, &expected_type))
}

void test_parse_array_declaration_with_initializer(void) {
        char *input = "int arr[3] = { 1, 2, 3 };";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    ptr_vector_t declarations = { .size = 0, .capacity = 0, .buffer = NULL, };
    CU_ASSERT_TRUE_FATAL(parse_declaration(&parser, &declarations))
    CU_ASSERT_EQUAL_FATAL(declarations.size, 1)
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)

    declaration_t *declaration = declarations.buffer[0];
    type_t expected_type = {
        .kind = TYPE_ARRAY,
        .value.array = {
            .element_type = &INT,
            .size = integer_constant("3"),
        },
    };
    CU_ASSERT_STRING_EQUAL_FATAL(declaration->identifier->value, "arr")
    CU_ASSERT_TRUE_FATAL(types_equal(declaration->type, &expected_type))
}

void test_parse_2d_array_declaration(void) {
        char *input = "int bar[1][2];";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    ptr_vector_t declarations = { .size = 0, .capacity = 0, .buffer = NULL, };
    CU_ASSERT_TRUE_FATAL(parse_declaration(&parser, &declarations))
    CU_ASSERT_EQUAL_FATAL(declarations.size, 1)
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)

    declaration_t *declaration = declarations.buffer[0];
    type_t inner_type = {
        .kind = TYPE_ARRAY,
        .value.array = {
            .element_type = &INT,
            .size = integer_constant("2"),
        },
    };
    type_t expected_type = {
        .kind = TYPE_ARRAY,
        .value.array = {
            .element_type = &inner_type,
            .size = integer_constant("1"),
        },
    };
    CU_ASSERT_STRING_EQUAL_FATAL(declaration->identifier->value, "bar")
    CU_ASSERT_TRUE_FATAL(types_equal(declaration->type, &expected_type))
}

void test_parse_array_of_functions_declaration(void) {
        char* input = "int foo[](void);";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
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
        .value.function = {
            .return_type = &INT,
            .parameter_list = &parameter_list,
        },
    };

    type_t type = {
        .kind = TYPE_ARRAY,
        .value.array = {
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

void test_parse_function_pointer(void) {
        char* input = "int (*foo)(void);";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
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
        .value.pointer = {
            .is_const = false,
            .is_volatile = false,
            .is_restrict = false,
            .base = &(type_t) {
                .storage_class = STORAGE_CLASS_AUTO,
                .is_volatile = false,
                .is_const = false,
                .kind = TYPE_FUNCTION,
                .value.function = {
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

void test_parse_complex_declaration(void) {
        char* input = "float *(*(*bar[1][2])(void))(int);";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
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
        .value.array = {
            .size = integer_constant("1"),
            .element_type = &(type_t) {
                .storage_class = STORAGE_CLASS_AUTO,
                .is_const = false,
                .is_volatile = false,
                .kind = TYPE_ARRAY,
                .value.array = {
                    .size = integer_constant("2"),
                    .element_type = ptr_to(&(type_t) {
                        .storage_class = STORAGE_CLASS_AUTO,
                        .is_const = false,
                        .is_volatile = false,
                        .kind = TYPE_FUNCTION,
                        .value.function = {
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
                                .value.function = {
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

void test_parse_empty_global_declaration(void) {
        char *input = "int;";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    translation_unit_t translation_unit;
    CU_ASSERT_TRUE_FATAL(parse(&parser, &translation_unit));
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
    CU_ASSERT_EQUAL_FATAL(lscan(&parser.lexer).kind, TK_EOF) // should have consumed the entire input
}

void test_parse_struct_type_declaration(void) {
        char *input = "struct Foo { int a; };";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    translation_unit_t translation_unit;
    CU_ASSERT_TRUE_FATAL(parse(&parser, &translation_unit));
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
    CU_ASSERT_EQUAL_FATAL(lscan(&parser.lexer).kind, TK_EOF) // should have consumed the entire input
}

void test_parse_typedef_struct_type(void) {
        char *input = "typedef struct Foo { int a; } foo;\n"
                  "foo value;\n";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    translation_unit_t translation_unit;
    CU_ASSERT_TRUE_FATAL(parse(&parser, &translation_unit))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
    CU_ASSERT_EQUAL_FATAL(lscan(&parser.lexer).kind, TK_EOF) // should have consumed the entire input

    CU_ASSERT_EQUAL_FATAL(translation_unit.length, 2)
    CU_ASSERT_EQUAL_FATAL(translation_unit.external_declarations[0]->kind, EXTERNAL_DECLARATION_DECLARATION)
    CU_ASSERT_EQUAL_FATAL(translation_unit.external_declarations[0]->value.declaration.length, 1)
    CU_ASSERT_EQUAL_FATAL(translation_unit.external_declarations[1]->kind, EXTERNAL_DECLARATION_DECLARATION)
    CU_ASSERT_EQUAL_FATAL(translation_unit.external_declarations[1]->value.declaration.length, 1)

    // The first external declaration declares the typedef name
    const declaration_t *decl = translation_unit.external_declarations[0]->value.declaration.declarations[0];
    CU_ASSERT_STRING_EQUAL_FATAL(decl->identifier->value, "foo")
    CU_ASSERT_EQUAL_FATAL(decl->type->kind, TYPE_STRUCT_OR_UNION)
    CU_ASSERT_EQUAL_FATAL(decl->type->storage_class, STORAGE_CLASS_TYPEDEF)
    CU_ASSERT_STRING_EQUAL_FATAL(decl->type->value.struct_or_union.identifier->value, "Foo")

    // The second external declaration declares a global variable "value" using the type referred to by the typedef
    // name "foo".
    decl = translation_unit.external_declarations[1]->value.declaration.declarations[0];
    CU_ASSERT_STRING_EQUAL_FATAL(decl->identifier->value, "value")
    CU_ASSERT_EQUAL_FATAL(decl->type->storage_class, STORAGE_CLASS_AUTO)
    CU_ASSERT_EQUAL_FATAL(decl->type->kind, TYPE_STRUCT_OR_UNION)
}

void test_parse_typedef_anonymous_struct_type(void) {
        char *input =
        "typedef struct {\n"
        "    int a;\n"
        "    int b;\n"
        "} my_struct;\n";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    translation_unit_t translation_unit;
    CU_ASSERT_TRUE_FATAL(parse(&parser, &translation_unit))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
    CU_ASSERT_EQUAL_FATAL(lscan(&parser.lexer).kind, TK_EOF) // should have consumed the entire input
}

void test_abstract_declarator_pointer_int(void) {
        char *input = "*"; // int token has already been parsed
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    type_t *result;
    CU_ASSERT_TRUE_FATAL(parse_abstract_declarator(&parser, INT, &result))
    CU_ASSERT_TRUE_FATAL(result->kind == TYPE_POINTER);
    CU_ASSERT_TRUE_FATAL(result->value.pointer.base->kind == TYPE_INTEGER);
}

void test_abstract_declarator_function_pointer(void) {
        char *input = "(*)(void)"; // int token has already been parsed
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    type_t *result;
    CU_ASSERT_TRUE_FATAL(parse_abstract_declarator(&parser, INT, &result))
    CU_ASSERT_TRUE_FATAL(result->kind == TYPE_POINTER)
    CU_ASSERT_TRUE_FATAL(result->value.pointer.base->kind == TYPE_FUNCTION)
}

void test_parse_function_prototype_void(void) {
        char *input = "float foo(void);";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    ptr_vector_t declarations = { .size = 0, .capacity = 0, .buffer = NULL, };
    CU_ASSERT_TRUE_FATAL(parse_declaration(&parser, &declarations))
    CU_ASSERT_EQUAL_FATAL(declarations.size, 1)
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)

    type_t type = {
        .kind = TYPE_FUNCTION,
        .value.function = {
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

void test_parse_function_prototype(void) {
        char *input = "double pow(float a, short b);";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
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
            .value.function = {
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

void test_parse_empty_statement(void) {
        char *input = ";";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    statement_t node;

    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &node))
    statement_t *expected = malloc(sizeof(statement_t));
    *expected = (statement_t) {
            .kind = STATEMENT_EMPTY,
            .terminator = token(TK_SEMICOLON, ";"),
    };
    CU_ASSERT_TRUE_FATAL(statement_eq(&node, expected))
}

void test_parse_expression_statement(void) {
        char *input = "1;";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    statement_t node;

    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &node))
    statement_t *expected = malloc(sizeof(statement_t));
    *expected = (statement_t) {
            .kind = STATEMENT_EXPRESSION,
            .value.expression = integer_constant("1"),
            .terminator = token(TK_SEMICOLON, ";"),
    };
    CU_ASSERT_TRUE_FATAL(statement_eq(&node, expected))
}

void test_parse_compound_statement(void) {
        char *input = "{ 1; 'a'; 1.0; }";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    statement_t node;

    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &node))

    statement_t *expected = malloc(sizeof(statement_t));
    statement_t *statements[3] = {
        expression_statement(integer_constant("1")),
        expression_statement(primary((primary_expression_t) {
            .kind = PE_CONSTANT,
            .value.token = (token_t) {
                .kind = TK_CHAR_LITERAL,
                .value = "'a'",
                .position = dummy_position(),
            },
        })),
        expression_statement(primary((primary_expression_t) {
            .kind = PE_CONSTANT,
            .value.token = (token_t) {
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
        .kind = STATEMENT_COMPOUND,
        .value.compound = {
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

void test_parse_compound_statement_with_error(void) {
    // The parser should recover, and continue parsing the rest of the statements.
        char *input = "{ a-; 1; }";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    statement_t node;

    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &node))
    statement_t *expected = malloc(sizeof(statement_t));
    block_item_t *block_items[1] = {
            block_item_s(expression_statement(integer_constant("1"))),
    };
    *expected = (statement_t) {
        .kind = STATEMENT_COMPOUND,
        .value.compound = {
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

void test_parse_if_statement(void) {
        char *input = "if (1) 2;";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    statement_t node;

    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &node))
    statement_t *expected = malloc(sizeof(statement_t));
    *expected = (statement_t) {
        .kind = STATEMENT_IF,
        .value.if_ = {
            .keyword = token(TK_IF, "if"),
            .condition = integer_constant("1"),
            .true_branch = expression_statement(integer_constant("2")),
            .false_branch = NULL,
        },
    };
    CU_ASSERT_TRUE_FATAL(statement_eq(&node, expected))
}

void test_parse_if_else_statement(void) {
        char *input = "if (1) 2; else 3;";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    statement_t node;

    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &node))
    statement_t *expected = malloc(sizeof(statement_t));
    *expected = (statement_t) {
        .kind = STATEMENT_IF,
        .value.if_ = {
            .keyword = token(TK_IF, "if"),
            .condition = integer_constant("1"),
            .true_branch = expression_statement(integer_constant("2")),
            .false_branch = expression_statement(integer_constant("3")),
        },
    };
    CU_ASSERT_TRUE_FATAL(statement_eq(&node, expected))
}

void test_parse_if_else_if_else_statement(void) {
        char *input = "if (1) 2; else if (3) 4; else 5;";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    statement_t node;

    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &node))
    statement_t *expected = malloc(sizeof(statement_t));
    *expected = (statement_t) {
        .kind = STATEMENT_IF,
        .value.if_ = {
            .keyword = token(TK_IF, "if"),
            .condition = integer_constant("1"),
            .true_branch = expression_statement(integer_constant("2")),
            .false_branch = if_statement(integer_constant("3"), expression_statement(integer_constant("4")), expression_statement(integer_constant("5"))),
        },
    };
    CU_ASSERT_TRUE_FATAL(statement_eq(&node, expected))
}

void test_parse_switch_statement(void) {
        char *input = "switch(foo);"; // switch + empty statement
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    statement_t node;

    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &node))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
    CU_ASSERT_EQUAL_FATAL(node.kind, STATEMENT_SWITCH)
    CU_ASSERT_PTR_NOT_NULL_FATAL(node.value.switch_.expression)
    CU_ASSERT_EQUAL_FATAL(node.value.switch_.expression->kind, EXPRESSION_PRIMARY)
    CU_ASSERT_PTR_NOT_NULL_FATAL(node.value.switch_.statement)
    CU_ASSERT_EQUAL_FATAL(node.value.switch_.statement->kind, STATEMENT_EMPTY)
}

void test_parse_return_statement(void) {
        char *input = "return 1;";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    statement_t node;

    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &node))
    statement_t *expected = malloc(sizeof(statement_t));
    *expected = (statement_t) {
        .kind = STATEMENT_RETURN,
        .value.return_ = {
            .keyword = token(TK_RETURN, "return"),
            .expression = integer_constant("1"),
        },
        .terminator = token(TK_SEMICOLON, ";"),
    };
    CU_ASSERT_TRUE_FATAL(statement_eq(&node, expected))
}

void test_parse_while_statement(void) {
        char *input = "while (cond > 0) { cond = cond - 1; }";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    statement_t node;

    // Assert that it was parsed successfully, and that the parser consumed all of the input.
    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &node))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
    CU_ASSERT_TRUE_FATAL(lscan(&parser.lexer).kind == TK_EOF)

    // Make sure the statement is parsed correctly
    // We have other tests to validate the condition and body, so just make sure
    // they're present and have the expected types.
    CU_ASSERT_EQUAL_FATAL(node.kind, STATEMENT_WHILE)
    CU_ASSERT_EQUAL_FATAL(node.value.while_.condition->kind, EXPRESSION_BINARY)
    CU_ASSERT_EQUAL_FATAL(node.value.while_.body->kind, STATEMENT_COMPOUND)
}

void test_parse_while_statement_with_empty_body(void) {
        char *input = "while (1);";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    statement_t node;

    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &node))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
    CU_ASSERT_TRUE_FATAL(lscan(&parser.lexer).kind == TK_EOF)

    CU_ASSERT_EQUAL_FATAL(node.kind, STATEMENT_WHILE)
    CU_ASSERT_EQUAL_FATAL(node.value.while_.condition->kind, EXPRESSION_PRIMARY)
    CU_ASSERT_EQUAL_FATAL(node.value.while_.body->kind, STATEMENT_EMPTY)
}

void test_parse_do_while_statement(void) {
        char *input = "do { cond = cond - 1; } while (cond > 0);";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    statement_t node;

    // Assert that it was parsed successfully, and that the parser consumed all of the input.
    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &node))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
    CU_ASSERT_TRUE_FATAL(lscan(&parser.lexer).kind == TK_EOF)

    // Make sure the statement is parsed correctly
    // We have other tests to validate the condition and body, so just make sure
    // they're present and have the expected types.
    CU_ASSERT_EQUAL_FATAL(node.kind, STATEMENT_DO_WHILE)
    CU_ASSERT_EQUAL_FATAL(node.value.do_while.condition->kind, EXPRESSION_BINARY)
    CU_ASSERT_EQUAL_FATAL(node.value.do_while.body->kind, STATEMENT_COMPOUND)
}

void test_parse_for_statement(void) {
        const char *input =
        "for (int i = 0; i < 10; i = i + 1) {\n"
        "    a = a + i;\n"
        "}";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    statement_t node;

    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &node))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
    CU_ASSERT_TRUE_FATAL(lscan(&parser.lexer).kind == TK_EOF)

    CU_ASSERT_EQUAL_FATAL(node.kind, STATEMENT_FOR)
    CU_ASSERT_EQUAL_FATAL(node.value.for_.initializer.kind, FOR_INIT_DECLARATION)
    CU_ASSERT_PTR_NOT_NULL_FATAL(node.value.for_.initializer.declarations)

    CU_ASSERT_PTR_NOT_NULL_FATAL(node.value.for_.condition)
    CU_ASSERT_EQUAL_FATAL(node.value.for_.condition->kind, EXPRESSION_BINARY)

    CU_ASSERT_PTR_NOT_NULL_FATAL(node.value.for_.post)
    CU_ASSERT_EQUAL_FATAL(node.value.for_.post->kind, EXPRESSION_BINARY)

    CU_ASSERT_EQUAL_FATAL(node.value.for_.body->kind, STATEMENT_COMPOUND)
}

void test_parse_for_statement_no_optional_parts(void) {
        const char *input = "for (;;);";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    statement_t node;

    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &node))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
    CU_ASSERT_TRUE_FATAL(lscan(&parser.lexer).kind == TK_EOF)

    CU_ASSERT_EQUAL_FATAL(node.kind, STATEMENT_FOR)
    CU_ASSERT_EQUAL_FATAL(node.value.for_.initializer.kind, FOR_INIT_EMPTY)
    CU_ASSERT_PTR_NULL_FATAL(node.value.for_.condition)
    CU_ASSERT_PTR_NULL_FATAL(node.value.for_.post)
    CU_ASSERT_EQUAL_FATAL(node.value.for_.body->kind, STATEMENT_EMPTY)
}

void test_parse_for_statement_expr_initializer(void) {
        const char *input = "for (i = 0;;);";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    statement_t node;

    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &node))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
    CU_ASSERT_TRUE_FATAL(lscan(&parser.lexer).kind == TK_EOF)

    CU_ASSERT_EQUAL_FATAL(node.kind, STATEMENT_FOR)
    CU_ASSERT_EQUAL_FATAL(node.value.for_.initializer.kind, FOR_INIT_EXPRESSION)
    CU_ASSERT_PTR_NOT_NULL_FATAL(node.value.for_.initializer.expression)
}

void parse_external_declaration_declaration(void) {
        char *input = "int a = 4;";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    external_declaration_t node;
    CU_ASSERT_TRUE_FATAL(parse_external_declaration(&parser, &node))
    declaration_t expected = (declaration_t ) {
        .type = &INT,
        .identifier = token(TK_IDENTIFIER, "a"),
        .initializer = & (initializer_t) {
            .kind = INITIALIZER_EXPRESSION,
            .value.expression = integer_constant("4"),
        },
    };

    CU_ASSERT_TRUE_FATAL(node.kind == EXTERNAL_DECLARATION_DECLARATION)
    CU_ASSERT_EQUAL_FATAL(node.value.declaration.length, 1)
    CU_ASSERT_TRUE_FATAL(declaration_eq(node.value.declaration.declarations[0], &expected))
}

void parse_external_definition_prototype_var_args(void) {
        char *input = "int printf(const char *format, ...);";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    external_declaration_t node;
    CU_ASSERT_TRUE_FATAL(parse_external_declaration(&parser, &node))
    CU_ASSERT_TRUE_FATAL(node.kind == EXTERNAL_DECLARATION_DECLARATION)

    declaration_t *declaration = node.value.declaration.declarations[0];

    type_t *expected_type = &(type_t) {
        .kind = TYPE_FUNCTION,
        .is_const = false,
        .is_volatile = false,
        .value.function = {
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

void parse_external_declaration_function_definition(void) {
        char *input = "float square(float val) { return val * val; }";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);

    external_declaration_t node;
    CU_ASSERT_TRUE_FATAL(parse_external_declaration(&parser, &node))
    CU_ASSERT_TRUE_FATAL(node.kind == EXTERNAL_DECLARATION_FUNCTION_DEFINITION)

    CU_ASSERT_TRUE_FATAL(types_equal(node.value.function_definition->return_type, &FLOAT))
    CU_ASSERT_TRUE_FATAL(strcmp(node.value.function_definition->identifier->value, "square") == 0)

    // validate the argument list
    CU_ASSERT_EQUAL_FATAL(node.value.function_definition->parameter_list->length, 1)
    CU_ASSERT_TRUE_FATAL(types_equal(node.value.function_definition->parameter_list->parameters[0]->type, &FLOAT))
    CU_ASSERT_TRUE_FATAL(strcmp(node.value.function_definition->parameter_list->parameters[0]->identifier->value, "val") == 0)

    // validate the body is parsed correctly
    statement_t *ret = {
        return_statement(binary((binary_expression_t) {
            .kind = BINARY_ARITHMETIC,
            .operator.arithmetic = BINARY_ARITHMETIC_MULTIPLY,
            .left = make_identifier("val"),
            .right = make_identifier("val"),
            .operator_token = token(TK_STAR, "*"),
        })),
    };
    block_item_t *block_item = block_item_s(ret);
    statement_t body = {
        .kind = STATEMENT_COMPOUND,
        .terminator = token(TK_RBRACE, "}"),
        .value.compound = {
            .block_items = {
                .size = 1,
                .capacity = 1,
                .buffer = (void**) &block_item,
            },
        },
    };
    CU_ASSERT_TRUE_FATAL(statement_eq(node.value.function_definition->body, &body))
}

void parse_external_definition_function_taking_void(void) {
        char *input = "int main(void) { return 0; }";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);

    external_declaration_t node;
    CU_ASSERT_TRUE_FATAL(parse_external_declaration(&parser, &node))
    CU_ASSERT_TRUE_FATAL(node.kind == EXTERNAL_DECLARATION_FUNCTION_DEFINITION)

    CU_ASSERT_TRUE_FATAL(types_equal(node.value.function_definition->return_type, &INT))
    CU_ASSERT_TRUE_FATAL(strcmp(node.value.function_definition->identifier->value, "main") == 0)

    // validate the argument list
    CU_ASSERT_EQUAL_FATAL(node.value.function_definition->parameter_list->length, 0)

    // validate the body is parsed correctly
    statement_t *ret = {
        return_statement(integer_constant("0")),
    };
    block_item_t *block_item = block_item_s(ret);
    statement_t body = {
        .kind = STATEMENT_COMPOUND,
        .terminator = token(TK_RBRACE, "}"),
        .value.compound = {
            .block_items = {
                .size = 1,
                .capacity = 1,
                .buffer = (void**) &block_item,
            },
        },
    };
    CU_ASSERT_TRUE_FATAL(statement_eq(node.value.function_definition->body, &body))
}

void test_parse_external_declaration_typedef(void) {
        const char* input = "typedef int integer;";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    external_declaration_t external_declaration;
    parse_external_declaration(&parser, &external_declaration);
    CU_ASSERT_TRUE_FATAL(external_declaration.value.declaration.length == 1)
    CU_ASSERT_TRUE_FATAL(external_declaration.value.declaration.declarations[0]->type->storage_class == STORAGE_CLASS_TYPEDEF);
    CU_ASSERT_TRUE_FATAL(external_declaration.value.declaration.declarations[0]->type->kind == TYPE_INTEGER);
    CU_ASSERT_STRING_EQUAL_FATAL(external_declaration.value.declaration.declarations[0]->identifier->value, "integer")
}

void test_parse_external_declaration_typedef_ptr(void) {
        const char *input = "typedef int* ptr;";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    external_declaration_t external_declaration;
    CU_ASSERT_TRUE_FATAL(parse_external_declaration(&parser, &external_declaration))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
    CU_ASSERT_TRUE_FATAL(external_declaration.value.declaration.length == 1)
    const declaration_t *decl = external_declaration.value.declaration.declarations[0];
    CU_ASSERT_EQUAL_FATAL(decl->type->storage_class, STORAGE_CLASS_TYPEDEF)
    CU_ASSERT_EQUAL_FATAL(decl->type->kind, TYPE_POINTER)
    CU_ASSERT_EQUAL_FATAL(decl->type->value.pointer.base->storage_class, STORAGE_CLASS_AUTO)
}

void test_parse_external_declaration_typedef_const_ptr(void) {
        const char *input = "typedef int * const ptr;";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    external_declaration_t external_declaration;
    CU_ASSERT_TRUE_FATAL(parse_external_declaration(&parser, &external_declaration))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
    CU_ASSERT_TRUE_FATAL(external_declaration.value.declaration.length == 1)
    const declaration_t *decl = external_declaration.value.declaration.declarations[0];
    CU_ASSERT_EQUAL_FATAL(decl->type->storage_class, STORAGE_CLASS_TYPEDEF)
    CU_ASSERT_EQUAL_FATAL(decl->type->kind, TYPE_POINTER)
    CU_ASSERT_TRUE_FATAL(decl->type->value.pointer.is_const)
    CU_ASSERT_EQUAL_FATAL(decl->type->value.pointer.base->storage_class, STORAGE_CLASS_AUTO)
}

void test_parse_external_declaration_that_uses_typedef(void) {
        const char* input = "typedef float type;\ntype foo;\n";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    translation_unit_t program;
    CU_ASSERT_TRUE_FATAL(parse(&parser, &program))

    CU_ASSERT_TRUE_FATAL(program.length == 2)
    const external_declaration_t *external_declaration = program.external_declarations[1];
    CU_ASSERT_TRUE_FATAL(external_declaration->value.declaration.length == 1)
    const declaration_t *decl = external_declaration->value.declaration.declarations[0];
    CU_ASSERT_STRING_EQUAL_FATAL(decl->identifier->value, "foo")
    CU_ASSERT_TRUE_FATAL(decl->type->kind == TYPE_FLOATING)
}

void test_parse_external_declaration_typedef_volatile_int(void) {
        const char *input = "typedef volatile int newtype;\n";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    translation_unit_t program;
    CU_ASSERT_TRUE_FATAL(parse(&parser, &program))
}

void test_parse_break_statement(void) {
        const char *input = "break;";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);

    statement_t statement;
    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &statement))
    CU_ASSERT_TRUE_FATAL(statement.kind == STATEMENT_BREAK)
}

void test_parse_continue_statement(void) {
        const char *input = "continue;";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);

    statement_t statement;
    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &statement))
    CU_ASSERT_TRUE_FATAL(statement.kind == STATEMENT_CONTINUE)
}

void test_parse_goto_statement(void) {
        const char *input = "goto foo;";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);

    statement_t statement;
    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &statement))
    CU_ASSERT_TRUE_FATAL(statement.kind == STATEMENT_GOTO)
    CU_ASSERT_TRUE_FATAL(statement.value.goto_.identifier != NULL)
    CU_ASSERT_STRING_EQUAL_FATAL(statement.value.goto_.identifier->value, "foo")
}

void test_parse_labeled_statement(void) {
        const char *input = "yoshi: ;"; // label + empty statement
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);

    statement_t statement;
    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &statement))
    CU_ASSERT_TRUE_FATAL(statement.kind == STATEMENT_LABEL)
    CU_ASSERT_STRING_EQUAL_FATAL(statement.value.label_.identifier->value, "yoshi")
    CU_ASSERT_TRUE_FATAL(statement.value.label_.statement->kind == STATEMENT_EMPTY)
}

void test_parse_default_statement(void) {
        const char *input = "default: ;"; // default + empty statement
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);

    statement_t statement;
    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &statement))
    CU_ASSERT_TRUE_FATAL(statement.kind == STATEMENT_CASE)
    CU_ASSERT_TRUE_FATAL(statement.value.case_.expression == NULL)
    CU_ASSERT_TRUE_FATAL(statement.value.case_.statement->kind == STATEMENT_EMPTY)
}

void test_parse_case_statement(void) {
        const char *input = "case 4: ;"; // case + empty statement
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);

    statement_t statement;
    CU_ASSERT_TRUE_FATAL(parse_statement(&parser, &statement))
    CU_ASSERT_TRUE_FATAL(statement.kind == STATEMENT_CASE)
    CU_ASSERT_TRUE_FATAL(statement.value.case_.expression != NULL)
    CU_ASSERT_TRUE_FATAL(statement.value.case_.statement->kind == STATEMENT_EMPTY)
}

void test_parse_program(void) {
        const char* input = "float square(float);\nfloat square(float val) {\n\treturn val * val;\n}\nint main(void) {\n\treturn square(2.0);\n}";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);

    translation_unit_t program;
    CU_ASSERT_TRUE_FATAL(parse(&parser, &program))
}

void test_parse_program_typedef_used_in_different_scope(void) {
        const char *input =
        "typedef int integer;\n"
        "int main(void) {\n"
        "    integer foo = 1;\n"
        "    return foo;\n"
        "}\n";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);

    translation_unit_t program;
    CU_ASSERT_TRUE_FATAL(parse(&parser, &program))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
}

void test_parse_program_typedef_identifier_shadowing(void) {
        const char *input =
        "typedef int identifier; // <-- identifier is a typedef-name\n"
        "identifier bar\n;"
        "int foo(identifier identifier) // <-- identifier becomes an identifier after we handle the first parameter declaration\n"
        "{\n"
        "    int bar = identifier + 1;\n"
        "    return bar;\n"
        "}\n";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);

    translation_unit_t program;
    CU_ASSERT_TRUE_FATAL(parse(&parser, &program))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
}

void test_parse_program_illegal_symbol_redefinition_in_function_scope(void) {
        const char *input =
        "void foo(int a) \n"   // <-- declare a as a parameter
        "{\n"                  // <-- in c, this doesn't create a new block scope
        "    typedef float a;\n" // <-- error: redefinition of a
        "}\n";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);

    translation_unit_t program;
    CU_ASSERT_FALSE_FATAL(parse(&parser, &program))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 1)

}

void test_parse_enum_declaration(void) {
        const char *input = "enum Foo { A, B = 4, };";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    ptr_vector_t declarations = VEC_INIT;
    CU_ASSERT_TRUE_FATAL(parse_declaration(&parser, &declarations))
    CU_ASSERT_EQUAL_FATAL(declarations.size, 1)
    declaration_t *decl = declarations.buffer[0];
    const type_t *enum_type = decl->type;
    CU_ASSERT_TRUE_FATAL(enum_type->kind == TYPE_ENUM)
    CU_ASSERT_TRUE_FATAL(enum_type->value.enum_specifier.identifier != NULL)
    CU_ASSERT_STRING_EQUAL(enum_type->value.enum_specifier.identifier->value, "Foo")
    CU_ASSERT_TRUE_FATAL(enum_type->value.enum_specifier.enumerators.size == 2)
    CU_ASSERT_STRING_EQUAL(enum_type->value.enum_specifier.enumerators.buffer[0].identifier->value, "A")
    CU_ASSERT_PTR_NULL(enum_type->value.enum_specifier.enumerators.buffer[0].value)
    CU_ASSERT_STRING_EQUAL(enum_type->value.enum_specifier.enumerators.buffer[1].identifier->value, "B")
    CU_ASSERT_PTR_NOT_NULL(enum_type->value.enum_specifier.enumerators.buffer[1].value)
}

void test_parse_var_enum_declaration_no_list(void) {
        const char *input = "enum Foo foo;";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    ptr_vector_t declarations = VEC_INIT;
    CU_ASSERT_TRUE_FATAL(parse_declaration(&parser, &declarations))
    CU_ASSERT_EQUAL_FATAL(declarations.size, 1)
    declaration_t *decl = declarations.buffer[0];
    const type_t *enum_type = decl->type;
    CU_ASSERT_TRUE_FATAL(enum_type->kind == TYPE_ENUM)
    CU_ASSERT_TRUE_FATAL(enum_type->value.enum_specifier.identifier != NULL)
    CU_ASSERT_STRING_EQUAL(enum_type->value.enum_specifier.identifier->value, "Foo")
    CU_ASSERT_STRING_EQUAL(decl->identifier->value, "foo")
}

void test_parse_sizeof_typedef_name(void) {
        const char *input =
        "void test() {\n"
        "    typedef unsigned long size_t;\n"
        "    sizeof(size_t);\n"
        "}\n";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    translation_unit_t program;
    CU_ASSERT_TRUE_FATAL(parse(&parser, &program))
    CU_ASSERT_EQUAL_FATAL(parser.errors.size, 0)
    CU_ASSERT_EQUAL_FATAL(program.length, 1)
    function_definition_t *function = program.external_declarations[0]->value.function_definition;
    CU_ASSERT_TRUE_FATAL(function->body->kind == STATEMENT_COMPOUND)
    block_item_t **block_items = (block_item_t**) function->body->value.compound.block_items.buffer;
    CU_ASSERT_TRUE_FATAL(function->body->value.compound.block_items.size == 2)
    block_item_t *typedef_declaration = block_items[0];
    block_item_t *sizeof_statement = block_items[1];
    CU_ASSERT_TRUE_FATAL(sizeof_statement->kind == BLOCK_ITEM_STATEMENT && sizeof_statement->value.statement->kind == STATEMENT_EXPRESSION)
    expression_t *sizeof_expression = sizeof_statement->value.statement->value.expression;
    CU_ASSERT_TRUE_FATAL(sizeof_expression->kind == EXPRESSION_SIZEOF);
}

void test_parse___builtin_va_list_declaration(void) {
    const char *input = "__builtin_va_list args;";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    ptr_vector_t declarations = VEC_INIT;
    CU_ASSERT_TRUE_FATAL(parse_declaration(&parser, &declarations))
    CU_ASSERT_EQUAL_FATAL(declarations.size, 1)
    declaration_t *decl = declarations.buffer[0];
    CU_ASSERT_TRUE_FATAL(decl->type->kind == TYPE_BUILTIN)
    CU_ASSERT_TRUE_FATAL(strcmp(decl->type->value.builtin_name, "__builtin_va_list") == 0)
    CU_ASSERT_STRING_EQUAL_FATAL(decl->identifier->value, "args")
}

void test_parse___builtin_va_arg(void) {
    const char *input = "__builtin_va_arg(args, int)";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t expr;
    CU_ASSERT_TRUE_FATAL(parse_expression(&parser, &expr))
    CU_ASSERT_TRUE_FATAL(expr.kind == EXPRESSION_CALL)
    CU_ASSERT_TRUE_FATAL(expr.value.call.callee->kind == EXPRESSION_PRIMARY)
    CU_ASSERT_STRING_EQUAL_FATAL(expr.value.call.callee->value.primary.value.token.value, "__builtin_va_arg")
    CU_ASSERT_TRUE_FATAL(expr.value.call.arguments.size == 2)
    // second argument should be a type
    expression_t *type_arg = (expression_t *) expr.value.call.arguments.buffer[1];
    CU_ASSERT_TRUE_FATAL(type_arg->kind == EXPRESSION_TYPE)
    CU_ASSERT_TRUE_FATAL(type_arg->value.type->kind == TYPE_INTEGER)
}

void test_parse___builtin_va_arg_invalid_type_name(void ) {
    const char *input = "__builtin_va_arg(args, badtype)";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    parser_t parser = pinit(lexer);
    expression_t expr;
    CU_ASSERT_FALSE_FATAL(parse_expression(&parser, &expr))
    CU_ASSERT_TRUE_FATAL(parser.errors.size >= 1)
}

int parser_tests_init_suite(void) {
    CU_pSuite pSuite = CU_add_suite("parser", NULL, NULL);
    if (NULL == CU_add_test(pSuite, "primary expression - identifier", test_parse_primary_expression_ident) ||
        NULL == CU_add_test(pSuite, "primary expression - integer", test_parse_primary_expression_int) ||
        NULL == CU_add_test(pSuite, "primary expression - float", test_parse_primary_expression_float) ||
        NULL == CU_add_test(pSuite, "primary expression - char", test_parse_primary_expression_char) ||
        NULL == CU_add_test(pSuite, "primary expression - parenthesized", test_parse_primary_expression_parenthesized) ||
        NULL == CU_add_test(pSuite, "primary expression - parenthesized identifier", test_parse_primary_expression_parenthesized_identifier) ||
        NULL == CU_add_test(pSuite, "postfix expression - function call", test_parse_postfix_expression_function_call) ||
        NULL == CU_add_test(pSuite, "postfix expression - array subscript", test_parse_postfix_expression_array_subscript) ||
        NULL == CU_add_test(pSuite, "postfix expression - multiple postfix expressions", test_parse_postfix_expression_2d_array_subscript) ||
        NULL == CU_add_test(pSuite, "postfix expression - member access", test_parse_postfix_expression_member_access) ||
        NULL == CU_add_test(pSuite, "type name - function pointer", test_parse_type_name_function_pointer) ||
        NULL == CU_add_test(pSuite, "unary expression - sizeof constant", test_parse_unary_sizeof_constant) ||
        NULL == CU_add_test(pSuite, "unary expression - sizeof (type)", test_parse_unary_sizeof_type) ||
        NULL == CU_add_test(pSuite, "unary expression - sizeof (more complicated type)", test_parse_unary_sizeof_function_pointer_type) ||
        NULL == CU_add_test(pSuite, "unary expression - sizeof (expression)", test_parse_unary_sizeof_parenthesized_expression) ||
        NULL == CU_add_test(pSuite, "unary pre-increment", test_parse_prefix_increment) ||
        NULL == CU_add_test(pSuite, "unary pre-decrement", test_parse_prefix_decrement) ||
        NULL == CU_add_test(pSuite, "unary post-increment", test_parse_postfix_increment) ||
        NULL == CU_add_test(pSuite, "unary post-decrement", test_parse_postfix_decrement) ||
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
        NULL == CU_add_test(pSuite, "compound assignment expression (add)", test_parse_compound_assignment_expression_add) ||
        NULL == CU_add_test(pSuite, "compound assignment expression (div)", test_parse_compound_assignment_expression_div) ||
        NULL == CU_add_test(pSuite, "compound literal", test_parse_compound_literal_expression) ||
        NULL == CU_add_test(pSuite, "int declaration specifiers", test_parse_int_declaration_specifiers) ||
        NULL == CU_add_test(pSuite, "invalid declaration specifiers", test_parse_invalid_declaration_specifiers) ||
        NULL == CU_add_test(pSuite, "struct definition", test_parse_struct_definition) ||
        NULL == CU_add_test(pSuite, "union definition", test_parse_union_definition) ||
        NULL == CU_add_test(pSuite, "struct with bitfields", test_parse_struct_definition_with_bitfields) ||
        NULL == CU_add_test(pSuite, "initializer - expression simple", test_parse_initializer_expression_simple) ||
        NULL == CU_add_test(pSuite, "initializer - array of integers", test_parse_initializer_list_array) ||
        NULL == CU_add_test(pSuite, "initializer - array of integers with trailing comma", test_parse_initializer_list_array_trailing_comma) ||
        NULL == CU_add_test(pSuite, "initializer - array index designator", test_parse_initializer_list_array_index_designator) ||
        NULL == CU_add_test(pSuite, "initializer - struct", test_parse_initializer_list_struct) ||
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
        NULL == CU_add_test(pSuite, "declaration - empty", test_parse_empty_global_declaration) ||
        NULL == CU_add_test(pSuite, "declaration - struct type", test_parse_struct_type_declaration) ||
        NULL == CU_add_test(pSuite, "declaration - typedef struct type", test_parse_typedef_struct_type) ||
        NULL == CU_add_test(pSuite, "declaration - typedef anonymous struct type", test_parse_typedef_anonymous_struct_type) ||
        NULL == CU_add_test(pSuite, "abstract declaration - pointer to int", test_abstract_declarator_pointer_int) ||
        NULL == CU_add_test(pSuite, "abstract declaration - function pointer", test_abstract_declarator_function_pointer) ||
        NULL == CU_add_test(pSuite, "function prototype (void)", test_parse_function_prototype_void) ||
        NULL == CU_add_test(pSuite, "function prototype", test_parse_function_prototype) ||
        NULL == CU_add_test(pSuite, "empty statement", test_parse_empty_statement) ||
        NULL == CU_add_test(pSuite, "expression statement", test_parse_expression_statement) ||
        NULL == CU_add_test(pSuite, "compound statement", test_parse_compound_statement) ||
        NULL == CU_add_test(pSuite, "compound statement with parse error", test_parse_compound_statement_with_error) ||
        NULL == CU_add_test(pSuite, "if statement", test_parse_if_statement) ||
        NULL == CU_add_test(pSuite, "if else statement", test_parse_if_else_statement) ||
        NULL == CU_add_test(pSuite, "if else if else statement", test_parse_if_else_if_else_statement) ||
        NULL == CU_add_test(pSuite, "switch statement", test_parse_switch_statement) ||
        NULL == CU_add_test(pSuite, "return statement", test_parse_return_statement) ||
        NULL == CU_add_test(pSuite, "while statement", test_parse_while_statement) ||
        NULL == CU_add_test(pSuite, "while statement with empty body", test_parse_while_statement_with_empty_body) ||
        NULL == CU_add_test(pSuite, "do while statement", test_parse_do_while_statement) ||
        NULL == CU_add_test(pSuite, "for statement", test_parse_for_statement) ||
        NULL == CU_add_test(pSuite, "for statement with no optional parts", test_parse_for_statement_no_optional_parts) ||
        NULL == CU_add_test(pSuite, "for statement with expression initializer", test_parse_for_statement_expr_initializer) ||
        NULL == CU_add_test(pSuite, "break statement", test_parse_break_statement) ||
        NULL == CU_add_test(pSuite, "continue statement", test_parse_continue_statement) ||
        NULL == CU_add_test(pSuite, "goto statement", test_parse_goto_statement) ||
        NULL == CU_add_test(pSuite, "labeled statement", test_parse_labeled_statement) ||
        NULL == CU_add_test(pSuite, "default case statement", test_parse_default_statement) ||
        NULL == CU_add_test(pSuite, "case statement", test_parse_case_statement) ||
        NULL == CU_add_test(pSuite, "external declaration - declaration", parse_external_declaration_declaration) ||
        NULL == CU_add_test(pSuite, "external declaration - prototype (var args)", parse_external_definition_prototype_var_args) ||
        NULL == CU_add_test(pSuite, "external declaration - function definition", parse_external_declaration_function_definition) ||
        NULL == CU_add_test(pSuite, "external declaration - function (void) definition", parse_external_definition_function_taking_void) ||
        NULL == CU_add_test(pSuite, "external declaration - typedef", test_parse_external_declaration_typedef) ||
        NULL == CU_add_test(pSuite, "external declaration - typedef ptr", test_parse_external_declaration_typedef_ptr) ||
        NULL == CU_add_test(pSuite, "external declaration - typedef const ptr", test_parse_external_declaration_typedef_const_ptr) ||
        NULL == CU_add_test(pSuite, "external declaration - using typedef", test_parse_external_declaration_that_uses_typedef) ||
        NULL == CU_add_test(pSuite, "external declaration - typedef volatile int", test_parse_external_declaration_typedef_volatile_int) ||
        NULL == CU_add_test(pSuite, "program", test_parse_program) ||
        NULL == CU_add_test(pSuite, "typedef used in lower scope", test_parse_program_typedef_used_in_different_scope) ||
        NULL == CU_add_test(pSuite, "illegal symbol redefinition in function scope", test_parse_program_illegal_symbol_redefinition_in_function_scope) ||
        NULL == CU_add_test(pSuite, "parse enum declaration", test_parse_enum_declaration) ||
        NULL == CU_add_test(pSuite, "parse enum variable declaration", test_parse_var_enum_declaration_no_list) ||
        NULL == CU_add_test(pSuite, "sizeof typedef name", test_parse_sizeof_typedef_name) ||
        NULL == CU_add_test(pSuite, "parse __builtin_va_list", test_parse___builtin_va_list_declaration) ||
        NULL == CU_add_test(pSuite, "parse __builtin_va_arg", test_parse___builtin_va_arg) ||
        NULL == CU_add_test(pSuite, "parse __builtin_va_arg - invalid type name", test_parse___builtin_va_arg_invalid_type_name)
    ) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    return 0;
}
