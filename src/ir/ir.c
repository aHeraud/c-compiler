#include <stdio.h>
#include <string.h>
#include "ir/ir.h"

void append_ir_instruction(ir_instruction_vector_t *vector, ir_instruction_t instruction) {
    if (vector->size == vector->capacity) {
        vector->capacity = vector->capacity * 2 + 1;
        vector->buffer = realloc(vector->buffer, vector->capacity * sizeof(ir_instruction_t));
    }
    vector->buffer[vector->size++] = instruction;
}

size_t size_of_type(const ir_type_t *type) {
    switch (type->kind) {
        case IR_TYPE_BOOL:
            return 1;
        case IR_TYPE_I8:
        case IR_TYPE_U8:
            return 8;
        case IR_TYPE_I16:
        case IR_TYPE_U16:
            return 16;
        case IR_TYPE_I32:
        case IR_TYPE_U32:
        case IR_TYPE_F32:
            return 32;
        case IR_TYPE_I64:
        case IR_TYPE_U64:
        case IR_TYPE_F64:
            return 64;
        case IR_TYPE_PTR:
            return 64; // this is actually architecture dependent, TODO: determine based on target architecture?
        case IR_TYPE_ARRAY:
            return type->array.length * size_of_type(type->array.element);
        case IR_TYPE_STRUCT:
            // TODO
            assert(false && "Unimplemented");
            exit(1);
        default:
            return 0;
    }
}

bool ir_types_equal(const ir_type_t *a, const ir_type_t *b) {
    if (a == b) {
        return true;
    } else if (a == NULL || b == NULL) {
        return false;
    }

    if (a->kind != b->kind) {
        return false;
    }

    switch (a->kind) {
        case IR_TYPE_ARRAY: {
            if (a->array.length != b->array.length) {
                return false;
            }
            return ir_types_equal(a->array.element, b->array.element);
        }
        case IR_TYPE_FUNCTION: {
            // TODO
            if (!ir_types_equal(a->function.return_type, b->function.return_type)) {
                return false;
            }
            if (a->function.num_params != b->function.num_params) {
                return false;
            }
            for (size_t i = 0; i < a->function.num_params; i++) {
                if (!ir_types_equal(a->function.params[i], b->function.params[i])) {
                    return false;
                }
            }
            return true;
        }
        case IR_TYPE_PTR: {
            return ir_types_equal(a->ptr.pointee, b->ptr.pointee);
        }
        case IR_TYPE_STRUCT: {
            // TODO
            assert(false && "Unimplemented");
        }
        default:
            return true;
    }
}

const ir_type_t *ir_get_type_of_value(const ir_value_t value) {
    if (value.kind == IR_VALUE_VAR) {
        return value.var.type;
    } else {
        return value.constant.type;
    }
}

bool ir_is_integer_type(const ir_type_t *type) {
    switch (type->kind) {
        case IR_TYPE_BOOL:
        case IR_TYPE_I8:
        case IR_TYPE_I16:
        case IR_TYPE_I32:
        case IR_TYPE_I64:
        case IR_TYPE_U8:
        case IR_TYPE_U16:
        case IR_TYPE_U32:
        case IR_TYPE_U64:
            return true;
        default:
            return false;
    }
}

bool ir_is_signed_integer_type(const ir_type_t *type) {
    switch (type->kind) {
        case IR_TYPE_I8:
        case IR_TYPE_I16:
        case IR_TYPE_I32:
        case IR_TYPE_I64:
            return true;
        default:
            return false;
    }
}

bool ir_is_float_type(const ir_type_t *type) {
    return type->kind == IR_TYPE_F32 || type->kind == IR_TYPE_F64;
}

void append_ir_validation_error(ir_validation_error_vector_t *vector, ir_validation_error_t error) {
    VEC_APPEND(vector, error);
}

void ir_validate_3_way_type_match(ir_validation_error_vector_t *errors, const ir_instruction_t *instr, const ir_type_t *a, const ir_value_t b, const ir_value_t c) {
    bool matches = ir_types_equal(a, ir_get_type_of_value(b)) &&
            ir_types_equal(ir_get_type_of_value(b), ir_get_type_of_value(c));
    if (!matches) {
        append_ir_validation_error(errors, (ir_validation_error_t) {
            .instruction = instr,
            .message = "Type mismatch (result and operands must have the same type)"
        });
    }
}

void ir_validate_2_way_type_match(ir_validation_error_vector_t *errors, const ir_instruction_t *instr, const ir_type_t *a, const ir_value_t b) {
    if (!ir_types_equal(a, ir_get_type_of_value(b))) {
        append_ir_validation_error(errors, (ir_validation_error_t) {
            .instruction = instr,
            .message = "Type mismatch (result and value must have the same type)"
        });
    }
}

void ir_validate_visit_variable(hash_table_t *vars, ir_validation_error_vector_t *errors, const ir_instruction_t *instr, ir_var_t var) {
    const ir_type_t *existing_type;
    if (hash_table_lookup(vars, var.name, (void **) &existing_type)) {
        if (!ir_types_equal(existing_type, var.type)) {
            append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instr,
                    .message = "Variable redefined with different type"
            });
        }
    } else {
        hash_table_insert(vars, var.name, (void*) var.type);
    }
}

void ir_validate_visit_value(hash_table_t *vars, ir_validation_error_vector_t *errors, const ir_instruction_t *instr, ir_value_t value) {
    if (value.kind == IR_VALUE_VAR) {
        ir_validate_visit_variable(vars, errors, instr, value.var);
    }
}

void ir_validate_visit_instruction(
        const ir_function_definition_t *function,
        hash_table_t *variables,
        ir_validation_error_vector_t *errors,
        const ir_instruction_t *instruction
) {
    switch (instruction->opcode) {
        case IR_NOP:
            // No validation needed!
            break;
        case IR_ADD:
        case IR_SUB:
        case IR_MUL:
        case IR_DIV:
        case IR_MOD:
        case IR_AND:
        case IR_OR:
        case IR_SHL:
        case IR_SHR:
        case IR_XOR:
            // The result and operands must have the same type
            ir_validate_3_way_type_match(errors, instruction, instruction->binary_op.result.type, instruction->binary_op.left, instruction->binary_op.right);
            ir_validate_visit_variable(variables, errors, instruction, instruction->binary_op.result);
            ir_validate_visit_value(variables, errors, instruction, instruction->binary_op.left);
            ir_validate_visit_value(variables, errors, instruction, instruction->binary_op.right);
            break;
        case IR_ASSIGN:
            // The result and value must have the same type
            ir_validate_2_way_type_match(errors, instruction, instruction->assign.result.type, instruction->assign.value);
            ir_validate_visit_variable(variables, errors, instruction, instruction->assign.result);
            ir_validate_visit_value(variables, errors, instruction, instruction->assign.value);
            break;
        case IR_NOT:
            ir_validate_2_way_type_match(errors, instruction, instruction->unary_op.result.type, instruction->unary_op.operand);
            ir_validate_visit_variable(variables, errors, instruction, instruction->unary_op.result);
            ir_validate_visit_value(variables, errors, instruction, instruction->unary_op.operand);
            break;
        case IR_EQ:
        case IR_NE:
        case IR_LT:
        case IR_LE:
        case IR_GT:
        case IR_GE:
        {
            // The operands must have the same type, and the result is always a boolean
            if (!ir_types_equal(ir_get_type_of_value(instruction->binary_op.left), ir_get_type_of_value(instruction->binary_op.right))) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "Type mismatch (comparison operands must have the same type)"
                });
            }
            if (instruction->binary_op.result.type->kind != IR_TYPE_BOOL) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "Comparison result must be a boolean"
                });
            }
            ir_validate_visit_variable(variables, errors, instruction, instruction->binary_op.result);
            ir_validate_visit_value(variables, errors, instruction, instruction->binary_op.left);
            ir_validate_visit_value(variables, errors, instruction, instruction->binary_op.right);
            break;
        }
        case IR_BR:
            if (instruction->branch.label == NULL) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "Branch instruction must have a label"
                });
            }
            break;
        case IR_BR_COND:
            if (instruction->branch.label == NULL) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "Branch instruction must have a label"
                });
            }
            if (!instruction->branch.has_cond) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "Branch instruction must have a condition"
                });
            } else {
                ir_validate_visit_value(variables, errors, instruction, instruction->branch.cond);
                if (ir_get_type_of_value(instruction->branch.cond)->kind != IR_TYPE_BOOL) {
                    append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "Branch condition must be a boolean"
                    });
                }
            }
            break;
        case IR_CALL:
            // TODO: validate call instruction
            break;
        case IR_RET: {
            const ir_type_t *return_type = &IR_VOID;
            if (instruction->ret.has_value) {
                ir_validate_visit_value(variables, errors, instruction, instruction->ret.value);
                return_type = ir_get_type_of_value(instruction->ret.value);
            }
            if (!ir_types_equal(return_type, function->type->function.return_type)) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "Return value type does not match function return type"
                });
            }
            break;
        }
        case IR_ALLOCA:
            ir_validate_visit_variable(variables, errors, instruction, instruction->alloca.result);
            if (instruction->alloca.result.type->kind != IR_TYPE_PTR) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "alloca result must be a pointer"
                });
            }
            if (!ir_types_equal(instruction->alloca.result.type->ptr.pointee, instruction->alloca.type)) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "alloca result type does not match the type of the value being allocated"
                });
            }
            break;
        case IR_LOAD:
            ir_validate_visit_variable(variables, errors, instruction, instruction->unary_op.result);
            ir_validate_visit_value(variables, errors, instruction, instruction->unary_op.operand);
            if (ir_get_type_of_value(instruction->unary_op.operand)->kind != IR_TYPE_PTR) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "load value must be a pointer"
                });
            } else {
                if (!ir_types_equal(instruction->unary_op.result.type, ir_get_type_of_value(instruction->unary_op.operand)->ptr.pointee)) {
                    append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "load result type does not match the type of the value being loaded"
                    });
                }
            }
            break;
        case IR_STORE:
            ir_validate_visit_value(variables, errors, instruction, instruction->store.value);
            ir_validate_visit_value(variables, errors, instruction, instruction->store.ptr);
            if (ir_get_type_of_value(instruction->store.ptr)->kind != IR_TYPE_PTR) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "store pointer must be a pointer"
                });
            } else {
                if (!ir_types_equal(ir_get_type_of_value(instruction->store.ptr)->ptr.pointee, ir_get_type_of_value(instruction->store.value))) {
                    append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "store value type does not match the type of the pointer being stored to"
                    });
                }
            }
            break;
        case IR_MEMCPY: {
            ir_validate_visit_variable(variables, errors, instruction, instruction->unary_op.result);
            ir_validate_visit_value(variables, errors, instruction, instruction->unary_op.operand);
            // Result must be an array, struct, or pointer
            if (instruction->unary_op.result.type->kind != IR_TYPE_PTR &&
                instruction->unary_op.result.type->kind != IR_TYPE_ARRAY &&
                instruction->unary_op.result.type->kind != IR_TYPE_STRUCT
            ) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "memcpy result must be an array, pointer, or struct"
                });
            }
            break;
        }
        case IR_TRUNC: {
            // The result type must be smaller than the value being truncated
            // Both the result and the value must be integers, or both must be floating point numbers
            ir_validate_visit_variable(variables, errors, instruction, instruction->unary_op.result);
            ir_validate_visit_value(variables, errors, instruction, instruction->unary_op.operand);
            const ir_type_t *result_type = instruction->unary_op.result.type;
            const ir_type_t *value_type = ir_get_type_of_value(instruction->unary_op.operand);
            if (ir_is_integer_type(result_type) && !ir_is_integer_type(value_type)) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "Truncation result and value must both be integers, or both must be floating point numbers"
                });
            } else if (ir_is_float_type(result_type) && !ir_is_float_type(value_type)) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "Truncation result and value must both be integers, or both must be floating point numbers"
                });
            } else if (!ir_is_integer_type(result_type) && !ir_is_float_type(result_type)) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "Truncation result and operand types must be integer or floating point numbers"
                });
            }
            if (size_of_type(result_type) >= size_of_type(value_type)) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "Truncation result type must be smaller than the value being truncated"
                });
            }
            break;
        }
        case IR_EXT:
            // The result type must be larger than the value being extended
            // Both the result and the value must be integers, or both must be floating point numbers
            ir_validate_visit_variable(variables, errors, instruction, instruction->unary_op.result);
            ir_validate_visit_value(variables, errors, instruction, instruction->unary_op.operand);
            const ir_type_t *result_type = instruction->unary_op.result.type;
            const ir_type_t *value_type = ir_get_type_of_value(instruction->unary_op.operand);
            if (ir_is_integer_type(result_type) && !ir_is_integer_type(value_type)) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "Extension result and value must both be integers, or both must be floating point numbers"
                });
            } else if (ir_is_float_type(result_type) && !ir_is_float_type(value_type)) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "Extension result and value must both be integers, or both must be floating point numbers"
                });
            } else if (!ir_is_integer_type(result_type) && !ir_is_float_type(result_type)) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "Extension result and operand types must be integer or floating point numbers"
                });
            }
            if (size_of_type(result_type) <= size_of_type(value_type)) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "Extension result type must be larger than the value being extended"
                });
            }
            break;
        case IR_FTOI:
            // The result must be an integer, and the operand must be a floating point number
            ir_validate_visit_variable(variables, errors, instruction, instruction->unary_op.result);
            ir_validate_visit_value(variables, errors, instruction, instruction->unary_op.operand);
            if (!ir_is_integer_type(instruction->unary_op.result.type)) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "ftoi result must be an integer"
                });
            }
            if (!ir_is_float_type(ir_get_type_of_value(instruction->unary_op.operand))) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "ftoi operand must be a floating point number"
                });
            }
            break;
        case IR_ITOF:
            // The result must be a floating point number, and the operand must be an integer
            ir_validate_visit_variable(variables, errors, instruction, instruction->unary_op.result);
            ir_validate_visit_value(variables, errors, instruction, instruction->unary_op.operand);
            if (!ir_is_float_type(instruction->unary_op.result.type)) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "itof result must be a floating point number"
                });
            }
            if (!ir_is_integer_type(ir_get_type_of_value(instruction->unary_op.operand))) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "itof operand must be an integer"
                });
            }
            break;
        case IR_PTOI:
            // The result must be an integer, and the operand must be a pointer
            ir_validate_visit_variable(variables, errors, instruction, instruction->unary_op.result);
            ir_validate_visit_value(variables, errors, instruction, instruction->unary_op.operand);
            if (!ir_is_integer_type(instruction->unary_op.result.type)) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "ptoi result must be an integer"
                });
            }
            if (ir_get_type_of_value(instruction->unary_op.operand)->kind != IR_TYPE_PTR) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "ptoi operand must be a pointer"
                });
            }
            break;
        case IR_ITOP:
            // The result must be a pointer, and the operand must be an integer
            ir_validate_visit_variable(variables, errors, instruction, instruction->unary_op.result);
            ir_validate_visit_value(variables, errors, instruction, instruction->unary_op.operand);
            if (instruction->unary_op.result.type->kind != IR_TYPE_PTR) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "itop result must be a pointer"
                });
            }
            if (!ir_is_integer_type(ir_get_type_of_value(instruction->unary_op.operand))) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "itop operand must be an integer"
                });
            }
            break;
        case IR_BITCAST:
            // TODO: validate bitcast instruction
            break;
        default:
            append_ir_validation_error(errors, (ir_validation_error_t) {
                .instruction = instruction,
                .message = "Invalid opcode value"
            });
            break;
    }
}

ir_validation_error_vector_t ir_validate_function(const ir_function_definition_t *function) {
    ir_validation_error_vector_t errors = { .buffer = NULL, .size = 0, .capacity = 0 };
    hash_table_t labels = hash_table_create_string_keys(64);
    hash_table_t variables = hash_table_create_string_keys(128);

    // First pass:
    // - Record all labels, and check for duplicates
    // - Verify that no variable is re-defined with a different type
    // - Validate that each instruction is well-formed
    for (size_t i = 0; i < function->body.size; i++) {
        const ir_instruction_t *instr = &function->body.buffer[i];
        if (instr->label != NULL) {
            if (hash_table_lookup(&labels, instr->label, NULL)) {
                append_ir_validation_error(&errors, (ir_validation_error_t) {
                    .instruction = instr,
                    .message = "Duplicate label"
                });
            }
            hash_table_insert(&labels, instr->label, (void*) instr);
        }
        ir_validate_visit_instruction(function, &variables, &errors, instr);
    }

    // Second pass: Check that all branch targets are valid
    for (size_t i = 0; i < function->body.size; i += 1) {
        const ir_instruction_t *instr = &function->body.buffer[i];
        const char *label = NULL;
        switch (instr->opcode) {
            case IR_BR:
            case IR_BR_COND:
                label = instr->branch.label;
                break;
            default:
                break;
        }
        if (label != NULL && !hash_table_lookup(&labels, label, NULL)) {
            append_ir_validation_error(&errors, (ir_validation_error_t) {
                .instruction = instr,
                .message = "Invalid branch target"
            });
        }
    }

    hash_table_destroy(&labels);
    hash_table_destroy(&variables);

    // There are some additional checks that could be performed if provided with a control flow graph:
    // * verify that all variables are defined before use
    // * verify that all paths return a value (if the function returns a value)

    return errors;
}

size_t ir_get_uses(ir_instruction_t *instr, ir_var_t **uses, size_t uses_max) {
    size_t count = 0;
    assert(uses_max >= 2 && "Output array must be able to store at least 2 variables");
    switch (instr->opcode) {
        case IR_NOP:
            break;
        case IR_ADD:
        case IR_SUB:
        case IR_MUL:
        case IR_DIV:
        case IR_MOD:
        case IR_AND:
        case IR_OR:
        case IR_SHL:
        case IR_SHR:
        case IR_XOR:
        case IR_EQ:
        case IR_NE:
        case IR_LT:
        case IR_LE:
        case IR_GT:
        case IR_GE:
            if (instr->binary_op.left.kind == IR_VALUE_VAR) uses[count++] = &instr->binary_op.left.var;
            if (instr->binary_op.right.kind == IR_VALUE_VAR) uses[count++] = &instr->binary_op.right.var;
            break;
        case IR_ASSIGN:
            if (instr->assign.value.kind == IR_VALUE_VAR) uses[count++] = &instr->assign.value.var;
            break;
        case IR_BR:
        case IR_BR_COND:
            if (instr->branch.has_cond && instr->branch.cond.kind == IR_VALUE_VAR) uses[count++] = &instr->branch.cond.var;
            break;
        case IR_CALL:
            uses[count++] = &instr->call.function;
            for (int i = 0; i < instr->call.num_args; i += 1) {
                assert(count < uses_max); // TODO
                if (instr->call.args[i].kind == IR_VALUE_VAR) uses[count++] = &instr->call.args[i].var;
            }
            break;
        case IR_RET:
            if (instr->ret.has_value && instr->ret.value.kind == IR_VALUE_VAR) uses[count++] = &instr->ret.value.var;
            break;
        case IR_ALLOCA:
            break;
        case IR_STORE:
            if (instr->store.value.kind == IR_VALUE_VAR) uses[count++] = &instr->store.value.var;
            if (instr->store.ptr.kind == IR_VALUE_VAR) uses[count++] = &instr->store.ptr.var;
            break;
        case IR_LOAD:
        case IR_NOT:
        case IR_MEMCPY:
        case IR_TRUNC:
        case IR_EXT:
        case IR_FTOI:
        case IR_ITOF:
        case IR_PTOI:
        case IR_ITOP:
        case IR_BITCAST:
            if (instr->unary_op.operand.kind == IR_VALUE_VAR) uses[count++] = &instr->unary_op.operand.var;
            break;
    }
    return count;
}

ir_var_t *ir_get_def(ir_instruction_t *instr) {
    switch (instr->opcode) {
        case IR_NOP:
            break;
        case IR_ADD:
        case IR_SUB:
        case IR_MUL:
        case IR_DIV:
        case IR_MOD:
        case IR_AND:
        case IR_OR:
        case IR_SHL:
        case IR_SHR:
        case IR_XOR:
        case IR_EQ:
        case IR_NE:
        case IR_LT:
        case IR_LE:
        case IR_GT:
        case IR_GE:
            return &instr->binary_op.result;
        case IR_ASSIGN:
            return &instr->assign.result;
        case IR_BR:
        case IR_BR_COND:
            break;
        case IR_CALL:
            if (instr->call.result != NULL) return instr->call.result;
            break;
        case IR_RET:
            break;
        case IR_ALLOCA:
            return &instr->alloca.result;
        case IR_STORE:
            break;
        case IR_LOAD:
        case IR_NOT:
        case IR_MEMCPY:
        case IR_TRUNC:
        case IR_EXT:
        case IR_FTOI:
        case IR_ITOF:
        case IR_PTOI:
        case IR_ITOP:
        case IR_BITCAST:
            return &instr->unary_op.result;
    }
    return NULL;
}