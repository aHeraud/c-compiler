#ifndef C_COMPILER_TYPES_H
#define C_COMPILER_TYPES_H

#include <stdbool.h>
#include <stddef.h>

typedef enum TypeKind {
    TYPE_VOID,
    TYPE_INTEGER,
} type_kind_t;

typedef enum IntegerType {
    INTEGER_TYPE_CHAR,
    INTEGER_TYPE_SHORT,
    INTEGER_TYPE_INT,
    INTEGER_TYPE_LONG,
    INTEGER_TYPE_LONG_LONG,
} integer_type_t;

typedef struct Type {
    type_kind_t kind;
    union {
        struct {
            bool is_signed;
            integer_type_t size;
        } integer;
    };
} type_t;

#endif //C_COMPILER_TYPES_H
