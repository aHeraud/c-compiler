#include <assert.h>
#include <malloc.h>
#include <stddef.h>
#include <string.h>

#include "vectors.h"

void append_char(char** buffer, size_t* buffer_len, size_t* buffer_max_len, char c) {
    if (*buffer_len + 1 >= *buffer_max_len) {
        *buffer_max_len > 0 ? (*buffer_max_len *= 2) : (*buffer_max_len = 1);
        *buffer = realloc(*buffer, *buffer_max_len);
        assert(buffer != NULL);
    }

    (*buffer)[*buffer_len] = c;
    *buffer_len += 1;
}

void append_chars(char** buffer, size_t* buffer_len, size_t* buffer_max_len, const char* s) {
    size_t len = strlen(s);
    if (*buffer_len + len >= *buffer_max_len) {
        *buffer_max_len > 0 ? (*buffer_max_len *= 2) : (*buffer_max_len = len);
        *buffer = realloc(*buffer, *buffer_max_len);
        assert(buffer != NULL);
    }

    memcpy(*buffer + *buffer_len, s, len);
    *buffer_len += len;
}

void append_ptr(void*** buffer, size_t* buffer_len, size_t* buffer_max_len, void* ptr) {
    if (*buffer_len + 1 >= *buffer_max_len) {
        *buffer_max_len > 0 ? (*buffer_max_len *= 2) : (*buffer_max_len = 1);
        *buffer = realloc(*buffer, *buffer_max_len);
        assert(buffer != NULL);
    }

    (*buffer)[*buffer_len] = ptr;
    *buffer_len += 1;
}

void shrink_ptr_vector(void*** buffer, const size_t* size, size_t* capacity) {
    *buffer = realloc(*buffer, *size * sizeof(void*));
    assert(buffer != NULL);
    *capacity = *size;
}
