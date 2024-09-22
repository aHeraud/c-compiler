#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>

#include "parser/lexer.h"
#include "types.h"

bool integer_value_fits_in_int_type(unsigned long long value, const type_t *type) {
    if (type->kind != TYPE_INTEGER) {
        return false;
    }

    // note: this is platform dependent
    // TODO: this should use the integer sizes for the target platform
    // For details on the sizes of integer constants, see section 6.4.4.1 of the C language specification:
    // https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf
    bool is_signed = type->value.integer.is_signed;
    switch (type->value.integer.size) {
        case INTEGER_TYPE_CHAR:
            return is_signed ? value <= CHAR_MAX : value <= UCHAR_MAX;
        case INTEGER_TYPE_SHORT:
            return is_signed ? value <= SHRT_MAX : value <= USHRT_MAX;
        case INTEGER_TYPE_INT:
            return is_signed ? value <= INT_MAX : value <= UINT_MAX;
        case INTEGER_TYPE_LONG:
            return is_signed ? value <= LONG_MAX : value <= ULONG_MAX;
        case INTEGER_TYPE_LONG_LONG:
            return is_signed ? value <= LLONG_MAX : value <= ULLONG_MAX;
        default:
            return false;
    }
}

// An integer constant has 3 parts:
// 1. an optional hexadecimal prefix ("0x" or "0X")
// 2. a sequence of 1 or more digits in the specified base (decimal, hex, or octal)
// 3. an optional integer suffix, which consists of (in any order)
//    1. an optional unsigned suffix ('u' or 'U')
//    2. an optional size suffix ('l', 'L', 'll', or 'LL')
// Semantics
// 1. The type of an integer constant is the first in the following table which is large enough to represent its value
//    | Suffix |    Decimal Constant    |   Octal/Hex Constant   |
//    |========|========================|========================|
//    | none   | int                    | int                    |
//    |        | long int               | unsigned int           |
//    |        | long long int          | long int               |
//    |        |                        | unsigned long int      |
//    |        |                        | long long int          |
//    |        |                        | unsigned long long int |
//    |--------|------------------------|------------------------|
//    | u/U    | unsigned int           | unsigned int           |
//    |        | unsigned long int      | unsigned long int      |
//    |        | unsigned long long int | unsigned long long int |
//    |--------|------------------------|------------------------|
//    | l/L    | long int               | long int               |
//    |        | long long int          | long long int          |
//    |        |                        | unsigned long int      |
//    |        |                        | unsigned long long int |
//    |--------|------------------------|------------------------|
//    | u/U &  | unsigned long int      | unsigned long int      |
//    | l/L    | unsigned long long int | unsigned long long int |
//    |--------|------------------------|------------------------|
//    | ll/LL  | long long int          | long long int          |
//    |        |                        | unsigned long long int |
//    |--------|------------------------|------------------------|
//    | u/U &  | unsigned long long int | unsigned long long int |
//    | ll/LL  |                        |                        |
//    |--------|------------------------|------------------------|
// 2. If the value of an integer constant cannot be represented by its type it may have an extended integer type
//    (implementation specific, e.g. __int128). If the value of an integer constant is outside the range of
//    representable values, then the integer constant has no type.
void decode_integer_constant(const token_t *token, unsigned long long *value, const type_t **type) {
    const char *raw = token->value;
    char *suffix = NULL;
    *value = strtoull(raw, &suffix, 0);
    if (errno == ERANGE) {
        // TODO: error if the value can not be represented by an integer type
        fprintf(stderr, "%s:%d:%d: warn: integer constant out of range\n",
                token->position.path, token->position.line, token->position.column);
    }

    // Default to int
    *type = &INT;

    bool is_decimal = raw[0] != '0';
    bool is_unsigned = false;
    bool is_long = false;
    bool is_long_long = false;
    if (suffix != NULL && (strstr(suffix, "u") != NULL || strstr(suffix, "U") != NULL)) {
        is_unsigned = true;
    }
    if (suffix != NULL && (strstr(suffix, "l") != NULL || strstr(suffix, "L") != NULL)) {
        is_long = true;
    }
    if (suffix != NULL && (strstr(suffix, "ll") != NULL || strstr(suffix, "LL") != NULL)) {
        is_long_long = true;
    }

    int table_size = 0;
    const type_t *table[6];
    if (is_decimal) {
        if (!is_unsigned) {
            if (!is_long && !is_long_long) {
                table[table_size++] = &INT;
            }
            if (!is_long_long) {
                table[table_size++] = &LONG;
            }
            table[table_size++] = &LONG_LONG;
        } else {
            if (!is_long && !is_long_long) {
                table[table_size++] = &UNSIGNED_INT;
            }
            if (!is_long_long) {
                table[table_size++] = &UNSIGNED_LONG;
            }
            table[table_size++] = &UNSIGNED_LONG_LONG;
        }
    } else {
        if (!is_long && !is_long_long) {
            if (!is_unsigned) {
                table[table_size++] = &INT;
            }
            table[table_size++] = &UNSIGNED_INT;
        }
        if (!is_long_long) {
            if (!is_unsigned) {
                table[table_size++] = &LONG;
            }
            table[table_size++] = &UNSIGNED_LONG;
        }
        if (!is_unsigned) {
            table[table_size++] = &LONG_LONG;
        }
        table[table_size++] = &UNSIGNED_LONG_LONG;
    }

    bool fits = false;
    for (int i = 0; i < table_size; i++) {
        if (integer_value_fits_in_int_type(*value, table[i])) {
            *type = table[i];
            fits = true;
            break;
        }
    }

    if (!fits) {
        // TODO: error if the value can not be represented by an integer type
        fprintf(stderr, "%s:%d:%d: warn: integer constant out of range\n",
                token->position.path, token->position.line, token->position.column);
    }
}

// A floating constant has 4 parts:
// 1. an optional hex prefix ("0x" or "0X")
// 2. a fractional constant
// 3. an optional exponent part
// 4. an optional floating suffix ('f', 'F', 'l', or 'L')
// Semantics
// 1. The significand part is a decimal or hexadecimal rational number.
// 2. For decimal floating constants, the exponent represents a power of 10, for hex a power of 2.
// 3. An un-suffixed floating constant has type double. If suffixed by the letter 'f' or 'F', it has type float. If
//    suffixed by the letter 'l' or 'L' it has type long double.
void decode_float_constant(const token_t *token, long double *value, const type_t **type) {
    char *suffix = NULL;
    *value = strtold(token->value, &suffix);

    if (suffix != NULL) {
        if (strstr(suffix, "f") != NULL || strstr(suffix, "F") != NULL) {
            *value = (float) *value;
            *type = &FLOAT;
        } else if (strstr(suffix, "l") != NULL || strstr(suffix, "L") != NULL) {
            *type = &LONG_DOUBLE;
        } else {
            *value = (double) *value;
            *type = &DOUBLE;
        }
    } else {
        // Default to double
        *value = (double) *value;
        *type = &DOUBLE;
    }
}
