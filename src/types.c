#include <assert.h>
#include <string.h>
#include "types.h"
#include "ast.h"


bool is_integer_type(const type_t *type) {
    return type->kind == TYPE_INTEGER;
}

bool is_small_integer_type(const type_t *type) {
    return is_integer_type(type) && INTEGER_TYPE_RANKS[type->value.integer.size] < INTEGER_TYPE_RANKS[INTEGER_TYPE_INT];
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
    return is_floating_type(type) && type->value.floating == FLOAT_TYPE_LONG_DOUBLE;
}

bool is_double_type(const type_t *type) {
    return is_floating_type(type) && type->value.floating == FLOAT_TYPE_DOUBLE;
}

bool is_arithmetic_type(const type_t *type) {
    return is_integer_type(type) || is_floating_type(type);
}

bool is_pointer_type(const type_t *type) {
    return type->kind == TYPE_POINTER;
}

bool is_scalar_type(const type_t *type) {
    return is_arithmetic_type(type) || is_pointer_type(type);
}

bool parameter_declaration_eq(const parameter_declaration_t *left, const parameter_declaration_t *right);

bool types_equal(const type_t *a, const type_t *b) {
    if (a == b) {
        return true;
    }

    if (a == NULL || b == NULL) {
        return false;
    }

    if (a->kind != b->kind) {
        return false;
    }

    switch (a->kind) {
        case TYPE_VOID:
            return true;
        case TYPE_INTEGER:
            return a->value.integer.is_signed == b->value.integer.is_signed && a->value.integer.size == b->value.integer.size;
        case TYPE_FLOATING:
            return a->value.floating == b->value.floating;
        case TYPE_POINTER:
            return types_equal(a->value.pointer.base, b->value.pointer.base);
        case TYPE_ARRAY:
            return types_equal(a->value.array.element_type, b->value.array.element_type) &&
                   expression_eq(a->value.array.size, b->value.array.size);
        case TYPE_FUNCTION:
            if (!types_equal(a->value.function.return_type, b->value.function.return_type)) {
                return false;
            }
            if (a->value.function.parameter_list->length != b->value.function.parameter_list->length) {
                return false;
            }
            if (a->value.function.parameter_list->variadic != b->value.function.parameter_list->variadic) {
                return false;
            }
            for (size_t i = 0; i < a->value.function.parameter_list->length; i++) {
                if (!parameter_declaration_eq(a->value.function.parameter_list->parameters[i],
                                             b->value.function.parameter_list->parameters[i])) {
                    return false;
                }
            }
            return true;
        default: // unknown type
            return false;
    }
}

bool parameter_declaration_eq(const parameter_declaration_t *left, const parameter_declaration_t *right) {
    if (left == NULL || right == NULL) {
        return left == right;
    }

    if (!types_equal(left->type, right->type)) {
        return false;
    }

    if (left->identifier == NULL || right->identifier == NULL) {
        return left->identifier == right->identifier;
    }

    return strcmp(left->identifier->value, right->identifier->value) == 0;
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
        if (promoted_a->value.integer.is_signed == promoted_b->value.integer.is_signed) {
            if (INTEGER_TYPE_RANKS[promoted_a->value.integer.size] < INTEGER_TYPE_RANKS[promoted_b->value.integer.size]) {
                return promoted_b;
            } else {
                return promoted_a;
            }
        }

        const type_t *signed_type = promoted_a->value.integer.is_signed ? promoted_a : promoted_b;
        const type_t *unsigned_type = signed_type == promoted_a ? promoted_b : promoted_a;

        // Otherwise, if the type of the unsigned operand has a rank greater than or equal to the rank of the signed
        // operand, then the signed operand is converted to the unsigned operand's type.
        if (INTEGER_TYPE_RANKS[unsigned_type->value.integer.size] >= INTEGER_TYPE_RANKS[signed_type->value.integer.size]) {
            return unsigned_type;
        }

        // Otherwise, if the type of the operand with signed integer type can represent all of the values of the type
        // of the operand with unsigned integer type, then the operand with unsigned integer type is converted to the
        // type of the operand with signed integer type.
        // TODO: What exactly does "can represent all of the values of" mean? What if the signed type is the same size
        //       as the unsigned type? Is it ok if converting the unsigned type to the signed type results in a negative
        //       value? For now we assume that it's ok.
        if (INTEGER_TYPE_RANKS[signed_type->value.integer.size] >= INTEGER_TYPE_RANKS[unsigned_type->value.integer.size]) {
            return signed_type;
        }

        // Otherwise, both operands are converted to the unsigned integer type corresponding to the type of the operand
        // with signed integer type.
        integer_type_t int_size = signed_type->value.integer.size;
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

const type_t *get_ptr_type(const type_t *inner) {
    type_t *ptr = malloc(sizeof(type_t));
    *ptr = (type_t) {
        .kind = TYPE_POINTER,
        .is_volatile = false,
        .is_const = false,
        .storage_class = STORAGE_CLASS_AUTO,
        .value.pointer = {
            .is_const = false,
            .is_restrict = false,
            .is_volatile = false,
            .base = inner,
        },
    };
    return ptr;
}
