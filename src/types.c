#include <assert.h>
#include "types.h"



bool is_integer_type(const type_t *type) {
    return type->kind == TYPE_INTEGER;
}

bool is_small_integer_type(const type_t *type) {
    return is_integer_type(type) && INTEGER_TYPE_RANKS[type->integer.size] < INTEGER_TYPE_RANKS[INTEGER_TYPE_INT];
}

const type_t *type_after_integer_promotion(const type_t *type) {
    if (is_small_integer_type(type)) {
        return &INT;
    } else {
        return type;
    }
}

bool is_floating_type(const type_t *type) {
    return type->kind == TYPE_FLOATING;
}

bool is_long_double_type(const type_t *type) {
    return is_floating_type(type) && type->floating == FLOAT_TYPE_LONG_DOUBLE;
}

bool is_double_type(const type_t *type) {
    return is_floating_type(type) && type->floating == FLOAT_TYPE_DOUBLE;
}

bool is_arithmetic_type(const type_t *type) {
    return is_integer_type(type) || is_floating_type(type);
}

bool types_equal(const type_t *a, const type_t *b) {
    if (a == b) {
        return true;
    }

    if (a->kind != b->kind) {
        return false;
    }

    switch (a->kind) {
        case TYPE_VOID:
            return true;
        case TYPE_INTEGER:
            return a->integer.is_signed == b->integer.is_signed && a->integer.size == b->integer.size;
        case TYPE_FLOATING:
            return a->floating == b->floating;
    }
}

const type_t *get_common_type(const type_t *a, const type_t *b) {
    assert(a != NULL && b != NULL);

    if (is_floating_type(a) || is_floating_type(b)) {
        // If one or both operands are a floating point type, then the common type is the floating point type of the
        // highest rank out of the two operands (if both are floating point types), or if only one operand is a floating
        // point type, then the other operand is converted to a floating point type of the same rank.
        if (is_long_double_type(a) || is_long_double_type(b)) {
            return &LONG_DOUBLE;
        } else if (is_double_type(a) || is_double_type(b)) {
            return &DOUBLE;
        } else {
            return &FLOAT;
        }
    } else {
        // If neither of the operands is a floating point type, the integer promotions are performed on both operands.
        const type_t *promoted_a = type_after_integer_promotion(a);
        const type_t *promoted_b = type_after_integer_promotion(b);
        assert(is_integer_type(promoted_a) && is_integer_type(promoted_b));

        // If both operands have the same type, then no further conversion is needed.
        if (types_equal(promoted_a, promoted_b)) {
            return promoted_a;
        }

        // Otherwise, if the types of both operands have the same signedness, then the operand with the lesser rank is
        // converted to the type of the operand with the greater rank.
        if (promoted_a->integer.is_signed == promoted_b->integer.is_signed) {
            if (INTEGER_TYPE_RANKS[promoted_a->integer.size] < INTEGER_TYPE_RANKS[promoted_b->integer.size]) {
                return promoted_b;
            } else {
                return promoted_a;
            }
        }

        const type_t *signed_type = promoted_a->integer.is_signed ? promoted_a : promoted_b;
        const type_t *unsigned_type = signed_type == promoted_a ? promoted_b : promoted_a;

        // Otherwise, if the type of the unsigned operand has a rank greater than or equal to the rank of the signed
        // operand, then the signed operand is converted to the unsigned operand's type.
        if (INTEGER_TYPE_RANKS[unsigned_type->integer.size] >= INTEGER_TYPE_RANKS[signed_type->integer.size]) {
            return unsigned_type;
        }

        // Otherwise, if the type of the operand with signed integer type can represent all of the values of the type
        // of the operand with unsigned integer type, then the operand with unsigned integer type is converted to the
        // type of the operand with signed integer type.
        // TODO: What exactly does "can represent all of the values of" mean? What if the signed type is the same size
        //       as the unsigned type? Is it ok if converting the unsigned type to the signed type results in a negative
        //       value? For now we assume that it's ok.
        if (INTEGER_TYPE_RANKS[signed_type->integer.size] >= INTEGER_TYPE_RANKS[unsigned_type->integer.size]) {
            return signed_type;
        }

        // Otherwise, both operands are converted to the unsigned integer type corresponding to the type of the operand
        // with signed integer type.
        integer_type_t int_size = signed_type->integer.size;
        switch (int_size) {
            case INTEGER_TYPE_BOOL:
                return &BOOL;
            case INTEGER_TYPE_CHAR:
                return &UNSIGNED_CHAR;
            case INTEGER_TYPE_SHORT:
                return &UNSIGNED_SHORT;
            case INTEGER_TYPE_INT:
                return &UNSIGNED_INT;
            case INTEGER_TYPE_LONG:
                return &UNSIGNED_LONG;
            case INTEGER_TYPE_LONG_LONG:
                return &UNSIGNED_LONG_LONG;
        }
    }
}