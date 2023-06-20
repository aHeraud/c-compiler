#ifndef C_COMPILER_VECTORS_H
#define C_COMPILER_VECTORS_H

#include <stddef.h>

typedef struct CharVector {
    char* buffer;
    size_t size;
    size_t capacity;
} char_vector_t;

typedef struct StringVector {
    char** buffer;
    size_t size;
    size_t capacity;
} string_vector_t;

typedef struct PtrVector {
    void** buffer;
    size_t size;
    size_t capacity;
} ptr_vector_t;

/**
 * Appends a character to a buffer, growing the buffer if necessary.
 * The pointer to the buffer may change if it is reallocated.
 * @param buffer buffer to append to
 * @param size number of elements in the buffer
 * @param capacity maximum length of the buffer
 * @param c character to append
 */
void append_char(char** buffer, size_t* size, size_t* capacity, char c);

/**
 * Appends a string to a buffer, growing the buffer if necessary.
 * The pointer to the buffer may change if it is reallocated.n
 * @param buffer buffer to append to
 * @param size number of elements in the buffer
 * @param capacity maximum number of elements in the buffer
 * @param str characters to append
 */
void append_chars(char** buffer, size_t* size, size_t* capacity, const char* str);

/**
 * Shrinks a buffer to its current size.
 * @param buffer buffer to shrink
 * @param size number of elements in the buffer
 * @param capacity maximum number of elements in the buffer
 */
void shrink_char_vector(char** buffer, const size_t* size, size_t* capacity);

/**
 * Appends a pointer to a buffer, growing the buffer if necessary.
 * @param buffer buffer to append to
 * @param size number of elements in the buffer
 * @param capacity maximum number of elements in the buffer
 * @param ptr pointer to append
 */
void append_ptr(void*** buffer, size_t* size, size_t* capacity, void* ptr);

/**
 * Shrinks a buffer to its current size.
 * @param buffer buffer to shrink
 * @param size number of elements in the buffer
 * @param capacity maximum number of elements in the buffer
 */
void shrink_ptr_vector(void*** buffer, const size_t* size, size_t* capacity);

#endif //C_COMPILER_VECTORS_H
