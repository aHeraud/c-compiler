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
            return type->value.array.length * ir_size_of_type_bits(arch, type->value.array.element);
        case IR_TYPE_STRUCT_OR_UNION: {
            // If the type is a union, then the size is the size of the largest field
            // Otherwise, the size is the sum of all fields
            int sum_bytes = 0;
            int max_bytes = 0;
            for (int i = 0; i < type->value.struct_or_union.fields.size; i += 1) {
                const ir_type_t *field_type = type->value.struct_or_union.fields.buffer[i]->type;
                assert(type != field_type);
                int size_bytes = ir_size_of_type_bytes(arch, field_type);
                sum_bytes += size_bytes;
                if (size_bytes > max_bytes) max_bytes = size_bytes;
            }

            return (type->value.struct_or_union.is_union ? max_bytes : sum_bytes) * BYTE_SIZE;
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
            if (a->value.array.length != b->value.array.length) {
                return false;
            }
            return ir_types_equal(a->value.array.element, b->value.array.element);
        }
        case IR_TYPE_FUNCTION: {
            // TODO
            if (!ir_types_equal(a->value.function.return_type, b->value.function.return_type)) {
                return false;
            }
            if (a->value.function.num_params != b->value.function.num_params) {
                return false;
            }
            for (size_t i = 0; i < a->value.function.num_params; i++) {
                if (!ir_types_equal(a->value.function.params[i], b->value.function.params[i])) {
                    return false;
                }
            }
            return true;
        }
        case IR_TYPE_PTR: {
            return ir_types_equal(a->value.ptr.pointee, b->value.ptr.pointee);
        }
        case IR_TYPE_STRUCT_OR_UNION: {
            return strcmp(a->value.struct_or_union.id, b->value.struct_or_union.id) == 0;
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
        return ir_get_alignment(arch, type->value.array.element);
    case IR_TYPE_STRUCT_OR_UNION:
        if (type->value.struct_or_union.fields.size == 0) return arch->int8_alignment;
        return ir_get_alignment(arch, type->value.struct_or_union.fields.buffer[0]->type);
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
                .value.array = {
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
            ir_validate_3_way_type_match(errors, instruction, instruction->value.binary_op.result.type, instruction->value.binary_op.left, instruction->value.binary_op.right);
            ir_validate_visit_variable(variables, errors, instruction, instruction->value.binary_op.result);
            ir_validate_visit_value(variables, errors, instruction, instruction->value.binary_op.left);
            ir_validate_visit_value(variables, errors, instruction, instruction->value.binary_op.right);
            break;
        case IR_ASSIGN:
            // The result and value must have the same type
            ir_validate_2_way_type_match(errors, instruction, instruction->value.assign.result.type, instruction->value.assign.value);
            ir_validate_visit_variable(variables, errors, instruction, instruction->value.assign.result);
            ir_validate_visit_value(variables, errors, instruction, instruction->value.assign.value);
            break;
        case IR_NOT:
            ir_validate_2_way_type_match(errors, instruction, instruction->value.unary_op.result.type, instruction->value.unary_op.operand);
            ir_validate_visit_variable(variables, errors, instruction, instruction->value.unary_op.result);
            ir_validate_visit_value(variables, errors, instruction, instruction->value.unary_op.operand);
            break;
        case IR_EQ:
        case IR_NE:
        case IR_LT:
        case IR_LE:
        case IR_GT:
        case IR_GE:
        {
            // The operands must have the same type, and the result is always a boolean
            if (!ir_types_equal(ir_get_type_of_value(instruction->value.binary_op.left), ir_get_type_of_value(instruction->value.binary_op.right))) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "Type mismatch (comparison operands must have the same type)"
                });
            }
            if (instruction->value.binary_op.result.type->kind != IR_TYPE_BOOL) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "Comparison result must be a boolean"
                });
            }
            ir_validate_visit_variable(variables, errors, instruction, instruction->value.binary_op.result);
            ir_validate_visit_value(variables, errors, instruction, instruction->value.binary_op.left);
            ir_validate_visit_value(variables, errors, instruction, instruction->value.binary_op.right);
            break;
        }
        case IR_BR:
            if (instruction->value.branch.label == NULL) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "Branch instruction must have a label"
                });
            }
            break;
        case IR_BR_COND:
            if (instruction->value.branch.label == NULL) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "Branch instruction must have a label"
                });
            }
            if (!instruction->value.branch.has_cond) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "Branch instruction must have a condition"
                });
            } else {
                ir_validate_visit_value(variables, errors, instruction, instruction->value.branch.cond);
                if (ir_get_type_of_value(instruction->value.branch.cond)->kind != IR_TYPE_BOOL) {
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
            if (instruction->value.ret.has_value) {
                ir_validate_visit_value(variables, errors, instruction, instruction->value.ret.value);
                return_type = ir_get_type_of_value(instruction->value.ret.value);
            }
            if (!ir_types_equal(return_type, function->type->value.function.return_type)) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "Return value type does not match function return type"
                });
            }
            break;
        }
        case IR_ALLOCA:
            ir_validate_visit_variable(variables, errors, instruction, instruction->value.alloca.result);
            if (instruction->value.alloca.result.type->kind != IR_TYPE_PTR) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "alloca result must be a pointer"
                });
            }
            if (!ir_types_equal(instruction->value.alloca.result.type->value.ptr.pointee, instruction->value.alloca.type)) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "alloca result type does not match the type of the value being allocated"
                });
            }
            break;
        case IR_LOAD:
            ir_validate_visit_variable(variables, errors, instruction, instruction->value.unary_op.result);
            ir_validate_visit_value(variables, errors, instruction, instruction->value.unary_op.operand);
            if (ir_get_type_of_value(instruction->value.unary_op.operand)->kind != IR_TYPE_PTR) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "load value must be a pointer"
                });
            } else {
                if (!ir_types_equal(instruction->value.unary_op.result.type, ir_get_type_of_value(instruction->value.unary_op.operand)->value.ptr.pointee)) {
                    append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "load result type does not match the type of the value being loaded"
                    });
                }
            }
            break;
        case IR_STORE:
            ir_validate_visit_value(variables, errors, instruction, instruction->value.store.value);
            ir_validate_visit_value(variables, errors, instruction, instruction->value.store.ptr);
            if (ir_get_type_of_value(instruction->value.store.ptr)->kind != IR_TYPE_PTR) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "store pointer must be a pointer"
                });
            } else {
                if (!ir_types_equal(ir_get_type_of_value(instruction->value.store.ptr)->value.ptr.pointee, ir_get_type_of_value(instruction->value.store.value))) {
                    append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "store value type does not match the type of the pointer being stored to"
                    });
                }
            }
            break;
        case IR_MEMCPY: {
            ir_validate_visit_value(variables, errors, instruction, instruction->value.memcpy.src);
            ir_validate_visit_value(variables, errors, instruction, instruction->value.memcpy.dest);
            ir_validate_visit_value(variables, errors, instruction, instruction->value.memcpy.length);
            // Result must be an array or pointer
            if (ir_get_type_of_value(instruction->value.memcpy.dest)->kind != IR_TYPE_PTR &&
                ir_get_type_of_value(instruction->value.memcpy.dest)->kind != IR_TYPE_ARRAY
            ) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "memcpy destination must be an array or pointer"
                });
            }
            // Source must be an array or pointer
            if (ir_get_type_of_value(instruction->value.memcpy.src)->kind != IR_TYPE_PTR &&
                ir_get_type_of_value(instruction->value.memcpy.src)->kind != IR_TYPE_ARRAY
                    ) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "memcpy source must be an array or pointer"
                });
            }
            break;
        }
        case IR_GET_ARRAY_ELEMENT_PTR: {
            ir_validate_visit_value(variables, errors, instruction, instruction->value.binary_op.left);
            ir_validate_visit_value(variables, errors, instruction, instruction->value.binary_op.right);
            ir_validate_visit_variable(variables, errors, instruction, instruction->value.binary_op.result);
            // The left operand must be a pointer, and the right operand must be an integer
            if (ir_get_type_of_value(instruction->value.binary_op.left)->kind != IR_TYPE_PTR) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "get_array_element_ptr left operand must be a pointer"
                });
            }
            if (!ir_is_integer_type(ir_get_type_of_value(instruction->value.binary_op.right))) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "get_array_element_ptr right operand must be an integer"
                });
            }
            // The result must be a pointer to the element type of the array
            if (instruction->value.binary_op.result.type->kind != IR_TYPE_PTR) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "get_array_element_ptr result must be a pointer"
                });
            } else {
                const ir_type_t *element_type = ir_get_type_of_value(instruction->value.binary_op.left)->value.ptr.pointee;
                if (element_type->kind == IR_TYPE_ARRAY) {
                    element_type = element_type->value.array.element;
                }
                if (!ir_types_equal(instruction->value.binary_op.result.type->value.ptr.pointee, element_type)) {
                    append_ir_validation_error(errors, (ir_validation_error_t) {
                            .instruction = instruction,
                            .message = "get_array_element_ptr result type does not match the element type of the source array"
                    });
                }
            }
            break;
        }
        case IR_GET_STRUCT_MEMBER_PTR: {
            ir_validate_visit_value(variables, errors, instruction, instruction->value.binary_op.left);
            ir_validate_visit_value(variables, errors, instruction, instruction->value.binary_op.right);
            ir_validate_visit_variable(variables, errors, instruction, instruction->value.binary_op.result);
            // The left operand must be a pointer to a struct or union
            if (ir_get_type_of_value(instruction->value.binary_op.left)->kind != IR_TYPE_PTR ||
                ir_get_type_of_value(instruction->value.binary_op.left)->value.ptr.pointee->kind != IR_TYPE_STRUCT_OR_UNION) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "get_struct_member_ptr left operand must be a pointer to a struct or union"
                });
                break;
            }

            const ir_type_t *struct_type = ir_get_type_of_value(instruction->value.binary_op.left)->value.ptr.pointee;

            // The right operand must be a constant integer (field index)
            if (instruction->value.binary_op.right.kind != IR_VALUE_CONST || instruction->value.binary_op.right.constant.kind != IR_CONST_INT) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "get_struct_member_ptr right operand (field index) must be a constant int"
                });
                break;
            }

            // The field index must be in range
            int index = instruction->value.binary_op.right.constant.value.i;
            if (struct_type->value.struct_or_union.fields.size <= index) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "get_struct_member_ptr right operand (field index) does not reference field in the struct type"
                });
                break;
            }

            // The result must be a pointer to the field type
            const ir_struct_field_t *field = struct_type->value.struct_or_union.fields.buffer[index];
            if (instruction->value.binary_op.result.type->kind != IR_TYPE_PTR ||
                !ir_types_equal(field->type, instruction->value.binary_op.result.type->value.ptr.pointee)) {
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
            ir_validate_visit_variable(variables, errors, instruction, instruction->value.unary_op.result);
            ir_validate_visit_value(variables, errors, instruction, instruction->value.unary_op.operand);
            const ir_type_t *result_type = instruction->value.unary_op.result.type;
            const ir_type_t *value_type = ir_get_type_of_value(instruction->value.unary_op.operand);
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
            ir_validate_visit_variable(variables, errors, instruction, instruction->value.unary_op.result);
            ir_validate_visit_value(variables, errors, instruction, instruction->value.unary_op.operand);
            const ir_type_t *result_type = instruction->value.unary_op.result.type;
            const ir_type_t *value_type = ir_get_type_of_value(instruction->value.unary_op.operand);
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
            ir_validate_visit_variable(variables, errors, instruction, instruction->value.unary_op.result);
            ir_validate_visit_value(variables, errors, instruction, instruction->value.unary_op.operand);
            if (!ir_is_integer_type(instruction->value.unary_op.result.type)) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "ftoi result must be an integer"
                });
            }
            if (!ir_is_float_type(ir_get_type_of_value(instruction->value.unary_op.operand))) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "ftoi operand must be a floating point number"
                });
            }
            break;
        case IR_ITOF:
            // The result must be a floating point number, and the operand must be an integer
            ir_validate_visit_variable(variables, errors, instruction, instruction->value.unary_op.result);
            ir_validate_visit_value(variables, errors, instruction, instruction->value.unary_op.operand);
            if (!ir_is_float_type(instruction->value.unary_op.result.type)) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "itof result must be a floating point number"
                });
            }
            if (!ir_is_integer_type(ir_get_type_of_value(instruction->value.unary_op.operand))) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "itof operand must be an integer"
                });
            }
            break;
        case IR_PTOI:
            // The result must be an integer, and the operand must be a pointer
            ir_validate_visit_variable(variables, errors, instruction, instruction->value.unary_op.result);
            ir_validate_visit_value(variables, errors, instruction, instruction->value.unary_op.operand);
            if (!ir_is_integer_type(instruction->value.unary_op.result.type)) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "ptoi result must be an integer"
                });
            }
            if (ir_get_type_of_value(instruction->value.unary_op.operand)->kind != IR_TYPE_PTR) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "ptoi operand must be a pointer"
                });
            }
            break;
        case IR_ITOP:
            // The result must be a pointer, and the operand must be an integer
            ir_validate_visit_variable(variables, errors, instruction, instruction->value.unary_op.result);
            ir_validate_visit_value(variables, errors, instruction, instruction->value.unary_op.operand);
            if (instruction->value.unary_op.result.type->kind != IR_TYPE_PTR) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "itop result must be a pointer"
                });
            }
            if (!ir_is_integer_type(ir_get_type_of_value(instruction->value.unary_op.operand))) {
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "itop operand must be an integer"
                });
            }
            break;
        case IR_BITCAST:
            // TODO: validate bitcast instruction
            break;
        case IR_SWITCH:
            // we will validate that the labels are valid later
            // for now just make sure that there is a default label
            if (instruction->value.switch_.default_label == NULL)
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "switch instruction must have a default label"
                });
            if (!ir_is_integer_type(ir_get_type_of_value(instruction->value.switch_.value)))
                append_ir_validation_error(errors, (ir_validation_error_t) {
                    .instruction = instruction,
                    .message = "switch expression must have integer value",
                });
            for (int i = 0; i < instruction->value.switch_.cases.size; i += 1) {
                ir_switch_case_t switch_case = instruction->value.switch_.cases.buffer[i];
                if (!ir_is_integer_type(switch_case.const_val.type)) {
                    append_ir_validation_error(errors, (ir_validation_error_t) {
                        .instruction = instruction,
                        .message = "switch case expression must have integer type",
                    });
                }
                // TODO: verify that there are no duplicate cases
            }
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
                label = instr->value.branch.label;
                break;
            case IR_SWITCH: {
                label = instr->value.switch_.default_label;
                for (int j = 0; j < instr->value.switch_.cases.size; j += 1) {
                    const char *case_label = instr->value.switch_.cases.buffer[j].label;
                    if (label == NULL) {
                        append_ir_validation_error(&errors, (ir_validation_error_t) {
                            .instruction = instr,
                            .message = "Missing label in switch case",
                        });
                    } else if (!hash_table_lookup(&labels, case_label, NULL)) {
                        append_ir_validation_error(&errors, (ir_validation_error_t) {
                            .instruction = instr,
                            .message = "Invalid switch case target label",
                        });
                    }
                }
            }
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
            if (instr->value.binary_op.left.kind == IR_VALUE_VAR) uses[count++] = &instr->value.binary_op.left.var;
            if (instr->value.binary_op.right.kind == IR_VALUE_VAR) uses[count++] = &instr->value.binary_op.right.var;
            break;
        case IR_ASSIGN:
            if (instr->value.assign.value.kind == IR_VALUE_VAR) uses[count++] = &instr->value.assign.value.var;
            break;
        case IR_BR:
        case IR_BR_COND:
            if (instr->value.branch.has_cond && instr->value.branch.cond.kind == IR_VALUE_VAR) uses[count++] = &instr->value.branch.cond.var;
            break;
        case IR_CALL:
            if (instr->value.call.function.kind == IR_VALUE_VAR) uses[count++] = &instr->value.call.function.var;
            for (int i = 0; i < instr->value.call.num_args; i += 1) {
                assert(count < uses_max); // TODO
                if (instr->value.call.args[i].kind == IR_VALUE_VAR) uses[count++] = &instr->value.call.args[i].var;
            }
            break;
        case IR_RET:
            if (instr->value.ret.has_value && instr->value.ret.value.kind == IR_VALUE_VAR) uses[count++] = &instr->value.ret.value.var;
            break;
        case IR_ALLOCA: break;
        case IR_STORE:
            if (instr->value.store.value.kind == IR_VALUE_VAR) uses[count++] = &instr->value.store.value.var;
            if (instr->value.store.ptr.kind == IR_VALUE_VAR) uses[count++] = &instr->value.store.ptr.var;
            break;
        case IR_LOAD:
        case IR_NOT:
        case IR_TRUNC:
        case IR_EXT:
        case IR_FTOI:
        case IR_ITOF:
        case IR_PTOI:
        case IR_ITOP:
        case IR_BITCAST:
            if (instr->value.unary_op.operand.kind == IR_VALUE_VAR) uses[count++] = &instr->value.unary_op.operand.var;
            break;
        case IR_MEMSET:
            if (instr->value.memset.ptr.kind == IR_VALUE_VAR) uses[count++] = &instr->value.memset.ptr.var;
            if (instr->value.memset.value.kind == IR_VALUE_VAR) uses[count++] = &instr->value.memset.value.var;
            if (instr->value.memset.length.kind == IR_VALUE_VAR) uses[count++] = &instr->value.memset.length.var;
            break;
        case IR_MEMCPY:
            if (instr->value.memcpy.dest.kind == IR_VALUE_VAR) uses[count++] = &instr->value.memcpy.dest.var;
            if (instr->value.memcpy.src.kind == IR_VALUE_VAR) uses[count++] = &instr->value.memcpy.src.var;
            if (instr->value.memcpy.length.kind == IR_VALUE_VAR) uses[count++] = &instr->value.memcpy.length.var;
            break;
        case IR_SWITCH:
            if (instr->value.switch_.value.kind == IR_VALUE_VAR) uses[count++] = &instr->value.switch_.value.var;
            break;
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
            return &instr->value.binary_op.result;
        case IR_ASSIGN:
            return &instr->value.assign.result;
        case IR_BR:
        case IR_BR_COND:
            break;
        case IR_CALL:
            if (instr->value.call.result != NULL) return instr->value.call.result;
            break;
        case IR_RET:
            break;
        case IR_ALLOCA:
            return &instr->value.alloca.result;
        case IR_STORE: break;
        case IR_LOAD:
        case IR_NOT:
        case IR_TRUNC:
        case IR_EXT:
        case IR_FTOI:
        case IR_ITOF:
        case IR_PTOI:
        case IR_ITOP:
        case IR_BITCAST:
            return &instr->value.unary_op.result;
        // No defs
        case IR_MEMSET:
        case IR_MEMCPY:
        case IR_SWITCH:
            break;
    }
    return NULL;
}

static void ir_collect_global_refs(const ir_const_t *val, string_vector_t *refs) {
    switch (val->kind) {
        case IR_CONST_GLOBAL_POINTER:
            VEC_APPEND(refs, (char*) val->value.global_name);
            break;
        case IR_CONST_ARRAY: {
            for (size_t i = 0; i < val->value.array.length; i += 1) {
                ir_collect_global_refs(&val->value.array.values[i], refs);
            }
            break;
        }
        case IR_CONST_STRUCT: {
            // Handle both structs and unions by visiting all initialized fields
            if (val->value._struct.is_union) {
                int idx = val->value._struct.union_field_index;
                if (idx >= 0 && (size_t)idx < val->value._struct.length) {
                    ir_collect_global_refs(&val->value._struct.fields[idx], refs);
                } else {
                    // Fallback: visit all fields if union selector is invalid
                    for (size_t i = 0; i < val->value._struct.length; i += 1) {
                        ir_collect_global_refs(&val->value._struct.fields[i], refs);
                    }
                }
            } else {
                for (size_t i = 0; i < val->value._struct.length; i += 1) {
                    ir_collect_global_refs(&val->value._struct.fields[i], refs);
                }
            }
            break;
        }
        default:
            break;
    }
}

// Based on Kahn's algorithm for topological sorting
// TODO: report cycles as error?
void ir_sort_global_definitions(ir_module_t *module) {
    ir_global_ptr_vector_t globals_sorted = { .buffer = NULL, .size = 0, .capacity = 0 };
    ir_global_ptr_vector_t pending = { .buffer = NULL, .size = 0, .capacity = 0 };

    // map of global name -> definition
    hash_table_t nodes = hash_table_create_string_keys(module->globals.size << 1);
    for (int i = 0; i < module->globals.size; i += 1) {
        ir_global_t *def = module->globals.buffer[i];
        hash_table_insert(&nodes, def->name, def);
    }

    // map of dep_name -> vector of dependents that reference dep_name
    hash_table_t edges = hash_table_create_string_keys(module->globals.size << 1);

    // map of global name -> in degree
    hash_table_t in_degree = hash_table_create_string_keys(module->globals.size << 1);

    // initialize in_degree to 0 for all nodes
    for (int i = 0; i < module->globals.size; i += 1) {
        ir_global_t *def = module->globals.buffer[i];
        hash_table_insert(&in_degree, def->name, (void*) (long) 0);
    }

    // build graph edges and in-degrees
    for (int i = 0; i < module->globals.size; i += 1) {
        const ir_global_t *def = module->globals.buffer[i];
        if (!def->initialized) continue;

        const ir_const_t *val = &def->value;
        string_vector_t refs = { .buffer = NULL, .size = 0, .capacity = 0 };
        ir_collect_global_refs(val, &refs);
        for (int j = 0; j < refs.size; j += 1) {
            const char *ref_name = refs.buffer[j];
            // Only consider edges to globals defined in this module
            void *ref_node = NULL;
            if (!hash_table_lookup(&nodes, ref_name, &ref_node)) {
                continue;
            }

            // edges[ref_name] -> add def->name as dependent
            string_vector_t *outs = NULL;
            void *outs_ptr = NULL;
            if (!hash_table_lookup(&edges, ref_name, &outs_ptr)) {
                outs = malloc(sizeof(string_vector_t));
                *outs = (string_vector_t) { .buffer = NULL, .size = 0, .capacity = 0 };
                hash_table_insert(&edges, ref_name, outs);
            } else {
                outs = (string_vector_t*) outs_ptr;
            }
            VEC_APPEND(outs, (char*) def->name);

            // increase in-degree of the dependent (def)
            void *deg_ptr = NULL;
            long deg = 0;
            if (hash_table_lookup(&in_degree, def->name, &deg_ptr)) {
                deg = (long) deg_ptr;
            }
            hash_table_insert(&in_degree, def->name, (void*) (deg + 1));
        }
        // cleanup refs buffer
        if (refs.buffer) free(refs.buffer);
    }

    // initialize pending list with nodes that have no dependencies
    for (int i = 0; i < module->globals.size; i += 1) {
        ir_global_t *def = module->globals.buffer[i];
        void *deg_ptr = NULL;
        long deg = 0;
        if (hash_table_lookup(&in_degree, def->name, &deg_ptr)) deg = (long) deg_ptr;
        if (deg == 0) VEC_APPEND(&pending, def);
    }

    while (pending.size > 0) {
        ir_global_t *u = pending.buffer[pending.size - 1];
        pending.size -= 1;
        VEC_APPEND(&globals_sorted, u);

        // for each dependent v of u, reduce in-degree and enqueue if 0
        void *outs_ptr = NULL;
        if (hash_table_lookup(&edges, u->name, &outs_ptr)) {
            string_vector_t *outs = (string_vector_t*) outs_ptr;
            for (size_t k = 0; k < outs->size; k += 1) {
                const char *v_name = outs->buffer[k];
                void *deg_ptr = NULL;
                long deg = 0;
                if (hash_table_lookup(&in_degree, v_name, &deg_ptr)) {
                    deg = (long) deg_ptr;
                }
                if (deg > 0) {
                    deg -= 1;
                    hash_table_insert(&in_degree, v_name, (void*) deg);
                    if (deg == 0) {
                        void *v_node = NULL;
                        if (hash_table_lookup(&nodes, v_name, &v_node)) {
                            VEC_APPEND(&pending, (ir_global_t*) v_node);
                        }
                    }
                }
            }
        }
    }

    // if a cycle exists, append remaining globals
    if (globals_sorted.size < module->globals.size) {
        // track which names are already in globals_sorted
        hash_table_t placed = hash_table_create_string_keys(module->globals.size << 1);
        for (size_t i = 0; i < globals_sorted.size; i += 1) {
            hash_table_insert(&placed, globals_sorted.buffer[i]->name, (void*)1);
        }
        for (size_t i = 0; i < module->globals.size; i += 1) {
            ir_global_t *g = module->globals.buffer[i];
            void *dummy = NULL;
            if (!hash_table_lookup(&placed, g->name, &dummy)) {
                VEC_APPEND(&globals_sorted, g);
                hash_table_insert(&placed, g->name, (void*)1);
            }
        }
        hash_table_destroy(&placed);
    }

    // write back the sorted order to the module
    ir_global_ptr_vector_t old = module->globals;
    module->globals = globals_sorted;
    // cleanup old buffer
    if (old.buffer) free(old.buffer);

    // clean up vectors in edges and the tables
    const hash_table_entry_t *it = hash_table_get_iterator(&edges);
    while (it != NULL) {
        string_vector_t *vec = (string_vector_t*) hash_table_entry_get_value(it);
        if (vec != NULL) {
            if (vec->buffer) free(vec->buffer);
            free(vec);
        }
        it = hash_table_iterator_next(it);
    }
    hash_table_destroy(&edges);
    hash_table_destroy(&in_degree);
    hash_table_destroy(&nodes);

    // cleanup pending vector buffer
    if (pending.buffer) free(pending.buffer);
}
