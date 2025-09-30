#include <string.h>
#include "errors.h"
#include "../../util/strings.h"
#include "ir/codegen/internal.h"
#include "parser/numeric-constants.h"

const expression_result_t EXPR_ERR = {
    .kind = EXPR_RESULT_ERR,
    .c_type = NULL,
    .is_lvalue = false,
    .is_string_literal = false,
    .addr_of = false,
    .value = {
        .kind = IR_VALUE_VAR,
        .var = {
            .name = NULL,
            .type = NULL,
        },
    },
};

expression_result_t ir_visit_constant_expression(ir_gen_context_t *context, const expression_t *expression) {
    // Create a dummy fake instruction context and a function builder (visit_expression will attempt to generate
    // instructions if this expression isn't actually a compile time constant).
    ir_function_definition_t *cur_fn = context->function;
    ir_function_builder_t *cur_builder = context->builder;
    context->function = & (ir_function_definition_t) {
        .name = "__gen_constexpr",
        .type = NULL,
        .num_params = 0,
        .params = NULL,
        .is_variadic = false,
        .body = NULL,
    };
    context->builder = ir_builder_create();

    expression_result_t result = ir_visit_expression(context, expression);
    if (result.kind != EXPR_RESULT_VALUE) result = EXPR_ERR;

    if (result.kind == EXPR_RESULT_VALUE && result.value.kind != IR_VALUE_CONST) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_EXPECTED_CONSTANT_EXPRESSION,
            .location = expression->span.start,
        });
        result = EXPR_ERR;
    }

    // Delete the builder, throw away any generated instructions, and restore whatever the previous values were
    ir_builder_destroy(context->builder);
    context->function = NULL;

    context->function = cur_fn;
    context->builder = cur_builder;

    return result;
}

expression_result_t ir_visit_expression(ir_gen_context_t *context, const expression_t *expression) {
    assert(context != NULL && "Context must not be NULL");
    assert(expression != NULL && "Expression must not be NULL");

    switch (expression->kind) {
        case EXPRESSION_ARRAY_SUBSCRIPT:
            return ir_visit_array_subscript_expression(context, expression);
        case EXPRESSION_BINARY:
            return ir_visit_binary_expression(context, expression);
        case EXPRESSION_CALL:
            return ir_visit_call_expression(context, expression);
        case EXPRESSION_CAST:
            return ir_visit_cast_expression(context, expression);
        case EXPRESSION_MEMBER_ACCESS:
            return ir_visit_member_access_expression(context, expression);
        case EXPRESSION_PRIMARY:
            return ir_visit_primary_expression(context, expression);
        case EXPRESSION_SIZEOF:
            return ir_visit_sizeof_expression(context, expression);
        case EXPRESSION_TERNARY:
            return ir_visit_ternary_expression(context, expression);
        case EXPRESSION_UNARY:
            return ir_visit_unary_expression(context, expression);
        case EXPRESSION_COMPOUND_LITERAL:
            return ir_visit_compound_literal(context, expression);
    }

    // TODO
    return EXPR_ERR;
}

expression_result_t ir_visit_array_subscript_expression(ir_gen_context_t *context, const expression_t *expr) {
    expression_result_t target = ir_visit_expression(context, expr->value.array_subscript.array);
    if (target.kind == EXPR_RESULT_ERR) return EXPR_ERR;

    // The target must be an array or a pointer
    if (target.c_type->kind != TYPE_ARRAY && target.c_type->kind != TYPE_POINTER) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_SUBSCRIPT_TARGET,
            .location = expr->value.array_subscript.array->span.start, // TODO, use the '[' token position?
        });
        return EXPR_ERR;
    }

    // If the target is a pointer, we need to dereference it to get the base pointer
    if (target.c_type->kind == TYPE_POINTER) {
        target = get_rvalue(context, target);
    }

    ir_value_t base_ptr;
    if (target.kind == EXPR_RESULT_VALUE) {
        assert(ir_get_type_of_value(target.value)->kind == IR_TYPE_PTR);
        base_ptr = target.value;
    } else {
        base_ptr = get_indirect_ptr(context, target);
    }

    const ir_type_t *ptr_type = ir_get_type_of_value(base_ptr);
    const ir_type_t *element_type = ptr_type->value.ptr.pointee->kind == IR_TYPE_ARRAY
        ? ptr_type->value.ptr.pointee->value.array.element
        : ptr_type->value.ptr.pointee;

    expression_result_t index = ir_visit_expression(context, expr->value.array_subscript.index);
    if (index.kind == EXPR_RESULT_ERR) return EXPR_ERR;
    if (index.is_lvalue) index = get_rvalue(context, index);
    assert(index.kind == EXPR_RESULT_VALUE);

    // The subscript must have an integer type
    if (!is_integer_type(index.c_type)) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_SUBSCRIPT_TYPE,
            .location = expr->value.array_subscript.index->span.start,
        });
        return EXPR_ERR;
    }

    ir_var_t result = temp_var(context, get_ir_ptr_type(element_type));
    ir_build_get_array_element_ptr(context->builder, base_ptr, index.value, result);

    const type_t *result_type = target.c_type->kind == TYPE_ARRAY
        ? target.c_type->value.array.element_type
        : target.c_type->value.pointer.base;
    return (expression_result_t) {
        .kind = EXPR_RESULT_VALUE,
        .c_type = result_type,
        .is_lvalue = true,
        .is_string_literal = false,
        .addr_of = false,
        .value = ir_value_for_var(result),
    };
}

expression_result_t ir_visit_call_expression(ir_gen_context_t *context, const expression_t *expr) {
    expression_result_t function = ir_visit_expression(context, expr->value.call.callee);
    ptr_vector_t arguments = (ptr_vector_t) {
        .size = 0,
        .capacity = 0,
        .buffer = NULL,
    };

    if (function.kind == EXPR_RESULT_ERR) return EXPR_ERR;

    // Function can be a function, or a pointer to a function
    // TODO: handle function pointers
    if (function.c_type->kind != TYPE_FUNCTION) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_CALL_TARGET_NOT_FUNCTION,
            .location = expr->value.call.callee->span.start,
            .value.call_target_not_function = {
                .type = function.c_type,
            }
        });
        return EXPR_ERR;
    }

    // Check that the number of arguments matches function arity
    size_t expected_args_count = function.c_type->value.function.parameter_list->length;
    bool variadic = function.c_type->value.function.parameter_list->variadic;
    size_t actual_args_count = expr->value.call.arguments.size;
    if ((variadic && actual_args_count < expected_args_count) || (!variadic && actual_args_count != expected_args_count)) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_CALL_ARGUMENT_COUNT_MISMATCH,
            .location = expr->value.call.callee->span.start,
            .value.call_argument_count_mismatch = {
                .expected = expected_args_count,
                .actual = actual_args_count,
            }
        });
        return EXPR_ERR;
    }

    // Evaluate the arguments
    ir_value_t *args = malloc(sizeof(ir_value_t) * actual_args_count);
    for (size_t i = 0; i < actual_args_count; i += 1) {
        expression_result_t arg = ir_visit_expression(context, expr->value.call.arguments.buffer[i]);

        // Error occurred while evaluating the argument
        if (arg.kind == EXPR_RESULT_ERR) return EXPR_ERR;

        if (arg.c_type->kind == TYPE_ARRAY) {
            arg = convert_to_type(context, arg.value, get_ptr_type(arg.c_type), get_ptr_type(arg.c_type->value.array.element_type));
        } else if (arg.is_lvalue) {
            arg = get_rvalue(context, arg);
        }

        // Implicit conversion to the parameter type
        // Variadic arguments are _NOT_ converted to a specific type, but chars, shorts, and floats are promoted
        // Array arguments are passed as pointers
        if (i < function.c_type->value.function.parameter_list->length) {
            const type_t *param_type = function.c_type->value.function.parameter_list->parameters[i]->type;
            if (param_type->kind == TYPE_ARRAY) param_type = get_ptr_type(param_type->value.array.element_type);
            arg = convert_to_type(context, arg.value, arg.c_type, param_type);
        } else {
            if (arg.c_type->kind == TYPE_INTEGER) {
                const type_t *new_type = type_after_integer_promotion(arg.c_type);
                arg = convert_to_type(context, arg.value, arg.c_type, new_type);
            } else if (arg.c_type->kind == TYPE_FLOATING && arg.c_type->value.floating == FLOAT_TYPE_FLOAT) {
                arg = convert_to_type(context, arg.value, arg.c_type, &DOUBLE);
            }
        }

        // Conversion was invalid
        if (arg.kind == EXPR_RESULT_ERR) return EXPR_ERR;

        args[i] = arg.value;
    }

    // Emit the call instruction
    ir_var_t *result = NULL;
    if (function.c_type->value.function.return_type->kind != TYPE_VOID) {
        result = (ir_var_t*) malloc(sizeof(ir_var_t));
        *result = temp_var(context, get_ir_type(context,function.c_type->value.function.return_type));
    }
    ir_build_call(context->builder, function.value, args, actual_args_count, result);

    ir_value_t result_value = result != NULL ?
        ir_value_for_var(*result) : (ir_value_t) {
        .kind = IR_VALUE_CONST,
        .constant = (ir_const_t) {
            .kind = IR_CONST_INT,
            .type = &IR_VOID,
        },
    };

    return (expression_result_t) {
        .kind = EXPR_RESULT_VALUE,
        .c_type = function.c_type->value.function.return_type,
        .is_lvalue = false,
        .is_string_literal = false,
        .addr_of = false,
        .value = result_value,
    };
}

expression_result_t ir_visit_cast_expression(ir_gen_context_t *context, const expression_t *expr) {
    assert(expr != NULL && expr->kind == EXPRESSION_CAST);

    expression_result_t value = ir_visit_expression(context, expr->value.cast.expression);
    if (value.kind == EXPR_RESULT_ERR) return EXPR_ERR;
    if (value.is_lvalue) value = get_rvalue(context, value);
    return convert_to_type(context, value.value, value.c_type, expr->value.cast.type);
}

expression_result_t ir_visit_binary_expression(ir_gen_context_t *context, const expression_t *expr) {
    assert(context != NULL && "Context must not be NULL");
    assert(expr != NULL && "Expression must not be NULL");
    assert(expr->kind == EXPRESSION_BINARY);

    switch (expr->value.binary.kind) {
        case BINARY_ARITHMETIC: {
            expression_result_t lhs = ir_visit_expression(context, expr->value.binary.left);
            expression_result_t rhs = ir_visit_expression(context, expr->value.binary.right);
            if (expr->value.binary.operator.arithmetic == BINARY_ARITHMETIC_ADD ||
                expr->value.binary.operator.arithmetic == BINARY_ARITHMETIC_SUBTRACT) {
                return ir_visit_additive_binexpr(context, expr, lhs, rhs);
            } else {
                return ir_visit_multiplicative_binexpr(context, expr, lhs, rhs);
            }
        }
        case BINARY_ASSIGNMENT: {
            return ir_visit_assignment_binexpr(context, expr);
        }
        case BINARY_BITWISE: {
            expression_result_t lhs = ir_visit_expression(context, expr->value.binary.left);
            expression_result_t rhs = ir_visit_expression(context, expr->value.binary.right);
            return ir_visit_bitwise_binexpr(context, expr, lhs, rhs);
        }
        case BINARY_COMMA: {
            // TODO
            source_position_t pos = expr->value.binary.operator_token->position;
            fprintf(stderr, "%s:%d:%d: comma operator not yet implemented\n",
                pos.path, pos.line, pos.column);
            exit(1);
        }
        case BINARY_COMPARISON: {
            return ir_visit_comparison_binexpr(context, expr);
        }
        case BINARY_LOGICAL: {
            return ir_visit_logical_expression(context, expr);
        }
    }
}

expression_result_t ir_visit_additive_binexpr(ir_gen_context_t *context, const expression_t *expr, expression_result_t left, expression_result_t right) {
    const type_t *result_type;
    const ir_type_t *ir_result_type;

    // Bubble up errors if the operands are invalid.
    if (left.kind == EXPR_RESULT_ERR || right.kind == EXPR_RESULT_ERR) return EXPR_ERR;

    bool is_addition = expr->value.binary.operator_token->kind == TK_PLUS
                     || expr->value.binary.operator_token->kind == TK_PLUS_ASSIGN;

    if (left.is_lvalue) left = get_rvalue(context, left);
    if (right.is_lvalue) right = get_rvalue(context, right);

    // Both operands must have arithmetic type, or one operand must be a pointer and the other an integer.
    if (is_arithmetic_type(left.c_type) && is_arithmetic_type(right.c_type)) {
        // Integer/Float + Integer/Float
        result_type = get_common_type(left.c_type, right.c_type);
        ir_result_type = get_ir_type(context,result_type);

        left = convert_to_type(context, left.value, left.c_type, result_type);
        right = convert_to_type(context, right.value, right.c_type, result_type);

        ir_value_t result;
        if (left.value.kind == IR_VALUE_CONST && right.value.kind == IR_VALUE_CONST) {
            // constant folding
            result = (ir_value_t) {
                .kind = IR_VALUE_CONST,
                .constant = (ir_const_t) {
                    .kind = is_floating_type(result_type) ? IR_CONST_FLOAT : IR_CONST_INT,
                    .type = ir_result_type,
                    .value.i = 0,
                }
            };

            if (is_floating_type(result_type)) {
                result.constant.value.f = is_addition ? left.value.constant.value.f + right.value.constant.value.f
                                                      : left.value.constant.value.f - right.value.constant.value.f;
            } else {
                result.constant.value.i = is_addition ? left.value.constant.value.i + right.value.constant.value.i
                                                      : left.value.constant.value.i - right.value.constant.value.i;
            }
        } else {
            // Generate a temp var to store the result
            ir_var_t temp = temp_var(context, ir_result_type);
            is_addition ? ir_build_add(context->builder, left.value, right.value, temp)
                        : ir_build_sub(context->builder, left.value, right.value, temp);
            result = ir_value_for_var(temp);
        }

        return (expression_result_t) {
            .kind = EXPR_RESULT_VALUE,
            .c_type = result_type,
            .is_lvalue = false,
            .is_string_literal = false,
            .value = result,
        };
    } else if ((is_pointer_type(left.c_type) && is_integer_type(right.c_type)) ||
               (is_integer_type(left.c_type) && is_pointer_type(right.c_type))) {
        // Pointer +/ integer.
        expression_result_t pointer_operand = is_pointer_type(left.c_type) ? left : right;
        expression_result_t integer_operand = is_pointer_type(left.c_type) ? right : left;

        if (!is_addition && is_pointer_type(right.c_type)) {
            // For subtraction the lhs must be the pointer
            append_compilation_error(&context->errors, (compilation_error_t) {
                .kind = ERR_INVALID_BINARY_EXPRESSION_OPERANDS,
                .location = expr->value.binary.operator_token->position,
                .value.invalid_binary_expression_operands = {
                    .operator = expr->value.binary.operator_token->value,
                    .left_type = left.c_type,
                    .right_type = right.c_type,
                },
            });
            return EXPR_ERR;
        }

        assert(pointer_operand.kind == EXPR_RESULT_VALUE); // todo
        assert(pointer_operand.kind == EXPR_RESULT_VALUE);
        ir_var_t result = temp_var(context, ir_get_type_of_value(pointer_operand.value));
        ir_build_get_array_element_ptr(context->builder, pointer_operand.value, integer_operand.value, result);

        // The result type is the same as the pointer type.
        result_type = pointer_operand.c_type;

        return (expression_result_t) {
            .kind = EXPR_RESULT_VALUE,
            .c_type = result_type,
            .is_lvalue = false,
            .is_string_literal = false,
            .addr_of = false,
            .value = ir_value_for_var(result),
        };
    } else {
        // Invalid operand types.
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_BINARY_EXPRESSION_OPERANDS,
            .location = expr->value.binary.operator_token->position,
            .value.invalid_binary_expression_operands = {
                .operator = expr->value.binary.operator_token->value,
                .left_type = left.c_type,
                .right_type = right.c_type,
            },
        });
        return EXPR_ERR;
    }
}

expression_result_t ir_visit_multiplicative_binexpr(ir_gen_context_t *context, const expression_t *expr, expression_result_t left, expression_result_t right) {
    bool is_modulo = expr->value.binary.operator_token->kind == TK_PERCENT || expr->value.binary.operator_token->kind == TK_MOD_ASSIGN;
    bool is_division = expr->value.binary.operator_token->kind == TK_SLASH || expr->value.binary.operator_token->kind == TK_DIVIDE_ASSIGN;

    // Bubble up errors if the operands are invalid.
    if (left.kind == EXPR_RESULT_ERR || right.kind == EXPR_RESULT_ERR) return EXPR_ERR;

    if (left.is_lvalue) left = get_rvalue(context, left);
    if (right.is_lvalue) right = get_rvalue(context, right);

    // For multiplication/division both operands must have arithmetic type
    // For modulo both operands must have integer type
    if ((is_modulo && (!is_integer_type(left.c_type) || !is_integer_type(right.c_type))) ||
        (!is_modulo && (!is_arithmetic_type(left.c_type) || !is_arithmetic_type(right.c_type)))
    ) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_BINARY_EXPRESSION_OPERANDS,
            .location = expr->value.binary.operator_token->position,
            .value.invalid_binary_expression_operands = {
                .operator = expr->value.binary.operator_token->value,
                .left_type = left.c_type,
                .right_type = right.c_type,
            },
        });
        return EXPR_ERR;
    }

    // Type conversions
    const type_t *result_type = get_common_type(left.c_type, right.c_type);
    const ir_type_t *ir_result_type = get_ir_type(context,result_type);

    left = convert_to_type(context, left.value, left.c_type, result_type);
    right = convert_to_type(context, right.value, right.c_type, result_type);

    ir_value_t result;

    if (left.value.kind == IR_VALUE_CONST && right.value.kind == IR_VALUE_CONST) {
        // constant folding
        ir_const_t value = (ir_const_t) {
            .kind = is_floating_type(result_type) ? IR_CONST_FLOAT : IR_CONST_INT,
            .type = ir_result_type,
            .value.i = 0,
        };

        if (ir_is_integer_type(ir_result_type)) {
            // TODO: emit warning and set undefined value for division by zero
            // For now we will just set the value to 0 and move on
            if (is_division && right.value.constant.value.i == 0) {
                value.value.i = 0;
            } else {
                if (is_modulo) value.value.i = left.value.constant.value.i % right.value.constant.value.i;
                else if (is_division) value.value.i = left.value.constant.value.i / right.value.constant.value.i;
                else value.value.i = left.value.constant.value.i * right.value.constant.value.i;
            }
        } else {
            // no modulo operator for floating point
            if (is_division) value.value.f = left.value.constant.value.f / right.value.constant.value.f;
            else value.value.f = left.value.constant.value.f * right.value.constant.value.f;
        }

        result = (ir_value_t) {
            .kind = IR_VALUE_CONST,
            .constant = value,
        };
    } else {
        ir_var_t temp = temp_var(context, ir_result_type);
        if (is_modulo) {
            ir_build_mod(context->builder, left.value, right.value, temp);
        } else if (is_division) {
            ir_build_div(context->builder, left.value, right.value, temp);
        } else { // multiplication
            ir_build_mul(context->builder, left.value, right.value, temp);
        }
        result = ir_value_for_var(temp);
    }

    return (expression_result_t) {
        .kind = EXPR_RESULT_VALUE,
        .c_type = result_type,
        .is_lvalue = false,
        .is_string_literal = false,
        .addr_of = false,
        .value = result,
    };
}

expression_result_t ir_visit_bitwise_binexpr(ir_gen_context_t *context, const expression_t *expr, expression_result_t left, expression_result_t right) {
    // Bubble up errors if the operands are invalid.
    if (left.kind == EXPR_RESULT_ERR || right.kind == EXPR_RESULT_ERR) return EXPR_ERR;

    if (left.is_lvalue) left = get_rvalue(context, left);
    if (right.is_lvalue) right = get_rvalue(context, right);

    bool is_lshift = expr->value.binary.operator_token->kind == TK_LSHIFT || expr->value.binary.operator_token->kind == TK_LSHIFT_ASSIGN;
    bool is_rshift = expr->value.binary.operator_token->kind == TK_RSHIFT || expr->value.binary.operator_token->kind == TK_RSHIFT_ASSIGN;
    bool is_shift = is_lshift || is_rshift;
    bool is_and = expr->value.binary.operator_token->kind == TK_AMPERSAND || expr->value.binary.operator_token->kind == TK_BITWISE_AND_ASSIGN;
    bool is_or = expr->value.binary.operator_token->kind == TK_BITWISE_OR || expr->value.binary.operator_token->kind == TK_BITWISE_OR_ASSIGN;

    // For bitwise operators, both operands must have integer type
    if (!is_integer_type(left.c_type) || !is_integer_type(right.c_type)) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_BINARY_EXPRESSION_OPERANDS,
            .location = expr->value.binary.operator_token->position,
            .value.invalid_binary_expression_operands = {
                .operator = expr->value.binary.operator_token->value,
                .left_type = left.c_type,
                .right_type = right.c_type,
            },
        });
        return EXPR_ERR;
    }

    const type_t *common_type = get_common_type(left.c_type, right.c_type);
    const ir_type_t *result_type = get_ir_type(context,common_type);

    left = convert_to_type(context, left.value, left.c_type, common_type);
    right = convert_to_type(context, right.value, right.c_type, common_type);

    ir_value_t result;

    if (left.value.kind == IR_VALUE_CONST && right.value.kind == IR_VALUE_CONST) {
        // constant folding
        ir_const_t value = {
            .kind = IR_CONST_INT,
            .type = result_type,
            .value.i = 0,
        };

        if (is_lshift) value.value.i = left.value.constant.value.i << right.value.constant.value.i;
        else if (is_rshift) value.value.i = left.value.constant.value.i >> right.value.constant.value.i;
        else if (is_and) value.value.i = left.value.constant.value.i & right.value.constant.value.i;
        else if (is_or) value.value.i = left.value.constant.value.i | right.value.constant.value.i;
        else value.value.i = left.value.constant.value.i ^ right.value.constant.value.i;

        result = (ir_value_t) {
            .kind = IR_VALUE_CONST,
            .constant = value,
        };
    } else {
        ir_var_t temp = temp_var(context, result_type);
        if (is_shift) {
            if (is_lshift) ir_build_shl(context->builder, left.value, right.value, temp);
            else ir_build_shr(context->builder, left.value, right.value, temp);
        } else if (is_and) {
            ir_build_and(context->builder, left.value, right.value, temp);
        } else if (is_or) {
            ir_build_or(context->builder, left.value, right.value, temp);
        } else {
            ir_build_xor(context->builder, left.value, right.value, temp);
        }

        result = ir_value_for_var(temp);
    }

    return (expression_result_t) {
        .kind = EXPR_RESULT_VALUE,
        .c_type = common_type,
        .is_lvalue = false,
        .is_string_literal = false,
        .addr_of = false,
        .value = result,
    };
}

expression_result_t ir_visit_assignment_binexpr(ir_gen_context_t *context, const expression_t *expr) {
    assert(context != NULL && "Context must not be NULL");
    assert(expr != NULL && "Expression must not be NULL");

    // Evaluate the left and right operands.
    expression_result_t left = ir_visit_expression(context, expr->value.binary.left);
    expression_result_t right = ir_visit_expression(context, expr->value.binary.right);

    // Bubble up errors if the operands are invalid.
    if (left.kind == EXPR_RESULT_ERR || right.kind == EXPR_RESULT_ERR) return EXPR_ERR;

    // The left operand must be a lvalue.
    if (!left.is_lvalue || left.c_type->is_const) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_ASSIGNMENT_TARGET,
            .location = expr->value.binary.operator_token->position,
        });
        return EXPR_ERR;
    }

    if (expr->value.binary.operator_token->kind != TK_ASSIGN) {
        switch (expr->value.binary.operator.assignment) {
            case BINARY_ADD_ASSIGN:
            case BINARY_SUBTRACT_ASSIGN:
                right = ir_visit_additive_binexpr(context, expr, left, right);
                break;
            case BINARY_DIVIDE_ASSIGN:
            case BINARY_MODULO_ASSIGN:
            case BINARY_MULTIPLY_ASSIGN:
                right = ir_visit_multiplicative_binexpr(context, expr, left, right);
                break;
            case BINARY_BITWISE_AND_ASSIGN:
            case BINARY_BITWISE_OR_ASSIGN:
            case BINARY_BITWISE_XOR_ASSIGN:
            case BINARY_SHIFT_LEFT_ASSIGN:
            case BINARY_SHIFT_RIGHT_ASSIGN:
                right = ir_visit_bitwise_binexpr(context, expr, left, right);
                break;
            default:
                // This should be unreachable
                fprintf(stderr, "%s:%d IR generation error, unrecognized assignment operator\n", __FILE__, __LINE__);
                exit(1);
        }
    }

    bool is_struct_assignment = right.c_type->kind == TYPE_STRUCT_OR_UNION;
    if (!is_struct_assignment && right.is_lvalue) right = get_rvalue(context, right);

    // Generate an assignment instruction.
    if (!types_equal(left.c_type, right.c_type)) {
        // Convert the right operand to the type of the left operand.
        right = convert_to_type(context, right.value, right.c_type, left.c_type);
        if (right.kind == EXPR_RESULT_ERR) return EXPR_ERR;
    }

    ir_value_t ptr;
    if (left.kind == EXPR_RESULT_VALUE) {
        ptr = left.value;
    } else if (left.kind == EXPR_RESULT_INDIRECTION) {
        ptr = get_indirect_ptr(context, left);
    } else {
        return EXPR_ERR;
    }

    if (is_struct_assignment) {
        // the struct types should be the same, so it doesn't matter if we use the length of the dest or src
        ssize_t size = ir_size_of_type_bytes(context->arch, ir_get_type_of_value(ptr)->value.ptr.pointee);
        ir_value_t length_val = ir_make_const_int(context->arch->ptr_int_type, size);
        ir_build_memcpy(context->builder, ptr, right.value, length_val);
    } else {
        ir_build_store(context->builder, ptr, right.value);
    }

    // assignments can be chained, e.g. `a = b = c;`
    return left;
}

expression_result_t ir_visit_comparison_binexpr(ir_gen_context_t *context, const expression_t *expr) {
    assert(context != NULL && "Context must not be NULL");
    assert(expr != NULL && "Expression must not be NULL");
    assert(expr->kind == EXPRESSION_BINARY && expr->value.binary.kind == BINARY_COMPARISON);

    // Evaluate the left and right operands.
    expression_result_t left = ir_visit_expression(context, expr->value.binary.left);
    expression_result_t right = ir_visit_expression(context, expr->value.binary.right);

    // Bubble up errors if the operands are invalid.
    if (left.kind == EXPR_RESULT_ERR || right.kind == EXPR_RESULT_ERR) return EXPR_ERR;

    if (left.is_lvalue) left = get_rvalue(context, left);
    if (right.is_lvalue) right = get_rvalue(context, right);

    // One of the following must be true:
    // 1. both operands have arithmetic type
    // 2. both operands are pointers to compatible types
    // 3. both operands are pointers, and one is a pointer to void
    // 4. one operand is a pointer and the other is a null pointer constant

    // We will lazily relax this to allow comparisons between two arithmetic types, or two pointer types.
    // TODO: Implement the correct type restrictions for pointer comparisons.

    if ((is_arithmetic_type(left.c_type) && is_arithmetic_type(right.c_type)) ||
        (is_pointer_type(left.c_type) && is_pointer_type(right.c_type))
    ) {
        const type_t *common_type;
        if (!is_pointer_type(left.c_type)) {
            common_type = get_common_type(left.c_type, right.c_type);
        } else {
            common_type = c_ptr_uint_type();
        }
        left = convert_to_type(context, left.value, left.c_type, common_type);
        right = convert_to_type(context, right.value, right.c_type, common_type);

        if (left.kind == EXPR_RESULT_ERR || right.kind == EXPR_RESULT_ERR) return EXPR_ERR;

        ir_value_t result;
        const binary_comparison_operator_t op = expr->value.binary.operator.comparison;

        if (left.value.kind == IR_VALUE_CONST && right.value.kind == IR_VALUE_CONST) {
            // constant folding
            ir_const_t value = {
                .kind = IR_CONST_INT,
                .type = &IR_BOOL,
                .value.i = 0,
            };
            bool floating = is_floating_type(common_type);
            long double leftf;
            long double rightf;
            long long lefti;
            long long righti;
            if (floating) {
                leftf = left.value.constant.kind == IR_CONST_INT ? left.value.constant.value.i : left.value.constant.value.f;
                rightf = right.value.constant.kind == IR_CONST_INT ? right.value.constant.value.i : right.value.constant.value.f;
            } else {
                lefti = left.value.constant.kind == IR_CONST_INT ? left.value.constant.value.i : left.value.constant.value.f;
                righti = right.value.constant.kind == IR_CONST_INT ? right.value.constant.value.i : right.value.constant.value.f;
            }
            switch (op) {
                case BINARY_COMPARISON_EQUAL:
                    value.value.i = floating ? leftf == rightf : lefti == righti;
                    break;
                case BINARY_COMPARISON_NOT_EQUAL:
                    value.value.i = floating ? leftf != rightf : lefti != righti;
                    break;
                case BINARY_COMPARISON_LESS_THAN:
                    value.value.i = floating ? leftf < rightf : lefti < righti;
                    break;
                case BINARY_COMPARISON_LESS_THAN_OR_EQUAL:
                    value.value.i = floating ? leftf <= rightf : lefti <= righti;
                    break;
                case BINARY_COMPARISON_GREATER_THAN:
                    value.value.i = floating ? leftf > rightf : lefti > righti;
                    break;
                case BINARY_COMPARISON_GREATER_THAN_OR_EQUAL:
                    value.value.i = floating ? leftf >= rightf : lefti >= righti;
                    break;
                default:
                    // Invalid comparison operator
                    // Should be unreachable
                    assert(false && "Invalid comparison operator");
            }
            result = (ir_value_t) {
                .kind = IR_VALUE_CONST,
                .constant = value,
            };
        } else {
            ir_var_t temp = temp_var(context, &IR_BOOL);
            switch (op) {
                case BINARY_COMPARISON_EQUAL:
                    ir_build_eq(context->builder, left.value, right.value, temp);
                    break;
                case BINARY_COMPARISON_NOT_EQUAL:
                    ir_build_ne(context->builder, left.value, right.value, temp);
                    break;
                case BINARY_COMPARISON_LESS_THAN:
                    ir_build_lt(context->builder, left.value, right.value, temp);
                    break;
                case BINARY_COMPARISON_LESS_THAN_OR_EQUAL:
                    ir_build_le(context->builder, left.value, right.value, temp);
                    break;
                case BINARY_COMPARISON_GREATER_THAN:
                    ir_build_gt(context->builder, left.value, right.value, temp);
                    break;
                case BINARY_COMPARISON_GREATER_THAN_OR_EQUAL:
                    ir_build_ge(context->builder, left.value, right.value, temp);
                    break;
                default:
                    // Invalid comparison operator
                    // Should be unreachable
                    assert(false && "Invalid comparison operator");
            }
            result = ir_value_for_var(temp);
        }

        return (expression_result_t) {
            .kind = EXPR_RESULT_VALUE,
            .c_type = &BOOL,
            .is_lvalue = false,
            .is_string_literal = false,
            .addr_of = false,
            .value = result,
        };
    } else if (is_pointer_type(left.c_type) && is_pointer_type(right.c_type)) {
        // TODO: Implement pointer comparisons
        assert(false && "Pointer comparisons not implemented");

    } else {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_BINARY_EXPRESSION_OPERANDS,
            .location = expr->value.binary.operator_token->position,
            .value.invalid_binary_expression_operands = {
                .operator = expr->value.binary.operator_token->value,
                .left_type = left.c_type,
                .right_type = right.c_type,
            },
        });
        return EXPR_ERR;
    }
}

expression_result_t ir_visit_logical_expression(ir_gen_context_t *context, const expression_t *expr) {
    assert(context != NULL && "Context must not be NULL");
    assert(expr != NULL && "Expression must not be NULL");
    assert(expr->kind == EXPRESSION_BINARY && expr->value.binary.kind == BINARY_LOGICAL);

    // Whether the operator is logical AND ('&&') or logical OR ('||')
    bool is_logical_and = expr->value.binary.operator.logical == BINARY_LOGICAL_AND;
    bool is_logical_or = !is_logical_and;

    // Evaluate the left operand
    // The logical && and || operators are short-circuiting, so if the left operand is false (for &&) or true (for ||),
    // then the right operand is not evaluated.
    expression_result_t left = ir_visit_expression(context, expr->value.binary.left);
    if (left.kind == EXPR_RESULT_ERR) return EXPR_ERR;
    if (left.is_lvalue) left = get_rvalue(context, left);

    // Both operands must have scalar type
    if (!is_scalar_type(left.c_type)) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_LOGICAL_BINARY_EXPRESSION_OPERAND_TYPE,
            .location = expr->value.binary.left->span.start,
            .value.invalid_logical_binary_expression_operand_type = {
                .type = left.c_type,
            },
        });
        return EXPR_ERR;
    }

    // Convert the left operand to a boolean value (if it is not already)
    // We already know that the left operand is a scalar type, so we don't need to check for errors since its a
    // valid conversion.
    ir_value_t left_bool = get_boolean_value(context, left.value, left.c_type, expr->value.binary.left).value;
    if (left_bool.kind == IR_VALUE_CONST) {
        // constant folding
        if ((is_logical_and && left_bool.constant.value.i == 0) || (is_logical_or && left_bool.constant.value.i != 0)) {
            // result is the value of the left operand (false for and, true for or)
            return (expression_result_t) {
                .kind = EXPR_RESULT_VALUE,
                .c_type = &BOOL,
                .is_lvalue = false,
                .is_string_literal = false,
                .addr_of = false,
                .value = left_bool,
            };
        } else {
            // result is the value of the right operand
            expression_result_t right = ir_visit_expression(context, expr->value.binary.right);
            if (right.kind == EXPR_RESULT_ERR) return EXPR_ERR;
            if (right.is_lvalue) right = get_rvalue(context, right);
            if (!is_scalar_type(right.c_type)) {
                append_compilation_error(&context->errors, (compilation_error_t) {
                    .kind = ERR_INVALID_LOGICAL_BINARY_EXPRESSION_OPERAND_TYPE,
                    .location = expr->value.binary.right->span.start,
                    .value.invalid_logical_binary_expression_operand_type = {
                        .type = right.c_type,
                    },
                });
                return EXPR_ERR;
            }
            ir_value_t right_bool = get_boolean_value(context, right.value, right.c_type, expr->value.binary.right).value;
            return (expression_result_t) {
                .kind = EXPR_RESULT_VALUE,
                .c_type = &BOOL,
                .is_lvalue = false,
                .is_string_literal = false,
                .addr_of = false,
                .value = right_bool,
            };
        }
    }

    // && - if the left operand is false, the result is false, otherwise the result is the value of the right operand
    // || - if the left operand is true, the result is true, otherwise the result is the value of the right operand
    ir_var_t result = temp_var(context, &IR_BOOL);
    ir_build_assign(context->builder, left_bool, result);
    const char* merge_label = gen_label(context);
    if (is_logical_and) {
        // if the left operand is false, the result is false
        // otherwise the result is the value of the right operand
        ir_var_t cond = temp_var(context, &IR_BOOL);
        ir_build_not(context->builder, left_bool, cond);
        ir_build_br_cond(context->builder, ir_value_for_var(cond), merge_label);
    } else {
        // if the left operand is true, the result is true
        // otherwise the result is the value of the right operand
        ir_build_br_cond(context->builder, left_bool, merge_label);
    }

    // Evaluate the right operand
    expression_result_t right = ir_visit_expression(context, expr->value.binary.right);
    if (right.kind == EXPR_RESULT_ERR) return EXPR_ERR;
    if (right.is_lvalue) right = get_rvalue(context, right);

    // Both operands must have scalar type
    if (!is_scalar_type(right.c_type)) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_LOGICAL_BINARY_EXPRESSION_OPERAND_TYPE,
            .location = expr->value.binary.left->span.start,
            .value.invalid_logical_binary_expression_operand_type = {
                .type = right.c_type,
            },
        });
        return EXPR_ERR;
    }

    // Convert the right operand to a boolean value (if it is not already)
    ir_value_t right_bool = right.value;
    if  (ir_get_type_of_value(right_bool)->kind != IR_TYPE_BOOL) {
        ir_var_t temp = temp_var(context, &IR_BOOL);
        ir_build_ne(context->builder, right.value, ir_get_zero_value(context, ir_get_type_of_value(right_bool)), temp);
        right_bool = ir_value_for_var(temp);
    }
    ir_build_assign(context->builder, right_bool, result);
    ir_build_nop(context->builder, merge_label);

    return (expression_result_t) {
        .kind = EXPR_RESULT_VALUE,
        .c_type = &BOOL,
        .is_lvalue = false,
        .is_string_literal = false,
        .addr_of = false,
        .value = ir_value_for_var(result),
    };
}

expression_result_t ir_visit_sizeof_expression(ir_gen_context_t *context, const expression_t *expr) {
    assert(expr != NULL && expr->kind == EXPRESSION_SIZEOF);
    const ir_type_t *type = get_ir_type(context, expr->value.sizeof_type);
    ssize_t size = ir_size_of_type_bytes(context->arch, type);
    const ir_value_t size_val = ir_make_const_int(context->arch->ptr_int_type, (long long) size);
    return (expression_result_t) {
        .addr_of = false,
        .c_type = c_ptr_uint_type(),
        .is_lvalue = false,
        .is_string_literal = false,
        .kind = EXPR_RESULT_VALUE,
        .value = size_val,
    };
}

expression_result_t ir_visit_ternary_expression(ir_gen_context_t *context, const expression_t *expr) {
    assert(context != NULL && "Context must not be NULL");
    assert(expr != NULL && "Expression must not be NULL");
    assert(expr->kind == EXPRESSION_TERNARY);

    expression_result_t condition = ir_visit_expression(context, expr->value.ternary.condition);
    if (condition.kind == EXPR_RESULT_ERR) return EXPR_ERR;
    if (condition.is_lvalue) condition = get_rvalue(context, condition);

    // The condition must have scalar type
    if (!is_scalar_type(condition.c_type)) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_TERNARY_CONDITION_TYPE,
            .location = expr->value.ternary.condition->span.start,
            .value.invalid_ternary_condition_type = {
                .type = condition.c_type,
            },
        });
        return EXPR_ERR;
    }

    const char* true_label = gen_label(context);
    const char* merge_label = gen_label(context);

    // Get the boolean value of the condition
    ir_value_t ir_condition = get_boolean_value(context, condition.value, condition.c_type, expr->value.ternary.condition).value;

    expression_result_t true_result;
    expression_result_t false_result;

    ir_instruction_node_t *true_branch_end = NULL;
    ir_instruction_node_t *false_branch_end = NULL;

    if (ir_condition.kind == IR_VALUE_CONST) {
        // Constant folding
        // Even though one of the branches will not be evaluated, we still need to visit it to perform semantic analysis
        // and to decide the type of the result. We will just throw away the generated code afterwards.

        if (ir_condition.constant.value.i != 0) {
            // Evaluate the true branch
            true_result = ir_visit_expression(context, expr->value.ternary.true_expression);
            if (true_result.kind == EXPR_RESULT_ERR) return EXPR_ERR;
            // Throw away the code for the false branch
            ir_instruction_node_t *position = ir_builder_get_position(context->builder);
            false_result = ir_visit_expression(context, expr->value.ternary.false_expression);
            ir_builder_clear_after(context->builder, position);
        } else {
            // Evaluate the false branch
            false_result = ir_visit_expression(context, expr->value.ternary.false_expression);
            if (false_result.kind == EXPR_RESULT_ERR) return EXPR_ERR;
            // Throw away the code for the true branch
            ir_instruction_node_t *position = ir_builder_get_position(context->builder);
            true_result = ir_visit_expression(context, expr->value.ternary.true_expression);
            ir_builder_clear_after(context->builder, position);
        }
    } else {
        // Branch based on the condition, falls through to the false branch
        ir_build_br_cond(context->builder, ir_condition, true_label);

        // False branch
        false_result = ir_visit_expression(context, expr->value.ternary.false_expression);
        if (false_result.kind == EXPR_RESULT_ERR) return EXPR_ERR;
        if (false_result.is_lvalue) false_result = get_rvalue(context, false_result);
        false_branch_end = ir_builder_get_position(context->builder);

        // True branch
        ir_build_nop(context->builder, true_label);
        true_result = ir_visit_expression(context, expr->value.ternary.true_expression);
        if (true_result.kind == EXPR_RESULT_ERR) return EXPR_ERR;
        if (true_result.is_lvalue) true_result = get_rvalue(context, true_result);
        true_branch_end = ir_builder_get_position(context->builder);
    }

    // One of the following must be true of the true and false operands:
    // 1. both have arithmetic type
    // 2. both have the same structure or union type (TODO)
    // 3. both operands have void type
    // 4. both operands are pointers to compatible types
    // 5. one operand is a pointer and the other is a null pointer constant
    // 6. one operand is a pointer to void, and the other is a pointer

    // This is a bit awkward, because we don't know the expected result type until after
    // generating code for the true and false branches.
    // After we know the result type, we have to generate conversion code (if necessary), then add an
    // assignment to the result variable in both branches (the IR is not in SSA form, so no phi node/block arguments).

    const type_t *result_type;
    const ir_type_t *ir_result_type;

    if (is_arithmetic_type(true_result.c_type) && is_arithmetic_type(false_result.c_type)) {
        result_type = get_common_type(true_result.c_type, false_result.c_type);
        ir_result_type = get_ir_type(context,result_type);
    } else if (true_result.c_type->kind == TYPE_VOID && false_result.c_type->kind == TYPE_VOID) {
        return (expression_result_t) {
            .kind = EXPR_RESULT_VALUE,
            .c_type = &VOID,
            .is_lvalue = false,
            .is_string_literal = false,
            .value = (ir_value_t) {
                .kind = IR_VALUE_CONST,
                .constant = (ir_const_t) {
                    .kind = IR_CONST_INT,
                    .type = &IR_VOID,
                    .value.i = 0,
                },
            }
        };
    } else if (is_pointer_type(true_result.c_type) && is_pointer_type(false_result.c_type)) {
        // TODO: pointer compatibility checks
        // For now, we will just use the type of the first non void* pointer branch
        result_type = true_result.c_type->value.pointer.base->kind == TYPE_VOID ? false_result.c_type : true_result.c_type;
        ir_result_type = get_ir_type(context,result_type);
    } else {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_TERNARY_EXPRESSION_OPERANDS,
            .location = expr->value.ternary.condition->span.start, // TODO: use the '?' token position
            .value.invalid_ternary_expression_operands = {
                .true_type = true_result.c_type,
                .false_type = false_result.c_type,
            },
        });
        return EXPR_ERR;
    }

    if (ir_condition.kind == IR_VALUE_CONST) {
        // Constant folding
        expression_result_t result_expr;
        return ir_condition.constant.value.i != 0
            ? convert_to_type(context, true_result.value, true_result.c_type, result_type)
            : convert_to_type(context, false_result.value, false_result.c_type, result_type);
    }

    ir_var_t result = temp_var(context, ir_result_type);

    ir_builder_position_after(context->builder, false_branch_end);
    if (!types_equal(false_result.c_type, result_type)) {
        false_result = convert_to_type(context, false_result.value, false_result.c_type, result_type);
        ir_build_assign(context->builder, false_result.value, result);
    } else {
        ir_build_assign(context->builder, false_result.value, result);
    }
    ir_build_br(context->builder, merge_label);

    ir_builder_position_after(context->builder, true_branch_end);
    if (!types_equal(true_result.c_type, result_type)) {
        true_result = convert_to_type(context, true_result.value, true_result.c_type, result_type);
        ir_build_assign(context->builder, true_result.value, result);
    } else {
        ir_build_assign(context->builder, true_result.value, result);
    }

    // Merge block
    ir_build_nop(context->builder, merge_label);

    return (expression_result_t) {
        .kind = EXPR_RESULT_VALUE,
        .c_type = result_type,
        .is_lvalue = false,
        .is_string_literal = false,
        .addr_of = false,
        .value = ir_value_for_var(result),
    };
}

expression_result_t ir_visit_unary_expression(ir_gen_context_t *context, const expression_t *expr) {
    assert(context != NULL && "Context must not be NULL");
    assert(expr != NULL && "Expression must not be NULL");
    assert(expr->kind == EXPRESSION_UNARY);

    switch (expr->value.unary.operator) {
        case UNARY_BITWISE_NOT:
            return ir_visit_bitwise_not_unexpr(context, expr);
        case UNARY_LOGICAL_NOT:
            return ir_visit_logical_not_unexpr(context, expr);
        case UNARY_ADDRESS_OF:
            return ir_visit_address_of_unexpr(context, expr);
        case UNARY_DEREFERENCE:
            return ir_visit_indirection_unexpr(context, expr);
        case UNARY_SIZEOF:
            return ir_visit_sizeof_unexpr(context, expr);
        case UNARY_PRE_DECREMENT:
            return ir_visit_increment_decrement(context, expr, true, false);
        case UNARY_POST_DECREMENT:
            return ir_visit_increment_decrement(context, expr, false, false);
        case UNARY_PRE_INCREMENT:
            return ir_visit_increment_decrement(context, expr, true, true);
        case UNARY_POST_INCREMENT:
            return ir_visit_increment_decrement(context, expr, false, true);
        case UNARY_MINUS:
            return ir_visit_minus_unexpr(context, expr);
        default:
            fprintf(stderr, "%s:%d:%d: Unary operator not implemented\n",
                expr->span.start.path, expr->span.start.line, expr->span.start.column);
            exit(1);
    }
}

expression_result_t ir_visit_minus_unexpr(ir_gen_context_t *context, const expression_t *expr) {
    // Unary minus negates its operand
    // The operand must have arithmetic type
    // The operand is promoted

    expression_result_t operand = ir_visit_expression(context, expr->value.unary.operand);
    if (operand.kind == EXPR_RESULT_ERR) return EXPR_ERR;
    if (operand.is_lvalue) operand = get_rvalue(context, operand);
    if (!is_arithmetic_type(operand.c_type)) {
        // The operand must have arithmetic type
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_UNARY_ARITHMETIC_OPERATOR_TYPE,
            .location = expr->value.unary.operand->span.start,
            .value.invalid_unary_arithmetic_operator_type = {
                .type = operand.c_type,
                .operator = *expr->value.unary.token,
            },
        });
        return EXPR_ERR;
    }

    // Apply integer promotions (if integral type)
    const type_t *result_ctype = type_after_integer_promotion(operand.c_type);
    operand = convert_to_type(context, operand.value, operand.c_type, result_ctype);

    if (operand.value.kind == IR_VALUE_CONST) {
        // Constant folding
        ir_value_t result;
        if (operand.value.constant.kind == IR_CONST_FLOAT) {
            result = (ir_value_t) {
                .kind = IR_VALUE_CONST,
                .constant = {
                    .kind = IR_CONST_INT,
                    .type = ir_get_type_of_value(operand.value),
                    .value.f = 0.0 - operand.value.constant.value.f,
                },
            };
        } else {
            result = (ir_value_t) {
                .kind = IR_VALUE_CONST,
                .constant = {
                    .kind = IR_CONST_INT,
                    .type = ir_get_type_of_value(operand.value),
                    .value.i = 0ll - operand.value.constant.value.i,
                },
            };
        }
        return (expression_result_t) {
            .kind = EXPR_RESULT_VALUE,
            .c_type = operand.c_type,
            .is_lvalue = false,
            .is_string_literal = false,
            .addr_of = false,
            .value = result,
        };
    }

    // Negate by subtracting the value from 0
    ir_var_t result = temp_var(context, ir_get_type_of_value(operand.value));
    ir_value_t zero = ir_get_zero_value(context, result.type);
    ir_build_sub(context->builder,zero, operand.value, result);

    return (expression_result_t) {
        .kind = EXPR_RESULT_VALUE,
        .c_type = operand.c_type,
        .is_lvalue = false,
        .is_string_literal = false,
        .addr_of = false,
        .value = ir_value_for_var(result),
    };
}

expression_result_t ir_visit_bitwise_not_unexpr(ir_gen_context_t *context, const expression_t *expr) {
    assert(context != NULL && "Context must not be NULL");
    assert(expr != NULL && "Expression must not be NULL");
    assert(expr->kind == EXPRESSION_UNARY);

    expression_result_t operand = ir_visit_expression(context, expr->value.unary.operand);
    if (operand.kind == EXPR_RESULT_ERR) return EXPR_ERR;
    if (operand.is_lvalue) operand = get_rvalue(context, operand);

    if (!is_integer_type(operand.c_type)) {
        // The operand must have integer type
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_UNARY_ARITHMETIC_OPERATOR_TYPE,
            .location = expr->value.unary.operand->span.start,
            .value.invalid_unary_arithmetic_operator_type = {
                .type = operand.c_type,
                .operator = *expr->value.unary.token,
            },
        });
        return EXPR_ERR;
    }

    if (operand.value.kind == IR_VALUE_CONST) {
        // Constant folding
        ir_value_t result = {
            .kind = IR_VALUE_CONST,
            .constant = {
                .kind = IR_CONST_INT,
                .type = ir_get_type_of_value(operand.value),
                .value.i = ~operand.value.constant.value.i,
            },
        };
        return (expression_result_t) {
            .kind = EXPR_RESULT_VALUE,
            .c_type = operand.c_type,
            .is_lvalue = false,
            .is_string_literal = false,
            .addr_of = false,
            .value = result,
        };
    }

    ir_var_t result = temp_var(context, ir_get_type_of_value(operand.value));
    ir_build_not(context->builder, operand.value, result);

    return (expression_result_t) {
        .kind = EXPR_RESULT_VALUE,
        .c_type = operand.c_type,
        .is_lvalue = false,
        .is_string_literal = false,
        .addr_of = false,
        .value = ir_value_for_var(result),
    };
}

expression_result_t ir_visit_logical_not_unexpr(ir_gen_context_t *context, const expression_t *expr) {
    assert(expr != NULL && expr->kind == EXPRESSION_UNARY);

    expression_result_t operand = ir_visit_expression(context, expr->value.unary.operand);
    if (operand.kind == EXPR_RESULT_ERR) return EXPR_ERR;
    if (operand.is_lvalue) operand = get_rvalue(context, operand);

    if (!is_scalar_type(operand.c_type)) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_UNARY_ARITHMETIC_OPERATOR_TYPE,
            .location = expr->value.unary.token->position,
            .value.invalid_unary_arithmetic_operator_type = {
                .operator = *expr->value.unary.token,
                .type = operand.c_type,
            }
        });
        return EXPR_ERR;
    }

    // The result has type int. It is 0 if the value of the operand compares unequal to 0, otherwise the result is 1.
    // The expression !expr is equivalent to (0 == expr).

    ir_value_t result;
    if (operand.value.kind == IR_VALUE_CONST) {
        // constant volding
        assert(operand.value.constant.kind == IR_CONST_INT);
        result = ir_make_const_int(context->arch->sint, operand.value.constant.value.i == 0 ? 1 : 0);
    } else {
        // Get a constant zero of the same type as the operand
        ir_value_t zero = ir_get_zero_value(context, ir_get_type_of_value(operand.value));

        // Compare to 0
        ir_var_t comparisson_result = temp_var(context, &IR_BOOL);
        ir_build_eq(context->builder, operand.value, zero, comparisson_result);

        // Extend the result to an int, as a boolean is just a 1-bit integer
        ir_var_t int_result = temp_var(context, context->arch->sint);
        ir_build_ext(context->builder, ir_value_for_var(comparisson_result), int_result);
        result = ir_value_for_var(int_result);
    }

    return (expression_result_t) {
        .addr_of = false,
        .c_type = &INT,
        .is_lvalue = false,
        .is_string_literal = false,
        .kind = EXPR_RESULT_VALUE,
        .value = result,
    };
}

expression_result_t ir_visit_address_of_unexpr(ir_gen_context_t *context, const expression_t *expr) {
    // The operand of the unary address of ('&') operator must be one of:
    // 1. A function designator
    // 2. The result of a [] or * operator
    // 3. A lvalue that designates an object that is not a bit-field and does not have the 'register' storage-class specifier

    expression_result_t operand = ir_visit_expression(context, expr->value.unary.operand);
    if (operand.kind == EXPR_RESULT_ERR) return EXPR_ERR;

    if (operand.is_lvalue) {
        return (expression_result_t) {
            .kind = EXPR_RESULT_VALUE,
            .value = operand.value,
            .c_type = operand.c_type,
            .is_lvalue = false,
            .is_string_literal = false,
            .addr_of = true,
        };
    } else {
        // TODO: handle result of [] or * operator, function designator
        assert(false && "Unimplemented");
        exit(1);
    }
}

expression_result_t ir_visit_indirection_unexpr(ir_gen_context_t *context, const expression_t *expr) {
    expression_result_t operand = ir_visit_expression(context, expr->value.unary.operand);
    if (operand.kind == EXPR_RESULT_ERR) return EXPR_ERR;

    // The operand must be a pointer.
    if (!is_pointer_type(operand.c_type)) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_UNARY_INDIRECTION_OPERAND_NOT_PTR_TYPE,
            .location = expr->span.start,
        });
        return EXPR_ERR;
    }

    // If the operand points to a function, the result is a function designator.
    // Otherwise, the result is a lvalue designating the object or function designated by the operand.
    if (operand.c_type->value.pointer.base->kind == TYPE_FUNCTION) {
        // TODO: dereference function pointers
        assert(false && "De-referencing function pointers not implemented");
    } else {
        expression_result_t *inner = malloc(sizeof(expression_result_t));
        *inner = operand;

        return (expression_result_t) {
            .kind = EXPR_RESULT_INDIRECTION,
            .c_type = operand.c_type->value.pointer.base,
            .is_lvalue = true,
            .is_string_literal = false,
            .addr_of = false,
            .indirection_inner = inner,
        };
    }
}

expression_result_t ir_visit_sizeof_unexpr(ir_gen_context_t *context, const expression_t *expr) {
    assert(expr != NULL && expr->kind == EXPRESSION_UNARY);
    // TODO: error if sizeof is applied to an expression that designates a bit-field member
    expression_result_t operand = ir_visit_expression(context, expr->value.unary.operand);
    if (operand.kind == EXPR_RESULT_ERR) return EXPR_ERR;
    const ir_type_t *ir_type = ir_get_type_of_value(operand.value);
    if (operand.is_lvalue) ir_type = ir_type->value.ptr.pointee;
    ssize_t size = ir_size_of_type_bytes(context->arch, ir_type);
    ir_value_t size_val = ir_make_const_int(ir_ptr_int_type(context), (long long) size);
    return (expression_result_t) {
        .kind = EXPR_RESULT_VALUE,
        .addr_of = false,
        .c_type = c_ptr_uint_type(),
        .is_lvalue = false,
        .is_string_literal = false,
        .value = size_val,
    };
}

expression_result_t ir_visit_increment_decrement(ir_gen_context_t *context, const expression_t *expr, bool pre, bool increment) {
    assert(expr != NULL && expr->kind == EXPRESSION_UNARY);

    expression_result_t lvalue = ir_visit_expression(context, expr->value.unary.operand);
    if (lvalue.kind == EXPR_RESULT_ERR) return EXPR_ERR;
    if (!lvalue.is_lvalue || lvalue.kind != EXPR_RESULT_VALUE) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_ASSIGNMENT_TARGET,
            .location = expr->value.unary.token->position,
        });
        return EXPR_ERR;
    }

    expression_result_t rvalue = get_rvalue(context, lvalue);
    if (rvalue.kind == EXPR_RESULT_ERR) return EXPR_ERR;

    // The semantics of the increment/decrement operators are similar to the additive operators.
    // The operand must have an arithmetic or pointer type.
    // TODO: this should also work for enums (?) when those get implemented
    if (!is_arithmetic_type(rvalue.c_type) && !is_pointer_type(rvalue.c_type)) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_CANNOT_INCREMENT_DECREMENT_TYPE,
            .location = expr->value.unary.token->position,
            .value.cannot_increment_decrement_type = {
                .type = rvalue.c_type,
            },
        });
        return EXPR_ERR;
    }

    const ir_type_t *ir_type = rvalue.value.var.type;
    ir_var_t post_value = temp_var(context, ir_type);
    if (is_integer_type(rvalue.c_type)) {
        ir_value_t rhs = ir_make_const_int(ir_type, 1);
        if (increment) ir_build_add(context->builder, rvalue.value, rhs, post_value);
        else ir_build_sub(context->builder, rvalue.value, rhs, post_value);
    } else if (is_floating_type(rvalue.c_type)) {
        ir_value_t rhs = ir_make_const_float(ir_type, 1.0);
        if (increment) ir_build_add(context->builder, rvalue.value, rhs, post_value);
        else ir_build_sub(context->builder, rvalue.value, rhs, post_value);
    } else /* has pointer type */ {
        ir_build_get_array_element_ptr(context->builder, rvalue.value, ir_make_const_int(&IR_I32, increment ? 1 : -1), post_value);
    }

    ir_build_store(context->builder, lvalue.value, ir_value_for_var(post_value));

    return (expression_result_t) {
        .addr_of = false,
        .c_type = lvalue.c_type,
        .is_lvalue = false,
        .is_string_literal = false,
        .kind = EXPR_RESULT_VALUE,
        .value = pre ? ir_value_for_var(post_value) : rvalue.value,
    };
}

expression_result_t ir_visit_member_access_expression(ir_gen_context_t *context, const expression_t *expr) {
    assert(expr != NULL && expr->kind == EXPRESSION_MEMBER_ACCESS);
    assert(expr->value.member_access.operator.kind == TK_ARROW || expr->value.member_access.operator.kind == TK_DOT);

    expression_result_t target = ir_visit_expression(context, expr->value.member_access.struct_or_union);
    if (target.kind == EXPR_RESULT_ERR) return EXPR_ERR;

    // The target must be a struct or a pointer to a struct
    if (expr->value.member_access.operator.kind == TK_ARROW &&
        (target.c_type->kind != TYPE_POINTER || target.c_type->value.pointer.base->kind != TYPE_STRUCT_OR_UNION)) {
        // If the operator is '->', then the type of the target must be a pointer to a struct.
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_MEMBER_ACCESS_TARGET,
            .location = expr->value.member_access.operator.position,
            .value.invalid_member_access_target = {
                .type = target.c_type,
                .operator = expr->value.member_access.operator
            }
        });
        return EXPR_ERR;
    } else if (expr->value.member_access.operator.kind == TK_DOT && target.c_type->kind != TYPE_STRUCT_OR_UNION) {
        // If the operator is '.', then the type of the target must be a struct.
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_MEMBER_ACCESS_TARGET,
            .location = expr->value.member_access.operator.position,
            .value.invalid_member_access_target = {
                .type = target.c_type,
                .operator = expr->value.member_access.operator
            }
        });
        return EXPR_ERR;
    }

    // If the target is a pointer, we need to dereference it to get the base pointer
    if (target.c_type->kind == TYPE_POINTER) {
        target = get_rvalue(context, target);
    }

    ir_value_t base_ptr;
    if (target.kind == EXPR_RESULT_VALUE) {
        assert(ir_get_type_of_value(target.value)->kind == IR_TYPE_PTR);
        base_ptr = target.value;
    } else {
        base_ptr = get_indirect_ptr(context, target);
    }

    const ir_type_t *struct_type = ir_get_type_of_value(base_ptr)->value.ptr.pointee;
    const tag_t *tag = lookup_tag_by_uid(context, struct_type->value.struct_or_union.id);
    assert(tag != NULL);

    // Look up the field in the struct definition to find its index
    const ir_struct_field_t *ir_field = NULL;
    hash_table_lookup(&struct_type->value.struct_or_union.field_map, expr->value.member_access.member.value, (void**) &ir_field);
    if (ir_field == NULL) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_STRUCT_FIELD_REFERENCE,
            .location = expr->value.member_access.operator.position,
            .value.invalid_struct_field_reference = {
                .type = tag->c_type,
                .field = expr->value.member_access.member
            }
        });
        return EXPR_ERR;
    }

    // Lookup the field in the c type (guaranteed to exist if its in the corresponding ir struct type).
    // Note that if padding was added between fields, the field indexes will not be equal, and we will have to find the
    // field with a matching identifier.
    const type_t *c_struct_type = tag->c_type;
    const struct_field_t *c_field = NULL;
    for (int i = 0; i < c_struct_type->value.struct_or_union.fields.size; i += 1) {
        if (strcmp(ir_field->name, c_struct_type->value.struct_or_union.fields.buffer[i]->identifier->value) == 0) {
            c_field = c_struct_type->value.struct_or_union.fields.buffer[i];
            break;
        }
    }
    assert(c_field != NULL);

    const ir_var_t result = temp_var(context, get_ir_ptr_type(ir_field->type));
    ir_build_get_struct_member_ptr(context->builder, target.value, ir_field->index, result);

    return (expression_result_t) {
        .kind = EXPR_RESULT_VALUE,
        .c_type = c_field->type,
        .is_lvalue = true,
        .is_string_literal = false,
        .addr_of = false,
        .value = ir_value_for_var(result),
    };
}

expression_result_t ir_visit_compound_literal(ir_gen_context_t *context, const expression_t *expr) {
    assert(expr->kind == EXPRESSION_COMPOUND_LITERAL);

    const type_t *type = expr->value.compound_literal.type;

    // TODO: type check

    const tag_t *tag = lookup_tag(context, type->value.struct_or_union.identifier->value);
    type = tag->c_type;

    // Create a stack slot to store the result temporarily because we don't know where this is being stored
    const ir_type_t *ir_type = tag->ir_type;
    ir_var_t res = temp_var(context, get_ir_ptr_type(ir_type));
    insert_alloca(context, ir_type, res);

    ir_visit_initializer_list(context, ir_value_for_var(res), type, &expr->value.compound_literal.initializer_list);

    return (expression_result_t) {
        .kind = EXPR_RESULT_VALUE,
        .value = ir_value_for_var(res),
        .c_type = type,
        .is_lvalue = true,
        .symbol = NULL,
        .is_string_literal = false,
    };
}

expression_result_t ir_visit_primary_expression(ir_gen_context_t *context, const expression_t *expr) {
    assert(context != NULL && "Context must not be NULL");
    assert(expr != NULL && "Primary expression must not be NULL");
    assert(expr->kind == EXPRESSION_PRIMARY);

    switch (expr->value.primary.kind) {
        case PE_IDENTIFIER: {
            symbol_t *symbol = lookup_symbol(context, expr->value.primary.value.token.value);
            if (symbol == NULL) {
                source_position_t pos = expr->value.primary.value.token.position;
                append_compilation_error(&context->errors, (compilation_error_t) {
                    .kind = ERR_USE_OF_UNDECLARED_IDENTIFIER,
                    .location = pos,
                    .value.use_of_undeclared_identifier = {
                        .identifier = expr->value.primary.value.token.value,
                    },
                });
                return EXPR_ERR;
            }

            if (symbol->kind == SYMBOL_ENUMERATION_CONSTANT) {
                // some symbols don't actually represent a variable and have no address
                return (expression_result_t) {
                    .kind = EXPR_RESULT_VALUE,
                    .symbol = symbol,
                    .value = (ir_value_t) {
                        .kind = IR_VALUE_CONST,
                        .constant = symbol->const_value,
                    },
                    .c_type = symbol->c_type,
                    .is_lvalue = false,
                    .is_string_literal = false,
                    .addr_of = false,
                };
            } else {
                // others just represent an address in the data segment or on the stack
                return (expression_result_t) {
                    .kind = EXPR_RESULT_VALUE,
                    .c_type = symbol->c_type,
                    .is_lvalue = true,
                    .is_string_literal = false,
                    .addr_of = false,
                    .symbol = symbol,
                    .value = symbol->ir_ptr,
                };
            }
        }
        case PE_CONSTANT: {
            return ir_visit_constant(context, expr);
        }
        case PE_STRING_LITERAL: {
            // String literal semantics:
            // - A string literal is an array of characters with static storage duration.
            // - Whether identical string literals are distinct or share a single storage location
            //   is implementation-defined.
            // - Modifying a string literal results in undefined behavior.

            // First we need to replace escape sequences in the string literal
            char *literal = replace_escape_sequences(expr->value.primary.value.token.value);

            // Maybe there should be a special expression node type for static lengths?
            expression_t *array_length_expr = malloc(sizeof(expression_t));
            *array_length_expr = (expression_t) {
                .kind = EXPRESSION_PRIMARY,
                .value.primary = {
                    .kind = PE_CONSTANT,
                    .value.token = {
                        .kind = TK_INTEGER_CONSTANT,
                        .value = malloc(32),
                        .position = expr->value.primary.value.token.position,
                    },
                },
            };
            char *val = malloc(32);
            snprintf(val, 32, "%zu", strlen(literal) + 1);
            array_length_expr->value.primary.value.token.value = val;

            // The C type is an array of characters
            type_t *c_type = malloc(sizeof(type_t));
            *c_type = (type_t) {
                .kind = TYPE_ARRAY,
                .value.array = {
                    .element_type = &CHAR,
                    .size = array_length_expr,
                },
            };

            ir_type_t *ir_type = malloc(sizeof(ir_type_t));
            *ir_type = (ir_type_t) {
                .kind = IR_TYPE_ARRAY,
                .value.array = {
                    .element = &IR_I8,
                    .length = strlen(literal) + 1,
                },
            };

            ir_global_t *global = malloc(sizeof(ir_global_t));
            *global = (ir_global_t) {
                .name = global_name(context),
                .type = get_ir_ptr_type(ir_type),
                .initialized = true,
                .value = (ir_const_t) {
                    .type = ir_type,
                    .kind = IR_CONST_STRING,
                    .value.s = literal,
                },
            };
            ir_append_global_ptr(&context->module->globals, global);

            const ir_type_t *ir_ptr_type = get_ir_ptr_type(ir_type);
            ir_const_t const_ref = {
                .kind = IR_CONST_GLOBAL_POINTER,
                .type = ir_ptr_type,
                .value.global_name = global->name,
            };

            return (expression_result_t) {
                .kind = EXPR_RESULT_VALUE,
                .c_type = c_type,
                .is_lvalue = false,
                .is_string_literal = true,
                .addr_of = false,
                .value = ir_value_for_const(const_ref),
            };
        }
        case PE_EXPRESSION: {
            return ir_visit_expression(context, expr->value.primary.value.expression);
        }
        default: {
            // Unreachable
            fprintf(stderr, "Invalid primary expression\n");
            exit(1);
        }
    }
}

expression_result_t ir_visit_constant(ir_gen_context_t *context, const expression_t *expr) {
    assert(context != NULL && "Context must not be NULL");
    assert(expr != NULL && "Expression must not be NULL");
    assert(expr->kind == EXPRESSION_PRIMARY && expr->value.primary.kind == PE_CONSTANT);
    assert(expr->value.primary.value.token.value != NULL && "Token value must not be NULL");

    switch (expr->value.primary.value.token.kind) {
        case TK_CHAR_LITERAL: {
            // TODO: Handle escape sequences, wide character literals.
            char c = expr->value.primary.value.token.value[0];
            // In C char literals are ints
            return (expression_result_t) {
                .kind = EXPR_RESULT_VALUE,
                .c_type = &INT,
                .is_lvalue = false,
                .is_string_literal = false,
                .addr_of = false,
                .value = (ir_value_t) {
                    .kind = IR_VALUE_CONST,
                    .constant = (ir_const_t) {
                        .kind = IR_CONST_INT,
                        .type = &IR_I32,
                        .value.i = (int)c,
                    }
                }
            };
        }
        case TK_INTEGER_CONSTANT: {
            unsigned long long value;
            const type_t *c_type;
            decode_integer_constant(&expr->value.primary.value.token, &value, &c_type);
            return (expression_result_t) {
                .kind = EXPR_RESULT_VALUE,
                .c_type = c_type,
                .is_lvalue = false,
                .addr_of = false,
                .is_string_literal = false,
                .value = (ir_value_t) {
                    .kind = IR_VALUE_CONST,
                    .constant = (ir_const_t) {
                            .kind = IR_CONST_INT,
                            .type = get_ir_type(context,c_type),
                            .value.i = value,
                    }
                }
            };
        }
        case TK_FLOATING_CONSTANT: {
            long double value;
            const type_t *c_type;
            decode_float_constant(&expr->value.primary.value.token, &value, &c_type);
            return (expression_result_t) {
                .kind = EXPR_RESULT_VALUE,
                .c_type = c_type,
                .is_lvalue = false,
                .is_string_literal = false,
                .addr_of = false,
                .value = (ir_value_t) {
                    .kind = IR_VALUE_CONST,
                    .constant = (ir_const_t) {
                        .kind = IR_CONST_FLOAT,
                        .type = get_ir_type(context,c_type),
                        .value.f = value,
                    }
                }
            };
        }
        default: {
            // Unreachable
            fprintf(stderr, "Invalid constant expression\n");
            exit(1);
        }
    }
}
