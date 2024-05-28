#ifndef C_COMPILER_VECTORS_H
#define C_COMPILER_VECTORS_H

#include <assert.h>
#include <stdlib.h>
#include <stddef.h>

#define VEC_DEFINE(name, typedef_name, type) \
typedef struct name { \
    type* buffer; \
    size_t size; \
    size_t capacity; \
} typedef_name;

VEC_DEFINE(CharVector, char_vector_t, char)
VEC_DEFINE(StringVector, string_vector_t, char*)
VEC_DEFINE(PtrVector, ptr_vector_t, void*)

#define VEC_INIT { .buffer = NULL, .size = 0, .capacity = 0 }

// Append a single element to a vector, growing the vector if necessary.
#define VEC_APPEND(vec, elem) \
do { \
    if ((vec)->size + 1 >= (vec)->capacity) { \
        (vec)->capacity > 0 ? ((vec)->capacity *= 2) : ((vec)->capacity = 1); \
        (vec)->buffer = realloc((vec)->buffer, (vec)->capacity * sizeof(elem)); \
        assert((vec)->buffer != NULL); \
    } \
    ((vec)->buffer)[(vec)->size++] = elem; \
} while (0)

#define VEC_FIND(vec, elem) \
({ \
    size_t i = -1; \
    for (; i < (vec)->size; i++) { \
        if ((vec)->buffer[i] == (elem)) { \
            break; \
        } \
    } \
    i; \
})

#define VEC_SHRINK(vec, type) \
do { \
    (vec)->buffer = realloc((vec)->buffer, (vec)->size * sizeof(type)); \
    (vec)->capacity = (vec)->size; \
    assert(((vec)->buffer) != NULL); \
} while (0)

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
 * Removes the last pointer from a buffer.
 * @param buffer buffer to pop from
 * @param size number of elements in the buffer
 * @param capacity maximum number of elements in the buffer
 */
void* pop_ptr(void** buffer, size_t* buffer_len);

/**
 * Shrinks a buffer to its current size.
 * @param buffer buffer to shrink
 * @param size number of elements in the buffer
 * @param capacity maximum number of elements in the buffer
 */
void shrink_ptr_vector(void*** buffer, const size_t* size, size_t* capacity);

/**
 * Reverses the order of elements in a buffer.
 * @param buffer buffer to reverse
 * @param size number of elements in the buffer
 */
void reverse_ptr_vector(void** buffer, size_t size);

#endif //C_COMPILER_VECTORS_H
