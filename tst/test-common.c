#include <malloc.h>
#include "lexer.h"
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
