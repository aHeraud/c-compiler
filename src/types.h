#ifndef C_COMPILER_TYPES_H
#define C_COMPILER_TYPES_H

#include <stdbool.h>
#include <stddef.h>

typedef enum TypeKind {
    TYPE_VOID,
    TYPE_INTEGER,
    TYPE_FLOATING,
    TYPE_POINTER,
} type_kind_t;


typedef enum IntegerType {
    INTEGER_TYPE_BOOL,
    INTEGER_TYPE_CHAR,
    INTEGER_TYPE_SHORT,
    INTEGER_TYPE_INT,
    INTEGER_TYPE_LONG,
    INTEGER_TYPE_LONG_LONG,
} integer_type_t;

// The rank of each integer type (for integer promotion) as defined in 6.3.1.1 of the C standard:
// https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf
static const int INTEGER_TYPE_RANKS[] = {
        [INTEGER_TYPE_BOOL] = -3,
        [INTEGER_TYPE_CHAR] = -2,
        [INTEGER_TYPE_SHORT] = -1,
        [INTEGER_TYPE_INT] = 0,
        [INTEGER_TYPE_LONG] = 1,
        [INTEGER_TYPE_LONG_LONG] = 2,
};

typedef enum FloatType {
    FLOAT_TYPE_FLOAT,
    FLOAT_TYPE_DOUBLE,
    FLOAT_TYPE_LONG_DOUBLE,
} float_type_t;

static const int FLOAT_TYPE_RANKS[] = {
        [FLOAT_TYPE_FLOAT] = 0,
        [FLOAT_TYPE_DOUBLE] = 1,
        [FLOAT_TYPE_LONG_DOUBLE] = 2,
};

typedef enum StorageClass {
    STORAGE_CLASS_AUTO,
    STORAGE_CLASS_EXTERN,
    STORAGE_CLASS_TYPEDEF,
    STORAGE_CLASS_REGISTER,
    STORAGE_CLASS_STATIC,
} storage_class_t;

/**
 * Represents a C type.
 */
typedef struct Type type_t;
typedef struct Type {
    type_kind_t kind;
    storage_class_t storage_class;
    bool is_const;
    bool is_volatile;
    union {
        struct {
            bool is_signed;
            integer_type_t size;
        } integer;
        float_type_t floating;
        struct {
            const type_t *base;
            bool is_const;
            bool is_volatile;
            bool is_restrict;
        } pointer;
    };
} type_t;

bool is_integer_type(const type_t *type);
bool is_small_integer_type(const type_t *type);
bool is_floating_type(const type_t *type);
bool is_long_double_type(const type_t *type);
bool is_double_type(const type_t *type);
bool is_arithmetic_type(const type_t *type);
bool is_scalar_type(const type_t *type);
bool is_pointer_type(const type_t *type);

/**
 * Compare two C types for equality.
 * @param a first type
 * @param b second type
 * @return true if the two types are equal
 */
bool types_equal(const type_t *a, const type_t *b);

/**
 * Get the type that results from integer promotion of the given type.
 * Integer promotion is only applied to integer types of rank less than that of int. For other types, the same type is
 * returned.
 *
 * See section 6.3.1.1 (Boolean, characters, and integers) of the C standard for further details.
 * @param type the type to promote
 * @return the promoted type
 */
const type_t *type_after_integer_promotion(const type_t *type);

/**
 * Determine the common type of two types for use in binary (arithmetic and simple assignment) operations.
 *
 * See section 6.3.1.8 (Usual arithmetic conversions) of the C standard for further details.
 *
 * @param a
 * @param b
 * @return the common type of a and b
 */
const type_t *get_common_type(const type_t *a, const type_t *b);

// Static type objects for common types

static const type_t VOID = {
        .kind = TYPE_VOID,
};

static const type_t BOOL = {
        .kind = TYPE_INTEGER,
        .integer = {
                .is_signed = false,
                .size = INTEGER_TYPE_BOOL,
        },
};

static const type_t CHAR = {
        .kind = TYPE_INTEGER,
        .integer = {
                .is_signed = true,
                .size = INTEGER_TYPE_CHAR,
        },
};

static const type_t SHORT = {
        .kind = TYPE_INTEGER,
        .integer = {
                .is_signed = true,
                .size = INTEGER_TYPE_SHORT,
        },
};

static const type_t INT = {
        .kind = TYPE_INTEGER,
        .integer = {
                .is_signed = true,
                .size = INTEGER_TYPE_INT,
        },
};

static const type_t LONG = {
        .kind = TYPE_INTEGER,
        .integer = {
                .is_signed = false,
                .size = INTEGER_TYPE_LONG,
        },
};

static const type_t LONG_LONG = {
        .kind = TYPE_INTEGER,
        .integer = {
                .is_signed = false,
                .size = INTEGER_TYPE_INT,
        },
};

static const type_t UNSIGNED_CHAR = {
        .kind = TYPE_INTEGER,
        .integer = {
                .is_signed = false,
                .size = INTEGER_TYPE_CHAR,
        },
};

static const type_t UNSIGNED_SHORT = {
        .kind = TYPE_INTEGER,
        .integer = {
                .is_signed = false,
                .size = INTEGER_TYPE_SHORT,
        },
};

static const type_t UNSIGNED_INT = {
        .kind = TYPE_INTEGER,
        .integer = {
                .is_signed = false,
                .size = INTEGER_TYPE_INT,
        },
};

static const type_t UNSIGNED_LONG = {
        .kind = TYPE_INTEGER,
        .integer = {
                .is_signed = false,
                .size = INTEGER_TYPE_LONG,
        },
};

static const type_t UNSIGNED_LONG_LONG = {
        .kind = TYPE_INTEGER,
        .integer = {
                .is_signed = false,
                .size = INTEGER_TYPE_LONG_LONG,
        },
};

static const type_t FLOAT = {
        .kind = TYPE_FLOATING,
        .floating = FLOAT_TYPE_FLOAT,
};

static const type_t DOUBLE = {
        .kind = TYPE_FLOATING,
        .floating = FLOAT_TYPE_DOUBLE,
};

static const type_t LONG_DOUBLE = {
        .kind = TYPE_FLOATING,
        .floating = FLOAT_TYPE_LONG_DOUBLE,
};

#endif //C_COMPILER_TYPES_H
