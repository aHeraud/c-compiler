#include <malloc.h>
#include "parser/lexer.h"
#include "utils/vectors.h"
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
        .kind = EXPRESSION_PRIMARY,
        .span = dummy_span(),
        .value.primary = primary,
    };
    return expr;
}

expression_t *integer_constant(char* value) {
    return primary((primary_expression_t) {
        .kind = PE_CONSTANT,
        .value.token = (token_t) {
            .kind = TK_INTEGER_CONSTANT,
            .value = value,
            .position = dummy_position(),
        },
    });
}

expression_t *float_constant(char* value) {
    return primary((primary_expression_t) {
        .kind = PE_CONSTANT,
        .value.token = (token_t) {
            .kind = TK_FLOATING_CONSTANT,
            .value = value,
            .position = dummy_position(),
        },
    });
}

type_t *ptr_to(const type_t *type) {
    type_t * ptr = malloc(sizeof(type_t));
    *ptr = (type_t) {
        .kind = TYPE_POINTER,
        .is_volatile = false,
        .is_const = false,
        .storage_class = STORAGE_CLASS_AUTO,
        .value.pointer = {
            .base = type,
        },
    };
    return ptr;
}

type_t *array_of(const type_t *type, expression_t *size) {
    type_t * array = malloc(sizeof(type_t));
    *array = (type_t) {
        .kind = TYPE_ARRAY,
        .is_volatile = false,
        .is_const = false,
        .storage_class = STORAGE_CLASS_AUTO,
        .value.array = {
            .element_type = type,
            .size = size,
        },
    };
    return array;
}

