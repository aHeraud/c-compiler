// Recursive descent parser for the C language, based on the reference c99 grammar: see docs/c99.bnf

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "parser.h"
#include "lexer.h"

source_span_t spanning_tokens(token_t *start, token_t *end) {
    return (source_span_t) {
            .start = start->position,
            .end = end->position,
    };
}

source_span_t span_starting(source_position_t start, token_t *end) {
    return (source_span_t) {
            .start = start,
            .end = end->position,
    };
}

source_span_t spanning(source_position_t start, source_position_t end) {
    return (source_span_t) {
            .start = start,
            .end = end,
    };
}

#define SPANNING_NEXT(start) spanning_tokens(start, next_token(parser))
#define SPAN_STARTING(start) span_starting(start, next_token(parser))

typedef struct ParseCheckpoint {
    size_t token_index;
    size_t error_index;
} parse_checkpoint_t;

void append_parse_error(parse_error_vector_t* vec, parse_error_t error) {
    VEC_APPEND(vec, error);
}

parser_t pinit(lexer_t lexer) {
    return (parser_t) {
            .lexer = lexer,
            .tokens = {.size = 0, .capacity = 0, .buffer = NULL},
            .errors = {.size = 0, .capacity = 0, .buffer = NULL},
            .next_token_index = 0,
    };
}

bool parse(parser_t* parser, expression_t* node) {
    return parse_expression(parser, node);
}

parse_checkpoint_t checkpoint(const parser_t* parser) {
    return (parse_checkpoint_t) {
        .token_index = parser->next_token_index,
        .error_index = parser->errors.size,
    };
}

void backtrack(parser_t* parser, parse_checkpoint_t checkpoint) {
    parser->next_token_index = checkpoint.token_index;
    parser->errors.size = checkpoint.error_index;
}

token_t *next_token(parser_t* parser) {
    token_t* token;
    if (parser->next_token_index < parser->tokens.size) {
        token = parser->tokens.buffer[parser->next_token_index];
    } else {
        token_t *next = malloc(sizeof(token_t));
        *next = lscan(&parser->lexer);
        if (next->kind != TK_EOF) {
            append_token_ptr(&parser->tokens.buffer, &parser->tokens.size, &parser->tokens.capacity, next);
        }
        token = parser->tokens.buffer[parser->tokens.size - 1];
    }
    return token;
}

source_position_t* current_position(parser_t* parser) {
    return &next_token(parser)->position;
}

bool accept(parser_t* parser, token_kind_t kind, token_t** token_out) {
    token_t* token = next_token(parser);
    bool eof = token->kind == TK_EOF;
    if (token->kind == kind) {
        if (!eof) parser->next_token_index++;
        if (token_out != NULL) *token_out = token;
        return true;
    } else {
        return false;
    }
}

bool require(parser_t* parser, token_kind_t kind, token_t** token_out, const char* production_name) {
    if (accept(parser, kind, token_out)) {
        return true;
    } else {
        token_t* token = next_token(parser);
        parse_error_t error;
        if (token->kind == TK_EOF) {
            error = (parse_error_t) {
                    .token = token,
                    .production_name = production_name,
                    .type = UNEXPECTED_END_OF_INPUT,
                    .unexpected_end_of_input = {
                            .expected = kind,
                    },
            };
        } else {
            error = (parse_error_t) {
                    .token = token,
                    .production_name = production_name,
                    .type = UNEXPECTED_TOKEN,
                    .unexpected_token = {
                            .expected = kind,
                            .actual = token->kind,
                    },
            };
        }
        append_parse_error(&parser->errors, error);
        return false;
    }
}

// Expressions

bool parse_expression(parser_t *parser, expression_t *node) {
    if (!parse_assignment_expression(parser, node)) {
        return false;
    }

    token_t *token = NULL;
    while (accept(parser, TK_COMMA, &token)) {
        expression_t *right = malloc(sizeof(expression_t));
        if (!parse_assignment_expression(parser, right)) {
            free(right);
            return false;
        }

        expression_t *left = malloc(sizeof(expression_t));
        *left = *node;
        *node = (expression_t) {
                .span = spanning(left->span.start, right->span.end),
                .type = EXPRESSION_BINARY,
                .binary = {
                        .left = left,
                        .right = right,
                        .binary_operator = BINARY_COMMA,
                        .operator = token,
                }
        };
    }

    return true;
}

binary_operator_t get_binary_operator(const token_t *token) {
    switch (token->kind) {
        case TK_ASSIGN:
            return BINARY_ASSIGN;
        case TK_MULTIPLY_ASSIGN:
            return BINARY_MULTIPLY_ASSIGN;
        case TK_DIVIDE_ASSIGN:
            return BINARY_DIVIDE_ASSIGN;
        case TK_MOD_ASSIGN:
            return BINARY_MODULO_ASSIGN;
        case TK_PLUS_ASSIGN:
            return BINARY_ADD_ASSIGN;
        case TK_MINUS_ASSIGN:
            return BINARY_SUBTRACT_ASSIGN;
        case TK_LSHIFT_ASSIGN:
            return BINARY_SHIFT_LEFT_ASSIGN;
        case TK_RSHIFT_ASSIGN:
            return BINARY_SHIFT_RIGHT_ASSIGN;
        case TK_BITWISE_AND_ASSIGN:
            return BINARY_BITWISE_AND_ASSIGN;
        case TK_BITWISE_OR_ASSIGN:
            return BINARY_BITWISE_OR_ASSIGN;
        case TK_BITWISE_XOR_ASSIGN:
            return BINARY_BITWISE_XOR_ASSIGN;
        case TK_LOGICAL_OR:
            return BINARY_LOGICAL_OR;
        case TK_LOGICAL_AND:
            return BINARY_LOGICAL_AND;
        case TK_BITWISE_OR:
            return BINARY_BITWISE_OR;
        case TK_BITWISE_XOR:
            return BINARY_BITWISE_XOR;
        case TK_AMPERSAND:
            return BINARY_BITWISE_AND;
        case TK_EQUALS:
            return BINARY_EQUAL;
        case TK_NOT_EQUALS:
            return BINARY_NOT_EQUAL;
        case TK_LESS_THAN:
            return BINARY_LESS_THAN;
        case TK_LESS_THAN_EQUAL:
            return BINARY_LESS_THAN_OR_EQUAL;
        case TK_GREATER_THAN:
            return BINARY_GREATER_THAN;
        case TK_GREATER_THAN_EQUAL:
            return BINARY_GREATER_THAN_OR_EQUAL;
        case TK_LSHIFT:
            return BINARY_SHIFT_LEFT;
        case TK_RSHIFT:
            return BINARY_SHIFT_RIGHT;
        case TK_PLUS:
            return BINARY_ADD;
        case TK_MINUS:
            return BINARY_SUBTRACT;
        case TK_STAR:
            return BINARY_MULTIPLY;
        case TK_SLASH:
            return BINARY_DIVIDE;
        case TK_PERCENT:
            return BINARY_MODULO;
        case TK_COMMA:
            return BINARY_COMMA;
        default:
            // This should never be reached
            fprintf(stderr, "%s:%d: Invalid binary operator %s", __FILE__, __LINE__, token->value);
            exit(1);
    }
}

/**
 * Parses a primary expression.
 *
 * <assignment-expression> ::= <conditional-expression>
 *                           | <unary-expression> <assignment-operator> <assignment-expression>
 *
 * @param parser Parser instance
 * @param expr Expression node to store the result in
 * @return false if an error occurred, true otherwise
 */
bool parse_assignment_expression(parser_t *parser, expression_t *expr) {
    if (!parse_conditional_expression(parser, expr)) {
        return false;
    }

    token_t *token;
    if (accept(parser, TK_ASSIGN, &token) ||
        accept(parser, TK_BITWISE_XOR_ASSIGN, &token) ||
        accept(parser, TK_MULTIPLY_ASSIGN, &token) ||
        accept(parser, TK_MULTIPLY_ASSIGN, &token) ||
        accept(parser, TK_DIVIDE_ASSIGN, &token) ||
        accept(parser, TK_MOD_ASSIGN, &token) ||
        accept(parser, TK_PLUS_ASSIGN, &token) ||
        accept(parser, TK_MINUS_ASSIGN, &token) ||
        accept(parser, TK_LSHIFT_ASSIGN, &token) ||
        accept(parser, TK_RSHIFT_ASSIGN, &token) ||
        accept(parser, TK_BITWISE_AND_ASSIGN, &token) ||
        accept(parser, TK_BITWISE_OR_ASSIGN, &token) ||
        accept(parser, TK_BITWISE_XOR_ASSIGN, &token)) {

        expression_t *right = malloc(sizeof(expression_t));
        if (!parse_assignment_expression(parser, right)) {
            free(right);
            return false;
        }

        expression_t *left = malloc(sizeof(expression_t));
        *left = *expr;
        *expr = (expression_t) {
                .span = spanning(left->span.start, right->span.end),
                .type = EXPRESSION_BINARY,
                .binary = {
                        .left = left,
                        .right = right,
                        .operator = token,
                        .binary_operator = get_binary_operator(token),
                }
        };
    }

    return true;
}

/**
 * Parses a conditional (ternary) expression.
 *
 * <conditional-expression> ::= <logical-or-expression>
 *                            | <logical-or-expression> '?' <expression> ':' <conditional-expression>
 *
 * @param parser Parser instance
 * @param expr Expression node to store the result in
 * @return false if an error occurred, true otherwise
 */
bool parse_conditional_expression(parser_t *parser, expression_t *expr) {
    if (!parse_logical_or_expression(parser, expr)) {
        return false;
    }

    token_t *token;
    if (accept(parser, TK_TERNARY, &token)) {
        expression_t *condition = malloc(sizeof(expression_t));
        *condition = *expr;

        expression_t *true_expression = malloc(sizeof(expression_t));
        if (!parse_expression(parser, true_expression)) {
            free(condition);
            free(true_expression);
            return false;
        }

        if (!require(parser, TK_COLON, NULL, "conditional-parse_expression")) {
            free(condition);
            free(true_expression);
            return false;
        }

        expression_t *false_expression = malloc(sizeof(expression_t));
        if (!parse_conditional_expression(parser, false_expression)) {
            free(condition);
            free(true_expression);
            free(false_expression);
            return false;
        }

        *expr = (expression_t) {
                .span = spanning(expr->span.start, false_expression->span.end),
                .type = EXPRESSION_TERNARY,
                .ternary = {
                        .condition = condition,
                        .true_expression = true_expression,
                        .false_expression = false_expression,
                }
        };
    }

    return true;
}

/**
 * Parses a logical or expression.
 *
 * <logical-or-expression> ::= <logical-and-expression>
 *                           | <logical-or-expression> '||' <logical-and-expression>
 *
 * After rewriting to eliminate left-recursion:
 * <logical-or-expression> ::= <logical-and-expression> <logical-or-expression'>*
 * <logical-or-expression'> ::= '||' <logical-and-expression>
 *
 * @param parser Parser instance
 * @param expr Expression node to store the result in
 * @return false if an error occurred, true otherwise
 */
bool parse_logical_or_expression(parser_t *parser, expression_t *expr) {
    if (!parse_logical_and_expression(parser, expr)) {
        return false;
    }

    token_t *token;
    while (accept(parser, TK_LOGICAL_OR, &token)) {
        expression_t *right = malloc(sizeof(expression_t));
        if (!parse_logical_and_expression(parser, right)) {
            free(right);
            return false;
        }

        expression_t *left = malloc(sizeof(expression_t));
        *left = *expr;
        *expr = (expression_t) {
                .span = spanning(left->span.start, right->span.end),
                .type = EXPRESSION_BINARY,
                .binary = {
                        .left = left,
                        .right = right,
                        .operator = token,
                        .binary_operator = BINARY_LOGICAL_OR,
                }
        };
    }

    return true;
}

/**
 * Parses a logical and expression.
 *
 * <logical-and-expression> ::= <inclusive-or-expression>
                              | <logical-and-expression> '&&' <inclusive-or-expression>
 *
 * After rewriting to eliminate left-recursion:
 *
 * <logical-and-expression> ::= <inclusive-or-expression> <logical-and-expression'>*
 * <logical-and-expression'> ::= '&&' <inclusive-or-expression>
 *
 * @param parser Parser instance
 * @param expr Expression node to store the result in
 * @return false if an error occurred, true otherwise
 */
bool parse_logical_and_expression(parser_t *parser, expression_t *expr) {
    if (!parse_inclusive_or_expression(parser, expr)) {
        return false;
    }

    token_t *token;
    while (accept(parser, TK_LOGICAL_AND, &token)) {
        expression_t *right = malloc(sizeof(expression_t));
        if (!parse_inclusive_or_expression(parser, right)) {
            free(right);
            return false;
        }

        expression_t *left = malloc(sizeof(expression_t));
        *left = *expr;
        *expr = (expression_t) {
                .span = spanning(left->span.start, right->span.end),
                .type = EXPRESSION_BINARY,
                .binary = {
                        .left = left,
                        .right = right,
                        .operator = token,
                        .binary_operator = BINARY_LOGICAL_AND,
                }
        };
    }

    return true;
}

/**
 * Parses an inclusive or expression.
 *
 * <inclusive-or-expression> ::= <exclusive-or-expression>
 *                             | <inclusive-or-expression> '|' <exclusive-or-expression>
 *
 * After rewriting to eliminate left-recursion:
 *
 * <inclusive-or-expression> ::= <exclusive-or-expression> <inclusive-or-expression'>*
 * <inclusive-or-expression'> ::= '|' <exclusive-or-expression> <inclusive-or-expression'>
 *
 * @param parser Parser instance
 * @param expr Expression node to store the result in
 * @return false if an error occurred, true otherwise
 */
bool parse_inclusive_or_expression(parser_t *parser, expression_t *expr) {
    if (!parse_exclusive_or_expression(parser, expr)) {
        return false;
    }

    token_t *token;
    while (accept(parser, TK_BITWISE_OR, &token)) {
        expression_t *right = malloc(sizeof(expression_t));
        if (!parse_exclusive_or_expression(parser, right)) {
            free(right);
            return false;
        }

        expression_t *left = malloc(sizeof(expression_t));
        *left = *expr;
        *expr = (expression_t) {
                .span = spanning(left->span.start, right->span.end),
                .type = EXPRESSION_BINARY,
                .binary = {
                        .left = left,
                        .right = right,
                        .operator = token,
                        .binary_operator = BINARY_BITWISE_OR,
                }
        };
    }

    return true;
}

/**
 * Parses an exclusive or expression.
 *
 * <exclusive-or-expression> ::= <and-expression>
 *                             | <exclusive-or-expression> '^' <and-expression>
 *
 * After rewriting to eliminate left-recursion:
 *
 * <exclusive-or-expression> ::= <and-expression> <exclusive-or-expression'>*
 * <exclusive-or-expression'> ::= '^' <and-expression>
 *
 * @param parser Parser instance
 * @param expr Expression node to store the result in
 * @return false if an error occurred, true otherwise
 */
bool parse_exclusive_or_expression(parser_t *parser, expression_t *expr) {
    if (!parse_equality_expression(parser, expr)) {
        return false;
    }

    token_t *token;
    while (accept(parser, TK_BITWISE_XOR, &token)) {
        expression_t *right = malloc(sizeof(expression_t));
        if (!parse_equality_expression(parser, right)) {
            free(right);
            return false;
        }

        expression_t *left = malloc(sizeof(expression_t));
        *left = *expr;
        *expr = (expression_t) {
                .span = spanning(left->span.start, right->span.end),
                .type = EXPRESSION_BINARY,
                .binary = {
                        .left = left,
                        .right = right,
                        .operator = token,
                        .binary_operator = BINARY_BITWISE_XOR,
                }
        };
    }

    return true;
}

/**
 * Parses an and expression.
 *
 * <and-expression> ::= <equality-expression>
 *                    | <and-expression> '&' <equality-expression>
 *
 * After rewriting to eliminate left-recursion:
 *
 * <and-expression> ::= <equality-expression> <and-expression'>*
 * <and-expression'> ::= '&' <equality-expression>
 *
 * @param parser Parser instance
 * @param node Expression node to store the result in
 * @return false if an error occurred, true otherwise
 */
bool parse_and_expression(parser_t* parser, expression_t* expr) {
    if (!parse_equality_expression(parser, expr)) {
        return false;
    }

    token_t *token;
    while (accept(parser, TK_AMPERSAND, &token)) {
        expression_t *right = malloc(sizeof(expression_t));
        if (!parse_equality_expression(parser, right)) {
            free(right);
            return false;
        }

        expression_t *left = malloc(sizeof(expression_t));
        *left = *expr;
        *expr = (expression_t) {
                .span = spanning(left->span.start, right->span.end),
                .type = EXPRESSION_BINARY,
                .binary = {
                        .left = left,
                        .right = right,
                        .operator = token,
                        .binary_operator = BINARY_BITWISE_AND,
                }
        };
    }

    return true;
}

bool equality_expression_prime(parser_t *parser, expression_t *expr, const token_t *operator);

/**
 * Parses an equality expression.
 *
 * <equality-expression> ::= <relational-expression>
 *                        | <equality-expression> ('==' | '!=') <relational-expression>
 *
 * After rewriting to eliminate left-recursion:
 *
 * <equality-expression> ::= <relational-expression> <equality-expression'>*
 * <equality-expression'> ::= ('==' | '!=') <relational-expression>
 *
 * @param parser Parser instance
 * @param expr Expression node to store the result in
 * @return false if an error occurred, true otherwise
 */
bool parse_equality_expression(parser_t *parser, expression_t *expr) {
    if (!parse_relational_expression(parser, expr)) {
        return false;
    }

    token_t *token;
    if (accept(parser, TK_EQUALS, &token) || accept(parser, TK_NOT_EQUALS, &token)) {
        return equality_expression_prime(parser, expr, token);
    } else {
        return true;
    }
}

bool equality_expression_prime(parser_t *parser, expression_t *expr, const token_t *operator) {
    expression_t *right = malloc(sizeof(expression_t));
    if (!parse_relational_expression(parser, right)) {
        free(right);
        return false;
    }

    binary_operator_t binary_operator = get_binary_operator(operator);

    expression_t *left = malloc(sizeof(expression_t));
    *left = *expr;
    *expr = (expression_t) {
            .span = spanning(left->span.start, right->span.end),
            .type = EXPRESSION_BINARY,
            .binary = {
                    .left = left,
                    .right = right,
                    .operator = operator,
                    .binary_operator = binary_operator,
            }
    };

    token_t *token;
    if (accept(parser, TK_EQUALS, &token) || accept(parser, TK_NOT_EQUALS, &token)) {
        return equality_expression_prime(parser, expr, token);
    } else {
        return true;
    }
}

bool relational_expression_prime(parser_t *parser, expression_t *expr, const token_t *operator);

/**
 * <relational-expression> ::= <shift-expression>
 *                          | <relational-expression> ('<' | '>' | '<=' | '>=') <shift-expression>
 *
 * After rewriting to eliminate left-recursion:
 *
 * <relational-expression> ::= <shift-expression> <relational-expression'>*
 * <relational-expression'> ::= ('<' | '>' | '<=' | '>=') <shift-expression>
 *
 * @param parser Parser instance
 * @param expr Expression node to store the result in
 * @return false if an error occurred, true otherwise
 */
bool parse_relational_expression(parser_t *parser, expression_t *expr) {
    if (!parse_shift_expression(parser, expr)) {
        return false;
    }

    token_t *token;
    if (accept(parser, TK_LESS_THAN, &token) ||
        accept(parser, TK_GREATER_THAN, &token) ||
        accept(parser, TK_LESS_THAN_EQUAL, &token) ||
        accept(parser, TK_GREATER_THAN_EQUAL, &token)) {
        return relational_expression_prime(parser, expr, token);
    } else {
        return true;
    }
}

bool relational_expression_prime(parser_t *parser, expression_t *expr, const token_t *operator) {
    expression_t *right = malloc(sizeof(expression_t));
    if (!parse_shift_expression(parser, right)) {
        free(right);
        return false;
    }

    binary_operator_t binary_operator = get_binary_operator(operator);

    expression_t *left = malloc(sizeof(expression_t));
    *left = *expr;
    *expr = (expression_t) {
            .span = spanning(left->span.start, right->span.end),
            .type = EXPRESSION_BINARY,
            .binary = {
                    .left = left,
                    .right = right,
                    .operator = operator,
                    .binary_operator = binary_operator,
            }
    };

    token_t *token;
    if (accept(parser, TK_LESS_THAN, &token) ||
        accept(parser, TK_LESS_THAN_EQUAL, &token) ||
        accept(parser, TK_GREATER_THAN, &token) ||
        accept(parser, TK_GREATER_THAN_EQUAL, &token)) {
        return relational_expression_prime(parser, expr, token);
    } else {
        return true;
    }
}

bool shift_expression_prime(parser_t *parser, expression_t *expr, const token_t *operator);

/**
 * <shift-expression> ::= <additive-expression>
 *                      | <shift-expression> '<<' <additive-expression>
 *                      | <shift-expression> '>>' <additive-expression>
 *
 * After rewriting to eliminate left-recursion:
 *
 * <shift-expression> ::= <additive-expression> <shift-expression'>*
 * <shift-expression'> ::= '<<' <additive-expression>
 *                      | '>>' <additive-expression>
 *
 * @param parser Parser instance
 * @param expr Expression node to store the resulting expression in
 * @return false if an error occurred, true otherwise
 */
bool parse_shift_expression(parser_t *parser, expression_t *expr) {
    if (!parse_additive_expression(parser, expr)) {
        return false;
    }

    token_t *token;
    if (accept(parser, TK_LSHIFT, &token) ||
        accept(parser, TK_RSHIFT, &token)) {
        return shift_expression_prime(parser, expr, token);
    } else {
        return true;
    }
}

bool shift_expression_prime(parser_t *parser, expression_t *expr, const token_t *operator) {
    expression_t *right = malloc(sizeof(expression_t));
    if (!parse_additive_expression(parser, right)) {
        free(right);
        return false;
    }

    expression_t *left = malloc(sizeof(expression_t));
    *left = *expr;
    *expr = (expression_t) {
            .span = spanning(left->span.start, right->span.end),
            .type = EXPRESSION_BINARY,
            .binary = {
                    .left = left,
                    .right = right,
                    .operator = operator,
                    .binary_operator = operator->kind == TK_LSHIFT ? BINARY_SHIFT_LEFT : BINARY_SHIFT_RIGHT,
            }
    };

    token_t *token;
    if (accept(parser, TK_LSHIFT, &token) ||
        accept(parser, TK_RSHIFT, &token)) {
        return shift_expression_prime(parser, expr, token);
    } else {
        return true;
    }
}

bool additive_expression_prime(parser_t *parser, expression_t *expr, const token_t *operator);

/**
 * <additive-expression> ::= <multiplicative-expression>
 *                         | <additive-expression> '+' <multiplicative-expression>
 *                         | <additive-expression> '-' <multiplicative-expression>
 *
 * As this rule is left-recursive, it must be rewritten to eliminate the left-recursion while preserving the operator
 * associativity. The new rule becomes:
 *
 * <additive-expression> ::= <multiplicative-expression> <additive-expression'>*
 * <additive-expression'> ::= '+' <multiplicative-expression> <additive-expression'>
 *                         | '-' <multiplicative-expression> <additive-expression'>
 *
 * @param parser Parser instance
 * @param expr Expression node to store the resulting expression in
 * @return false if an error occurred, true otherwise
 */
bool parse_additive_expression(parser_t* parser, expression_t* expr) {
    if (!parse_multiplicative_expression(parser, expr)) {
        return false;
    }

    token_t *token;
    if (accept(parser, TK_PLUS, &token) || accept(parser, TK_MINUS, &token)) {
        return additive_expression_prime(parser, expr, token);
    } else {
        return true;
    }
}

bool additive_expression_prime(parser_t* parser, expression_t* expr, const token_t *operator) {
    expression_t *right = malloc(sizeof(expression_t));
    if (!parse_cast_expression(parser, right)) {
        free(right);
        return false;
    }

    expression_t *left = malloc(sizeof(expression_t));
    *left = *expr;
    *expr = (expression_t) {
            .span = spanning(left->span.start, right->span.end),
            .type = EXPRESSION_BINARY,
            .binary = {
                    .left = left,
                    .right = right,
                    .operator = operator,
                    .binary_operator = operator-> kind == TK_PLUS ? BINARY_ADD : BINARY_SUBTRACT
            }
    };

    token_t *token;
    if (accept(parser, TK_PLUS, &token) ||
        accept(parser, TK_MINUS, &token)) {
        return additive_expression_prime(parser, expr, token);
    } else {
        return true;
    }
}

bool multiplicative_expression_prime(parser_t* parser, expression_t* node, const token_t *operator);

/**
 * <multiplicative-expression> ::= <cast-expression>
 *                               | <multiplicative-expression> '*' <cast-expression>
 *                               | <multiplicative-expression> '/' <cast-expression>
 *                               | <multiplicative-expression> '%' <cast-expression>
 *
 * The rule is left-recursive and cannot be directly parsed using recursive descent.
 * We rewrite it to eliminate the left-recursion, while preserving the operator associativity:
 *
 * <multiplicative-expression> ::= <cast-expression> <multiplicative-expression'>*
 * <multiplicative-expression'> ::= '*' <cast-expression> <multiplicative-expression'>
 *                                | '/' <cast-expression> <multiplicative-expression'>
 *                                | '%' <cast-expression> <multiplicative-expression'>
 *
 * @param parser Parser instance
 * @param expr Expression node to store the resulting expression in
 * @return false if an error occurred, true otherwise
 */
bool parse_multiplicative_expression(parser_t* parser, expression_t* expr) {
    if (!parse_cast_expression(parser, expr)) {
        return false;
    }

    token_t *token;
    if (accept(parser, TK_STAR, &token) ||
        accept(parser, TK_SLASH, &token) ||
        accept(parser, TK_PERCENT, &token)) {
        return multiplicative_expression_prime(parser, expr, token);
    } else {
        return true;
    }
}

bool multiplicative_expression_prime(parser_t* parser, expression_t* expr, const token_t *operator) {
    expression_t *right = malloc(sizeof(expression_t));
    if (!parse_cast_expression(parser, right)) {
        free(right);
        return false;
    }

    expression_t *left = malloc(sizeof(expression_t));
    *left = *expr;
    *expr = (expression_t) {
            .span = spanning(left->span.start, right->span.end),
            .type = EXPRESSION_BINARY,
            .binary = {
                .left = left,
                .right = right,
                .operator = operator,
                .binary_operator = operator-> kind == TK_STAR ? BINARY_MULTIPLY :
                            operator-> kind == TK_SLASH ? BINARY_DIVIDE :
                            BINARY_MODULO,
            }
    };

    token_t *token;
    if (accept(parser, TK_STAR, &token) ||
        accept(parser, TK_SLASH, &token) ||
        accept(parser, TK_PERCENT, &token)) {
        return multiplicative_expression_prime(parser, expr, token);
    } else {
        return true;
    }
}

bool parse_cast_expression(parser_t *parser, expression_t *expr) {
    if (accept(parser, TK_LPAREN, NULL)) {
        fprintf(stderr, "TODO: Implement parsing cast-parse_expression ::= '(' <type-name> ')' <cast-expression>\n");
        assert(false);
    } else {
        return parse_unary_expression(parser, expr);
    }
}

// helper function to parse: <unary-operator> <cast-expression>
bool unary_op(parser_t *parser, expression_t* expr, token_t *token) {
    expression_t *operand = malloc(sizeof(expression_t));
    if (!parse_cast_expression(parser, operand)) {
        free(operand);
        return false;
    }

    int operator;
    switch (token->kind) {
        case TK_AMPERSAND:
            operator = UNARY_ADDRESS_OF;
            break;
        case TK_STAR:
            operator = UNARY_DEREFERENCE;
            break;
        case TK_PLUS:
            operator = UNARY_PLUS;
            break;
        case TK_MINUS:
            operator = UNARY_MINUS;
            break;
        case TK_TILDE:
            operator = UNARY_BITWISE_NOT;
            break;
        case TK_EXCLAMATION:
            operator = UNARY_LOGICAL_NOT;
            break;
        default:
            assert(false);
    }

    *expr = (expression_t) {
            .span = SPANNING_NEXT(token),
            .type = EXPRESSION_UNARY,
            .unary = {
                    .operator = operator,
                    .operand = operand,
            },
    };
    return true;
}

bool parse_unary_expression(parser_t *parser, expression_t *expr) {
    token_t *token;

    if (accept(parser, TK_INCREMENT, &token)) {
        expression_t *operand = malloc(sizeof(expression_t));
        if (!parse_unary_expression(parser, operand)) {
            free(operand);
            return false;
        }

        *expr = (expression_t) {
            .span = SPANNING_NEXT(token),
            .type = EXPRESSION_UNARY,
            .unary = {
                .operator = UNARY_PRE_INCREMENT,
                .operand = operand,
            },
        };
        return true;
    } else if (accept(parser, TK_DECREMENT, &token)) {
        expression_t *operand = malloc(sizeof(expression_t));
        if (!parse_unary_expression(parser, operand)) {
            free(operand);
            return false;
        }

        *expr = (expression_t) {
            .span = SPANNING_NEXT(token),
            .type = EXPRESSION_UNARY,
            .unary = {
                .operator = UNARY_PRE_DECREMENT,
                .operand = operand,
            }
        };
        return true;
    } else if (accept(parser, TK_AMPERSAND, &token) ||
               accept(parser, TK_STAR, &token) ||
               accept(parser, TK_PLUS, &token) ||
               accept(parser, TK_MINUS, &token) ||
               accept(parser, TK_TILDE, &token) ||
               accept(parser, TK_EXCLAMATION, &token)) {
        return unary_op(parser, expr, token);
    } else if (accept(parser, TK_SIZEOF, &token)) {
        fprintf(stderr, "TODO: Implement parsing unary-expression ::= 'sizeof' unary-parse_expression\n");
        assert(false);
    } else {
        return parse_postfix_expression(parser, expr);
    }
}

bool parse_postfix_expression(parser_t *parser, expression_t *expr) {
    if (!parse_primary_expression(parser, expr)) {
        return false;
    }

    token_t *token = NULL;
    if (accept(parser, TK_LBRACKET, NULL)) {
        // array indexing
        expression_t *index = malloc(sizeof(expression_t));
        if (!parse_expression(parser, index)) {
            free(index);
            return false;
        }

        if (!require(parser, TK_RBRACKET, NULL, "postfix-parse_expression")) {
            free(index);
            return false;
        }

        expression_t *array = malloc(sizeof(expression_t));
        *array = *expr;

        *expr = (expression_t) {
            .span = spanning(array->span.start, *current_position(parser)),
            .type = EXPRESSION_ARRAY_SUBSCRIPT,
            .array_subscript = {
                .array = array,
                .index = index,
            },
        };

        return true;
    } else if (accept(parser, TK_LPAREN, NULL)) {
        // function call
        // parse argument list
        ptr_vector_t arguments = {.size = 0, .capacity = 0, .buffer = NULL};
        while (next_token(parser) ->kind != TK_RPAREN && next_token(parser)->kind != TK_EOF) {
            expression_t *argument = malloc(sizeof(expression_t));
            if (!parse_assignment_expression(parser, argument)) {
                free(argument);
                // TODO: cleanup arguments
                return false;
            }

            append_ptr(&arguments.buffer, &arguments.size, &arguments.capacity, argument);

            if (!accept(parser, TK_COMMA, NULL)) {
                break;
            }
        }

        if (!require(parser, TK_RPAREN, NULL, "argument-parse_expression-list")) {
            // TODO: cleanup
            return false;
        }

        expression_t *callee = malloc(sizeof(expression_t));
        *callee = *expr;

        *expr = (expression_t) {
            .span = spanning(callee->span.start, *current_position(parser)),
            .type = EXPRESSION_CALL,
            .call = {
                .callee = callee,
                .arguments = arguments,
            },
        };

        return true;
    } else if (accept(parser, TK_DOT, &token) || accept(parser, TK_ARROW, &token)) {
        // struct member access
        token_t *identifier;
        if (!require(parser, TK_IDENTIFIER, &identifier, "postfix-parse_expression")) {
            return false;
        }

        expression_t *struct_or_union = malloc(sizeof(expression_t));
        *struct_or_union = *expr;

        *expr = (expression_t) {
            .span = spanning(struct_or_union->span.start, *current_position(parser)),
            .type = EXPRESSION_MEMBER_ACCESS,
            .member_access = {
                .struct_or_union = struct_or_union,
                .operator = *token,
                .member = *identifier,
            },
        };
        return true;
    } else if (accept(parser, TK_INCREMENT, NULL)) {
        // post-increment
        expression_t *operand = malloc(sizeof(expression_t));
        *operand = *expr;
        *expr = (expression_t) {
            .span = SPAN_STARTING(operand->span.start),
            .type = EXPRESSION_UNARY,
            .unary = {
                    .operator = UNARY_POST_INCREMENT,
                    .operand = operand,
            },
        };
        return true;
    } else if (accept(parser, TK_DECREMENT, NULL)) {
        expression_t *operand = malloc(sizeof(expression_t));
        *operand = *expr;

        *expr = (expression_t) {
            .span = {
                .start = operand->span.start,
                .end = *current_position(parser),
            },
            .type = EXPRESSION_UNARY,
            .unary = {
                .operator = UNARY_POST_DECREMENT,
                .operand = operand,
            },
        };
        return true;
    } else {
        return true;
    }
}

bool parse_primary_expression(parser_t* parser, expression_t* expr) {
    token_t *token;
    source_position_t start = *current_position(parser);

    if (accept(parser, TK_IDENTIFIER, &token)) {
        *expr = (expression_t) {
            .span = SPAN_STARTING(start),
            .type = EXPRESSION_PRIMARY,
            .primary = {
                .type = PE_IDENTIFIER,
                .token = *token,
            }
        };
        return true;
    } else if (accept(parser, TK_INTEGER_CONSTANT, &token) ||
               accept(parser, TK_FLOATING_CONSTANT, &token) ||
               accept(parser, TK_CHAR_LITERAL, &token)) {
        *expr = (expression_t) {
            .span = SPAN_STARTING(start),
            .type = EXPRESSION_PRIMARY,
            .primary = {
                .type = PE_CONSTANT,
                .token = *token,
            },
        };
        return true;
    } else if (accept(parser, TK_STRING_LITERAL, &token)) {
        *expr = (expression_t) {
            .span = SPAN_STARTING(start),
            .type = EXPRESSION_PRIMARY,
            .primary = {
                .type = PE_STRING_LITERAL,
                .token = *token,
            },
        };
        return true;
    } else if (accept(parser, TK_LPAREN, &token)) {
        expression_t *expr2 = malloc(sizeof(expression_t));
        if (!parse_expression(parser, expr2)) {
            free(expr2);
            return false;
        }

        if (!require(parser, TK_RPAREN, NULL, "primary-parse_expression")) {
            free(expr2);
            return false;
        }

        *expr = (expression_t) {
            .span = spanning(start, *current_position(parser)),
            .type = EXPRESSION_PRIMARY,
            .primary = {
                .type = PE_EXPRESSION,
                .expression = expr2,
            },
        };

        return true;
    } else {
        // TODO: error handling/recovery
        fprintf(stderr, "Expected primary-parse_expression at \n");
        return false;
    }
}
