#include <assert.h>
#include <malloc.h>
#include <stddef.h>

#include "vectors.h"

void append_char(char** buffer, size_t* buffer_len, size_t* buffer_max_len, char c) {
    if (*buffer_len + 1 >= *buffer_max_len) {
        *buffer_max_len *= 2;
        *buffer = realloc(*buffer, *buffer_max_len);
        assert(buffer != NULL);
    }

    (*buffer)[*buffer_len] = c;
    *buffer_len += 1;
}

void append_ptr(void*** buffer, size_t* buffer_len, size_t* buffer_max_len, void* ptr) {
    if (*buffer_len + 1 >= *buffer_max_len) {
        *buffer_max_len *= 2;
        *buffer = realloc(*buffer, *buffer_max_len);
        assert(buffer != NULL);
    }

    (*buffer)[*buffer_len] = ptr;
    *buffer_len += 1;
}
