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

const char* ir_fmt_type(char *buffer, size_t size, const ir_type_t *type) {
    switch (type->kind) {
        case IR_TYPE_VOID:
            snprintf(buffer, size, "void");
            break;
        case IR_TYPE_BOOL:
            snprintf(buffer, size, "bool");
            break;
        case IR_TYPE_I8:
            snprintf(buffer, size, "i8");
            break;
        case IR_TYPE_I16:
            snprintf(buffer, size, "i16");
            break;
        case IR_TYPE_I32:
            snprintf(buffer, size, "i32");
            break;
        case IR_TYPE_I64:
            snprintf(buffer, size, "i64");
            break;
        case IR_TYPE_U8:
            snprintf(buffer, size, "u8");
            break;
        case IR_TYPE_U16:
            snprintf(buffer, size, "u16");
            break;
        case IR_TYPE_U32:
            snprintf(buffer, size, "u32");
            break;
        case IR_TYPE_U64:
            snprintf(buffer, size, "u64");
            break;
        case IR_TYPE_F32:
            snprintf(buffer, size, "f32");
            break;
        case IR_TYPE_F64:
            snprintf(buffer, size, "f64");
            break;
        case IR_TYPE_PTR:
            snprintf(buffer, size, "*%s", ir_fmt_type(alloca(256), 256, type->ptr.pointee));
            break;
        case IR_TYPE_ARRAY: {
            char element[256];
            ir_fmt_type(element, sizeof(element), type->array.element);
            snprintf(buffer, size, "[%s;%lu] ", element, (unsigned long) type->array.length);
            break;
        }
        case IR_TYPE_STRUCT:
            // TODO
            assert(false && "Unimplemented");
            exit(1);
        case IR_TYPE_FUNCTION: {
            char param_list[512] = { 0 };
            char *curr = param_list;
            for (size_t i = 0; i < type->function.num_params; i++) {
                const ir_type_t *param = type->function.params[i];
                curr += snprintf(curr, param_list + sizeof(param_list) - curr, "%s", ir_fmt_type(alloca(256), 256, param));
                if (i < type->function.num_params - 1) {
                    curr += snprintf(curr, param_list + sizeof(param_list) - curr, ", ");
                }
            }
            snprintf(buffer, size, "(%s) -> %s", param_list, ir_fmt_type(alloca(256), 256, type->function.return_type));
            break;
        }
    }
    return buffer;
}

const char* ir_fmt_const(char *buffer, size_t size, ir_const_t constant) {
    switch (constant.kind) {
        case IR_CONST_INT:
            snprintf(buffer, size, "%s %llu", ir_fmt_type(alloca(256), 256, constant.type), constant.i);
            break;
        case IR_CONST_FLOAT:
            snprintf(buffer, size, "%s %Lf", ir_fmt_type(alloca(256), 256, constant.type), constant.f);
            break;
        case IR_CONST_STRING:
            snprintf(buffer, size, "%s \"%s\"", ir_fmt_type(alloca(256), 256, constant.type), constant.s);
            break;
    }
    return buffer;
}

const char* ir_fmt_var(char *buffer, size_t size, const ir_var_t var) {
    snprintf(buffer, size, "%s %s", ir_fmt_type(alloca(256), 256, var.type), var.name);
    return buffer;
}

const char* ir_fmt_val(char *buffer, size_t size, const ir_value_t value) {
    switch (value.kind) {
        case IR_VALUE_CONST:
            return ir_fmt_const(buffer, size, value.constant);
        case IR_VALUE_VAR:
            return ir_fmt_var(buffer, size, value.var);
    }
}

#define FMT_VAL(val) ir_fmt_val(alloca(256), 256, val)
#define FMT_VAR(var) ir_fmt_var(alloca(256), 256, var)
#define FMT_TYPE(type) ir_fmt_type(alloca(256), 256, type)

const char* ir_fmt_instr(char *buffer, size_t size, const ir_instruction_t *instr) {
    const char* start = buffer;
    if (instr->label != NULL) {
        size_t len = snprintf(buffer, size, "%s: ", instr->label);
        buffer += len;
        size -= len;
    }

    switch (instr->opcode) {
        case IR_NOP:
            snprintf(buffer, size, "nop");
            break;
        case IR_ADD:
            snprintf(buffer, size, "%s = add %s, %s", FMT_VAR(instr->binary_op.result), FMT_VAL(instr->binary_op.left), FMT_VAL(instr->binary_op.right));
            break;
        case IR_SUB:
            snprintf(buffer, size, "%s = sub %s, %s", FMT_VAR(instr->binary_op.result), FMT_VAL(instr->binary_op.left), FMT_VAL(instr->binary_op.right));
            break;
        case IR_MUL:
            snprintf(buffer, size, "%s = mul %s, %s", FMT_VAR(instr->binary_op.result), FMT_VAL(instr->binary_op.left), FMT_VAL(instr->binary_op.right));
            break;
        case IR_DIV:
            snprintf(buffer, size, "%s = div %s, %s", FMT_VAR(instr->binary_op.result), FMT_VAL(instr->binary_op.left), FMT_VAL(instr->binary_op.right));
            break;
        case IR_MOD:
            snprintf(buffer, size, "%s = mod %s, %s", FMT_VAR(instr->binary_op.result), FMT_VAL(instr->binary_op.left), FMT_VAL(instr->binary_op.right));
            break;
        case IR_ASSIGN:
            snprintf(buffer, size, "%s = %s", FMT_VAR(instr->assign.result), FMT_VAL(instr->assign.value));
            break;
        case IR_AND:
            snprintf(buffer, size, "%s = and %s, %s", FMT_VAR(instr->binary_op.result), FMT_VAL(instr->binary_op.left), FMT_VAL(instr->binary_op.right));
            break;
        case IR_OR:
            snprintf(buffer, size, "%s = or %s, %s", FMT_VAR(instr->binary_op.result), FMT_VAL(instr->binary_op.left), FMT_VAL(instr->binary_op.right));
            break;
        case IR_SHL:
            snprintf(buffer, size, "%s = shl %s, %s", FMT_VAR(instr->binary_op.result), FMT_VAL(instr->binary_op.left), FMT_VAL(instr->binary_op.right));
            break;
        case IR_SHR:
            snprintf(buffer, size, "%s = shr %s, %s", FMT_VAR(instr->binary_op.result), FMT_VAL(instr->binary_op.left), FMT_VAL(instr->binary_op.right));
            break;
        case IR_XOR:
            snprintf(buffer, size, "%s = xor %s, %s", FMT_VAR(instr->binary_op.result), FMT_VAL(instr->binary_op.left), FMT_VAL(instr->binary_op.right));
            break;
        case IR_NOT:
            snprintf(buffer, size, "%s = not %s", FMT_VAR(instr->unary_op.result), FMT_VAL(instr->unary_op.operand));
            break;
        case IR_EQ:
            snprintf(buffer, size, "%s = eq %s, %s", FMT_VAR(instr->binary_op.result), FMT_VAL(instr->binary_op.left), FMT_VAL(instr->binary_op.right));
            break;
        case IR_NE:
            snprintf(buffer, size, "%s = ne %s, %s", FMT_VAR(instr->binary_op.result), FMT_VAL(instr->binary_op.left), FMT_VAL(instr->binary_op.right));
            break;
        case IR_LT:
            snprintf(buffer, size, "%s = lt %s, %s", FMT_VAR(instr->binary_op.result), FMT_VAL(instr->binary_op.left), FMT_VAL(instr->binary_op.right));
            break;
        case IR_LE:
            snprintf(buffer, size, "%s = le %s, %s", FMT_VAR(instr->binary_op.result), FMT_VAL(instr->binary_op.left), FMT_VAL(instr->binary_op.right));
            break;
        case IR_GT:
            snprintf(buffer, size, "%s = gt %s, %s", FMT_VAR(instr->binary_op.result), FMT_VAL(instr->binary_op.left), FMT_VAL(instr->binary_op.right));
            break;
        case IR_GE:
            snprintf(buffer, size, "%s = ge %s, %s", FMT_VAR(instr->binary_op.result), FMT_VAL(instr->binary_op.left), FMT_VAL(instr->binary_op.right));
            break;
        case IR_BR:
            snprintf(buffer, size, "br %s", instr->branch.label);
            break;
        case IR_BR_COND:
            snprintf(buffer, size, "br %s, %s", FMT_VAL(instr->branch.cond), instr->branch.label);
            break;
        case IR_CALL: {
            if (instr->call.result != NULL) {
                size_t offset = snprintf(buffer, size, "%s = ", FMT_VAR(*instr->call.result));
                buffer += offset;
                size -= offset;
            }
            size_t offset = snprintf(buffer, size, "call %s(", instr->call.function.name);
            buffer += offset;
            size -= offset;
            for (size_t i = 0; i < instr->call.num_args; i += 1) {
                offset = snprintf(buffer, size, "%s", FMT_VAL(instr->call.args[i]));
                buffer += offset;
                size -= offset;
                if (i < instr->call.num_args - 1) {
                    offset = snprintf(buffer, size, ", ");
                    buffer += offset;
                    size -= offset;
                }
            }
            snprintf(buffer, size, ")");
            break;
        }
        case IR_RET: {
            if (instr->ret.has_value) {
                snprintf(buffer, size, "ret %s", FMT_VAL(instr->ret.value));
            } else {
                snprintf(buffer, size, "ret void");
            }
            break;
        }
        case IR_ALLOCA:
            snprintf(buffer, size, "%s = alloca %s", FMT_VAR(instr->alloca.result), FMT_TYPE(instr->alloca.type));
            break;
        case IR_LOAD:
            snprintf(buffer, size, "%s = load %s", FMT_VAR(instr->unary_op.result), FMT_VAL(instr->unary_op.operand));
            break;
        case IR_STORE:
            snprintf(buffer, size, "store %s, %s", FMT_VAL(instr->store.value), FMT_VAL(instr->store.ptr));
            break;
        case IR_MEMCPY:
            snprintf(buffer, size, "memcpy %s, %s", FMT_VAR(instr->unary_op.result), FMT_VAL(instr->unary_op.operand));
            break;
        case IR_TRUNC:
            snprintf(buffer, size, "%s = trunc %s", FMT_VAR(instr->unary_op.result), FMT_VAL(instr->unary_op.operand));
            break;
        case IR_EXT:
            snprintf(buffer, size, "%s = ext %s", FMT_VAR(instr->unary_op.result), FMT_VAL(instr->unary_op.operand));
            break;
        case IR_FTOI:
            snprintf(buffer, size, "%s = ftoi %s", FMT_VAR(instr->unary_op.result), FMT_VAL(instr->unary_op.operand));
            break;
        case IR_ITOF:
            snprintf(buffer, size, "%s = itof %s", FMT_VAR(instr->unary_op.result), FMT_VAL(instr->unary_op.operand));
            break;
        case IR_ITOP:
            snprintf(buffer, size, "%s = itop %s", FMT_VAR(instr->unary_op.result), FMT_VAL(instr->unary_op.operand));
            break;
        case IR_PTOI:
            snprintf(buffer, size, "%s = ptoi %s", FMT_VAR(instr->unary_op.result), FMT_VAL(instr->unary_op.operand));
            break;
        case IR_BITCAST:
            snprintf(buffer, size, "%s = bitcast %s", FMT_VAR(instr->unary_op.result), FMT_VAL(instr->unary_op.operand));
            break;
    }

    return start;
}

void ir_print_module(FILE *file, const ir_module_t *module) {
    for (size_t i = 0; i < module->functions.size; i++) {
        ir_function_definition_t *function = module->functions.buffer[i];
        fprintf(file, "function %s %s", function->name, FMT_TYPE(function->type));
        fprintf(file, " {\n");
        char buffer[512];
        for (size_t j = 0; j < function->body.size; j++) {
            ir_instruction_t *instr = &function->body.buffer[j];
            fprintf(file, "    %s\n", ir_fmt_instr(buffer, sizeof(buffer), instr));
        }
        fprintf(file, "}\n");
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
