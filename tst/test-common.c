#include <assert.h>
#include <malloc.h>
#include "lexer.h"
#include "parser.h"
#include "util/vectors.h"
#include "test-common.h"

token_kind_t* token_kind_array(token_t* tokens, size_t size) {
    token_kind_t* array = malloc(size * sizeof(token_kind_t));
    for (size_t i = 0; i < size; i++) {
        array[i] = tokens[i].kind;
    }
    return array;
}

const char** token_value_array(token_t* tokens, size_t size) {
    const char** array = malloc(size * sizeof(char*));
    for (size_t i = 0; i < size; i++) {
        array[i] = tokens[i].value;
    }
    return array;
}

char* format_token_kind_array(const token_kind_t* array, size_t size) {
    char_vector_t vec = {.buffer = NULL, .size = 0, .capacity = 0};
    append_char(&vec.buffer, &vec.size, &vec.capacity, '[');
    for (size_t i = 0; i < size; i++) {
        const char* str = token_kind_names[array[i]];
        append_chars(&vec.buffer, &vec.size, &vec.capacity, str);
        if (i < size - 1) {
            append_chars(&vec.buffer, &vec.size, &vec.capacity, ", ");
        }
    }
    append_char(&vec.buffer, &vec.size, &vec.capacity, ']');
    append_char(&vec.buffer, &vec.size, &vec.capacity, '\0');
    shrink_char_vector(&vec.buffer, &vec.size, &vec.capacity);
    return vec.buffer;
}

char* format_string_array(const char** array, size_t size) {
    char_vector_t vec = {.buffer = NULL, .size = 0, .capacity = 0};
    append_char(&vec.buffer, &vec.size, &vec.capacity, '[');
    for (size_t i = 0; i < size; i++) {
        append_char(&vec.buffer, &vec.size, &vec.capacity, '"');
        append_chars(&vec.buffer, &vec.size, &vec.capacity, array[i]);
        append_char(&vec.buffer, &vec.size, &vec.capacity, '"');
        if (i < size - 1) {
            append_chars(&vec.buffer, &vec.size, &vec.capacity, ", ");
        }
    }
    append_char(&vec.buffer, &vec.size, &vec.capacity, ']');
    append_char(&vec.buffer, &vec.size, &vec.capacity, '\0');
    shrink_char_vector(&vec.buffer, &vec.size, &vec.capacity);
    return vec.buffer;
}

// Compare two expression nodes for equality.
bool expression_eq(const expression_t *left, const expression_t *right) {
    if (left == NULL || right == NULL) {
        return left == right;
    }

    if (left->type != right->type) {
        return false;
    }

    switch (left->type) {
        case EXPRESSION_PRIMARY:
            if (left->primary.type != right->primary.type) {
                return false;
            }
            switch (left->primary.type) {
                case PE_IDENTIFIER:
                case PE_CONSTANT:
                case PE_STRING_LITERAL:
                    return strcmp(left->primary.token.value, right->primary.token.value) == 0;
                case PE_EXPRESSION:
                    return expression_eq(left->primary.expression, right->primary.expression);
                default:
                    perror("Invalid primary parse_expression type");
                    assert(false);
            }
        case EXPRESSION_BINARY:
            if (left->binary.binary_operator != right->binary.binary_operator) {
                return false;
            }
            if (left->binary.operator->kind != right->binary.operator->kind) {
                return false;
            }
            return expression_eq(left->binary.left, right->binary.left) &&
                   expression_eq(left->binary.right, right->binary.right);
        case EXPRESSION_UNARY:
            if (left->unary.operator != right->unary.operator) {
                return false;
            }
            return expression_eq(left->unary.operand, right->unary.operand);
        case EXPRESSION_TERNARY:
            return expression_eq(left->ternary.condition, right->ternary.condition) &&
                   expression_eq(left->ternary.true_expression, right->ternary.true_expression) &&
                   expression_eq(left->ternary.false_expression, right->ternary.false_expression);
        case EXPRESSION_CALL:
            if (!expression_eq(left->call.callee, right->call.callee)) {
                return false;
            }
            if (left->call.arguments.size != right->call.arguments.size) {
                return false;
            }
            for (size_t i = 0; i < left->call.arguments.size; i++) {
                if (!expression_eq(left->call.arguments.buffer[i], right->call.arguments.buffer[i])) {
                    return false;
                }
            }
            return true;
        case EXPRESSION_ARRAY_SUBSCRIPT:
            return expression_eq(left->array_subscript.array, right->array_subscript.array) &&
                   expression_eq(left->array_subscript.index, right->array_subscript.index);
        case EXPRESSION_MEMBER_ACCESS:
            if (left->member_access.operator.kind != right->member_access.operator.kind) {
                return false;
            }
            if (left->member_access.member.kind != right->member_access.member.kind) {
                return false;
            }
            return expression_eq(left->member_access.struct_or_union, right->member_access.struct_or_union);
    }
}

bool statement_eq(const statement_t *left, const statement_t *right) {
    if (left == NULL || right == NULL) {
        return left == right;
    }

    if (left->type != right->type) {
        return false;
    }

    switch (left->type) {
        case STATEMENT_EMPTY:
            return true;
        case STATEMENT_EXPRESSION:
            return expression_eq(left->expression, right->expression);
        case STATEMENT_COMPOUND:
            if (left->compound.statements.size != right->compound.statements.size) {
                return false;
            }
            for (size_t i = 0; i < left->compound.statements.size; i++) {
                if (!statement_eq(left->compound.statements.buffer[i], right->compound.statements.buffer[i])) {
                    return false;
                }
            }
            return true;
        case STATEMENT_RETURN:
            assert(left->return_.keyword != NULL && right->return_.keyword != NULL);
            if (left->return_.keyword->kind != right->return_.keyword->kind) {
                return false;
            }
            return expression_eq(left->return_.expression, right->return_.expression);
    }
}
