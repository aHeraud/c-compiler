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

// The rank of each integer type (for integer promotion) as defined in 6.3.1.1 of the C standard:
// https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf
static const int INTEGER_TYPE_RANKS[] = {
        [INTEGER_TYPE_CHAR] = -2,
        [INTEGER_TYPE_SHORT] = -1,
        [INTEGER_TYPE_INT] = 0,
        [INTEGER_TYPE_LONG] = 1,
        [INTEGER_TYPE_LONG_LONG] = 2,
};

typedef struct Type {
    type_kind_t kind;
    union {
        struct {
            bool is_signed;
            integer_type_t size;
        } integer;
    };
} type_t;

static const type_t INT = {
        .kind = TYPE_INTEGER,
        .integer = {
                .is_signed = true,
                .size = INTEGER_TYPE_INT,
        },
};


#endif //C_COMPILER_TYPES_H
