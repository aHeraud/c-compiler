#ifndef C_COMPILER_VECTORS_H
#define C_COMPILER_VECTORS_H

#include <stddef.h>

typedef struct CharVector {
    char* buffer;
    size_t len;
    size_t max_len;
} char_vector_t;

typedef struct StringVector {
    char** buffer;
    size_t len;
    size_t max_len;
} string_vector_t;

typedef struct PtrVector {
    void** buffer;
    size_t len;
    size_t max_len;
} ptr_vector_t;

/**
 * Appends a character to a buffer, growing the buffer if necessary.
 * The pointer to the buffer may change if it is reallocated.
 * @param buffer buffer to append to
 * @param buffer_len number of elements in the buffer
 * @param buffer_max_len maximum length of the buffer
 * @param c character to append
 */
void append_char(char** buffer, size_t* buffer_len, size_t* buffer_max_len, char c);

/**
 * Appends a pointer to a buffer, growing the buffer if necessary.
 * @param buffer buffer to append to
 * @param buffer_len number of elements in the buffer
 * @param buffer_max_len maximum number of elements in the buffer
 * @param ptr pointer to append
 */
void append_ptr(void*** buffer, size_t* buffer_len, size_t* buffer_max_len, void* ptr);

#endif //C_COMPILER_VECTORS_H
