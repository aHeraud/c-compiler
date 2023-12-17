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
                    perror("Invalid primary expression type");
                    assert(false);
            }
        case EXPRESSION_BINARY:
            if (left->binary.operator != right->binary.operator) {
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
    }
}
