#include <string.h>
#include "ir/ir.h"

void append_ir_instruction(ir_instruction_vector_t *vector, ir_instruction_t instruction) {
    if (vector->size == vector->capacity) {
        vector->capacity = vector->capacity * 2 + 1;
        vector->buffer = realloc(vector->buffer, vector->capacity * sizeof(ir_instruction_t));
    }
    vector->buffer[vector->size++] = instruction;
}

/**
 * Size of a type in bits
 * @param arch target architecture
 * @param type ir type
 * @return size of the type in bits
 */
ssize_t ir_size_of_type_bits(const ir_arch_t *arch, const ir_type_t *type) {
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
            return ir_size_of_type_bits(arch, arch->ptr_int_type);
        case IR_TYPE_ARRAY:
            return type->array.length * ir_size_of_type_bits(arch, type->array.element);
        case IR_TYPE_STRUCT_OR_UNION: {
            if (type->struct_or_union.is_union) {
                // If the type is a union, then the size is the size of the largest field
                int max = 0;
                for (int i = 0; i < type->struct_or_union.fields.size; i += 1) {
                    int size = ir_size_of_type_bytes(arch, type->struct_or_union.fields.buffer[i]->type);
                    if (size > max) max = size;
                }
                return max * BYTE_SIZE;
            }

            int size_bytes = 0;
            for (int i = 0; i < type->struct_or_union.fields.size; i += 1) {
                size_bytes += ir_size_of_type_bytes(arch, type->struct_or_union.fields.buffer[i]->type);
            }
            return size_bytes * BYTE_SIZE;
        }
        default:
            return 0;
    }
}

ssize_t ir_size_of_type_bytes(const ir_arch_t *arch, const ir_type_t *type) {
    return (ir_size_of_type_bits(arch, type) + BYTE_SIZE - 1) / BYTE_SIZE;
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
        case IR_TYPE_STRUCT_OR_UNION: {
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

bool ir_is_scalar_type(const ir_type_t *type) {
    return ir_is_integer_type(type) || ir_is_float_type(type) || type->kind == IR_TYPE_PTR;
}

int ir_get_alignment(const ir_arch_t *arch, const ir_type_t *type) {
    switch (type->kind) {
    case IR_TYPE_VOID:
    case IR_TYPE_BOOL:
    case IR_TYPE_U8:
    case IR_TYPE_I8:
        return arch->int8_alignment;
    case IR_TYPE_U16:
    case IR_TYPE_I16:
        return arch->int16_alignment;
    case IR_TYPE_U32:
    case IR_TYPE_I32:
        return arch->int32_alignment;
    case IR_TYPE_I64:
    case IR_TYPE_U64:
        return arch->int64_alignment;
    case IR_TYPE_F32:
        return arch->f32_alignment;
    case IR_TYPE_F64:
        return arch->f64_alignment;
    case IR_TYPE_PTR:
        return ir_get_alignment(arch, arch->ptr_int_type);
    case IR_TYPE_ARRAY:
        return ir_get_alignment(arch, type->array.element);
    case IR_TYPE_STRUCT_OR_UNION:
        if (type->struct_or_union.fields.size == 0) return arch->int8_alignment;
        return ir_get_alignment(arch, type->struct_or_union.fields.buffer[0]->type);
    case IR_TYPE_FUNCTION:
        return 1; // this shouldn't be reachable, you would only allocate a pointer to a function in c
    }
}

ir_type_struct_t ir_pad_struct(const ir_arch_t *arch, const ir_type_struct_t *source) {
    assert(source != NULL && !source->is_union);
    ir_type_struct_t result = {
        .id = source->id,
        .field_map = hash_table_create_string_keys(64),
        .fields = VEC_INIT,
        .is_union = source->is_union,
    };

    int pad_field_id = 0;
    int offset = 0;
    int result_field_index = 0;
    for (int source_field_index = 0; source_field_index < source->fields.size; source_field_index += 1) {
        // add padding before the field if the current offset is not divisible by the alignment requirement of
        // the fields type
        // no padding should be added before the first field
        const ir_struct_field_t *source_field = source->fields.buffer[source_field_index];
        const int alignment = ir_get_alignment(arch, source_field->type);
        if (offset % alignment != 0) {
            // add padding before the field
            int pad_bytes = alignment - (offset % alignment);
            ir_type_t *pad_type = malloc(sizeof(ir_type_t));
            *pad_type = (ir_type_t) {
                .kind = IR_TYPE_ARRAY,
                .array = {
                    .element = &IR_U8,
                    .length = pad_bytes,
                },
            };
            ir_struct_field_t *padding = malloc(sizeof(ir_struct_field_t));
            char name_buf[64];
            int name_len = snprintf(name_buf, 64, "__padding_%d", pad_field_id++) + 1;
            *padding = (ir_struct_field_t) {
                .index = result_field_index++,
                .name = memcpy(malloc(name_len), name_buf, name_len),
                .type = pad_type,
            };
            VEC_APPEND(&result.fields, padding);
            hash_table_insert(&result.field_map, padding->name, padding);
            offset += pad_bytes;
        }
        assert(offset % alignment == 0);
        ir_struct_field_t *field = malloc(sizeof(ir_struct_field_t));
        *field = (ir_struct_field_t) {
            .index = result_field_index++,
            .name = source_field->name,
            .type = source_field->type,
        };
        VEC_APPEND(&result.fields, field);
        hash_table_insert(&result.field_map, field->name, field);
        offset += ir_size_of_type_bytes(arch, field->type);
    }

    return result;
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
        const ir_module_t *module,
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
                instruction->unary_op.result.type->kind != IR_TYPE_STRUCT_OR_UNION
            ) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "memcpy result must be an array, pointer, or struct"
                });
            }
            break;
        }
        case IR_GET_ARRAY_ELEMENT_PTR: {
            ir_validate_visit_value(variables, errors, instruction, instruction->binary_op.left);
            ir_validate_visit_value(variables, errors, instruction, instruction->binary_op.right);
            ir_validate_visit_variable(variables, errors, instruction, instruction->binary_op.result);
            // The left operand must be a pointer, and the right operand must be an integer
            if (ir_get_type_of_value(instruction->binary_op.left)->kind != IR_TYPE_PTR) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "get_array_element_ptr left operand must be a pointer"
                });
            }
            if (!ir_is_integer_type(ir_get_type_of_value(instruction->binary_op.right))) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "get_array_element_ptr right operand must be an integer"
                });
            }
            // The result must be a pointer to the element type of the array
            if (instruction->binary_op.result.type->kind != IR_TYPE_PTR) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "get_array_element_ptr result must be a pointer"
                });
            } else {
                const ir_type_t *element_type = ir_get_type_of_value(instruction->binary_op.left)->ptr.pointee;
                if (element_type->kind == IR_TYPE_ARRAY) {
                    element_type = element_type->array.element;
                }
                if (!ir_types_equal(instruction->binary_op.result.type->ptr.pointee, element_type)) {
                    append_ir_validation_error(errors, (ir_validation_error_t) {
                            .instruction = instruction,
                            .message = "get_array_element_ptr result type does not match the element type of the source array"
                    });
                }
            }
            break;
        }
        case IR_GET_STRUCT_MEMBER_PTR: {
            ir_validate_visit_value(variables, errors, instruction, instruction->binary_op.left);
            ir_validate_visit_value(variables, errors, instruction, instruction->binary_op.right);
            ir_validate_visit_variable(variables, errors, instruction, instruction->binary_op.result);
            // The left operand must be a pointer to a struct or union
            if (ir_get_type_of_value(instruction->binary_op.left)->kind != IR_TYPE_PTR ||
                ir_get_type_of_value(instruction->binary_op.left)->ptr.pointee->kind != IR_TYPE_STRUCT_OR_UNION) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "get_struct_member_ptr left operand must be a pointer to a struct or union"
                });
                break;
            }

            const ir_type_t *struct_type = ir_get_type_of_value(instruction->binary_op.left)->ptr.pointee;

            // The right operand must be a constant integer (field index)
            if (instruction->binary_op.right.kind != IR_VALUE_CONST || instruction->binary_op.right.constant.kind != IR_CONST_INT) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "get_struct_member_ptr right operand (field index) must be a constant int"
                });
                break;
            }

            // The field index must be in range
            int index = instruction->binary_op.right.constant.i;
            if (struct_type->struct_or_union.fields.size <= index) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "get_struct_member_ptr right operand (field index) does not reference field in the struct type"
                });
                break;
            }

            // The result must be a pointer to the field type
            const ir_struct_field_t *field = struct_type->struct_or_union.fields.buffer[index];
            if (instruction->binary_op.result.type->kind != IR_TYPE_PTR ||
                !ir_types_equal(field->type, instruction->binary_op.result.type->ptr.pointee)) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "get_struct_member_ptr result type must be a pointer with a base type which matches the field type"
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
            if (ir_size_of_type_bits(module->arch, result_type) >= ir_size_of_type_bits(module->arch, value_type)) {
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
            if (ir_size_of_type_bits(module->arch, result_type) <= ir_size_of_type_bits(module->arch, value_type)) {
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

ir_validation_error_vector_t ir_validate_function(const ir_module_t *module, const ir_function_definition_t *function) {
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
        ir_validate_visit_instruction(module, function, &variables, &errors, instr);
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
    assert(uses_max >= 3 && "Output array must be able to store at least 3 variables");
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
        case IR_GET_ARRAY_ELEMENT_PTR:
        case IR_GET_STRUCT_MEMBER_PTR:
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
        case IR_ALLOCA: break;
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
        case IR_MEMSET:
            if (instr->memset.ptr.kind == IR_VALUE_VAR) uses[count++] = &instr->memset.ptr.var;
            if (instr->memset.value.kind == IR_VALUE_VAR) uses[count++] = &instr->memset.value.var;
            if (instr->memset.length.kind == IR_VALUE_VAR) uses[count++] = &instr->memset.length.var;
    }
    return count;
}

ir_var_t *ir_get_def(ir_instruction_t *instr) {
    switch (instr->opcode) {
        case IR_NOP: break;
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
        case IR_GET_ARRAY_ELEMENT_PTR:
        case IR_GET_STRUCT_MEMBER_PTR:
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
        case IR_STORE: break;
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
        case IR_MEMSET: break;
    }
    return NULL;
}
