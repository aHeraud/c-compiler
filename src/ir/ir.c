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
        case IR_TYPE_I8:
        case IR_TYPE_U8:
            return 1;
        case IR_TYPE_I16:
        case IR_TYPE_U16:
            return 2;
        case IR_TYPE_I32:
        case IR_TYPE_U32:
            return 4;
        case IR_TYPE_I64:
        case IR_TYPE_U64:
            return 8;
        case IR_TYPE_PTR:
            return 8;
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
            assert(false && "Unimplemented");
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

const char* format_ir_type(char *buffer, size_t size, const ir_type_t *type) {
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
            snprintf(buffer, size, "*%s", format_ir_type(alloca(256), 256, type->ptr.pointee));
            break;
        case IR_TYPE_ARRAY: {
            char element[256];
            format_ir_type(element, sizeof(element), type->array.element);
            snprintf(buffer, size, "[%s;%lu] ", element, (unsigned long) type->array.length);
            break;
        }
        case IR_TYPE_STRUCT:
            // TODO
            assert(false && "Unimplemented");
            exit(1);
        case IR_TYPE_FUNCTION:
            // TODO
            assert(false && "Unimplemented");
            exit(1);
    }
    return buffer;
}

const char* format_ir_constant(char *buffer, size_t size, const ir_const_t constant) {
    switch (constant.kind) {
        case IR_CONST_INT:
            snprintf(buffer, size, "%s %llu", format_ir_type(alloca(256), 256, constant.type), constant.i);
            break;
        case IR_CONST_FLOAT:
            snprintf(buffer, size, "%s %Lf", format_ir_type(alloca(256), 256, constant.type), constant.f);
            break;
        case IR_CONST_STRING:
            snprintf(buffer, size, "%s \"%s\"", format_ir_type(alloca(256), 256, constant.type), constant.s);
            break;
    }
    return buffer;
}

const char* format_ir_variable(char *buffer, size_t size, const ir_var_t var) {
    snprintf(buffer, size, "%s %s", format_ir_type(alloca(256), 256, var.type), var.name);
    return buffer;
}

const char* format_ir_value(char *buffer, size_t size, const ir_value_t value) {
    switch (value.kind) {
        case IR_VALUE_CONST:
            return format_ir_constant(buffer, size, value.constant);
        case IR_VALUE_VAR:
            return format_ir_variable(buffer, size, value.var);
    }
}

#define FMT_VAL(val) format_ir_value(alloca(256), 256, val)
#define FMT_VAR(var) format_ir_variable(alloca(256), 256, var)
#define FMT_TYPE(type) format_ir_type(alloca(256), 256, type)

const char* format_ir_instruction(char *buffer, size_t size, const ir_instruction_t *instr) {
    const char* start = buffer;
    if (instr->label != NULL) {
        snprintf(buffer, size, "%s: ", instr->label);
        buffer += strlen(buffer);
    }

    switch (instr->opcode) {
        case IR_ADD:
            snprintf(buffer, size, "%s = add %s, %s", FMT_VAR(instr->add.result), FMT_VAL(instr->add.left), FMT_VAL(instr->add.right));
            break;
        case IR_SUB:
            snprintf(buffer, size, "%s = sub %s, %s", FMT_VAR(instr->add.result), FMT_VAL(instr->add.left), FMT_VAL(instr->add.right));
            break;
        case IR_MUL:
            snprintf(buffer, size, "%s = mul %s, %s", FMT_VAR(instr->add.result), FMT_VAL(instr->add.left), FMT_VAL(instr->add.right));
            break;
        case IR_DIV:
            snprintf(buffer, size, "%s = div %s, %s", FMT_VAR(instr->add.result), FMT_VAL(instr->add.left), FMT_VAL(instr->add.right));
            break;
        case IR_MOD:
            snprintf(buffer, size, "%s = mod %s, %s", FMT_VAR(instr->add.result), FMT_VAL(instr->add.left), FMT_VAL(instr->add.right));
            break;
        case IR_ASSIGN:
            snprintf(buffer, size, "%s = %s", FMT_VAR(instr->assign.result), FMT_VAL(instr->assign.value));
            break;
        case IR_AND:
            break;
        case IR_OR:
            break;
        case IR_SHL:
            break;
        case IR_SHR:
            break;
        case IR_XOR:
            break;
        case IR_NOT:
            break;
        case IR_EQ:
            break;
        case IR_NE:
            break;
        case IR_LT:
            break;
        case IR_LE:
            break;
        case IR_GT:
            break;
        case IR_GE:
            break;
        case IR_BR:
            snprintf(buffer, size, "br %s", instr->br.label);
            break;
        case IR_BR_COND:
            snprintf(buffer, size, "br %s, %s", FMT_VAL(instr->br_cond.cond), instr->br_cond.label);
            break;
        case IR_CALL:
            break;
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
            snprintf(buffer, size, "%s = load %s", FMT_VAR(instr->load.result), FMT_VAL(instr->load.ptr));
            break;
        case IR_STORE:
            snprintf(buffer, size, "store %s, %s", FMT_VAL(instr->store.value), FMT_VAL(instr->store.ptr));
            break;
        case IR_TRUNC:
            snprintf(buffer, size, "%s = trunc %s", FMT_VAR(instr->trunc.result), FMT_VAL(instr->trunc.value));
            break;
        case IR_EXT:
            snprintf(buffer, size, "%s = ext %s", FMT_VAR(instr->ext.result), FMT_VAL(instr->ext.value));
            break;
        case IR_FTOI:
            snprintf(buffer, size, "%s = ftoi %s", FMT_VAR(instr->ftoi.result), FMT_VAL(instr->ftoi.value));
            break;
        case IR_ITOF:
            snprintf(buffer, size, "%s = itof %s", FMT_VAR(instr->itof.result), FMT_VAL(instr->itof.value));
            break;
        case IR_BITCAST:
            break;
    }

    return start;
}
