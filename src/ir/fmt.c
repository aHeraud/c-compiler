#include <stdio.h>
#include <string.h>
#include "ir/ir.h"
#include "ir/fmt.h"

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
            snprintf(buffer, size, "*%s", ir_fmt_type(alloca(256), 256, type->value.ptr.pointee));
            break;
        case IR_TYPE_ARRAY: {
            char element[256];
            ir_fmt_type(element, sizeof(element), type->value.array.element);
            snprintf(buffer, size, "[%s;%lu]", element, (unsigned long) type->value.array.length);
            break;
        }
        case IR_TYPE_STRUCT_OR_UNION:
            // This just prints the name of the struct, not the full definition
            snprintf(buffer, size, "%s.%s", type->value.struct_or_union.is_union ? "union" : "struct", type->value.struct_or_union.id);
            break;
        case IR_TYPE_FUNCTION: {
            char param_list[512] = { 0 };
            char *curr = param_list;
            for (size_t i = 0; i < type->value.function.num_params; i++) {
                const ir_type_t *param = type->value.function.params[i];
                curr += snprintf(curr, param_list + sizeof(param_list) - curr, "%s", ir_fmt_type(alloca(256), 256, param));
                if (i < type->value.function.num_params - 1) {
                    curr += snprintf(curr, param_list + sizeof(param_list) - curr, ", ");
                }
            }
            snprintf(buffer, size, "(%s) -> %s", param_list, ir_fmt_type(alloca(256), 256, type->value.function.return_type));
            break;
        }
    }
    return buffer;
}

// Format a constant string, escaping control characters
char* ir_fmt_const_string(const char *str) {
    char_vector_t string = (char_vector_t) VEC_INIT;

    size_t len = strlen(str);
    for (int i = 0; i < len; i += 1) {
        char c = str[i];

        // TODO: This doesn't currently handle escaped backslashes
        bool prev_escape = i > 0 && str[i - 1] == '\\';
        bool is_control_char = c == '\n' || c == '\t' || c == '\r' || c == '"' || c == '\\';
        if (is_control_char && !prev_escape) {
            VEC_APPEND(&string, '\\');
            switch (c) {
                case '\n':
                    VEC_APPEND(&string, 'n');
                    break;
                case '\t':
                    VEC_APPEND(&string, 't');
                    break;
                case '\r':
                    VEC_APPEND(&string, 'r');
                    break;
                case '"':
                    VEC_APPEND(&string, '"');
                    break;
                case '\\':
                    VEC_APPEND(&string, '\\');
                    break;
            }
        } else {
            VEC_APPEND(&string, c);
        }
    }
    VEC_APPEND(&string, '\0');
    VEC_SHRINK(&string, char);
    return string.buffer;
}

const char* ir_fmt_const(char *buffer, size_t size, ir_const_t constant) {
    switch (constant.kind) {
        case IR_CONST_INT:
            snprintf(buffer, size, "%s %lli", ir_fmt_type(alloca(256), 256, constant.type), constant.value.i);
            break;
        case IR_CONST_FLOAT:
            snprintf(buffer, size, "%s %Lf", ir_fmt_type(alloca(256), 256, constant.type), constant.value.f);
            break;
        case IR_CONST_STRING: {
            char *str = ir_fmt_const_string(constant.value.s);
            snprintf(buffer, size, "%s \"%s\"", ir_fmt_type(alloca(256), 256, constant.type), str);
            free(str);
            break;
        }
        case IR_CONST_ARRAY: {
            int l = snprintf(buffer, size, "%s {", ir_fmt_type(alloca(256), 256, constant.type));
            buffer += l;
            size -= l;
            for (int i = 0; i < constant.value.array.length; i += 1) {
                ir_const_t val = constant.value.array.values[i];
                const char *str = NULL;
                switch (val.kind) {
                    case IR_CONST_ARRAY:
                        str = "{ ... }"; // TODO
                        break;
                    case IR_CONST_INT: {
                        char *s = alloca(256);
                        snprintf(s, 256, "%ld", (long) val.value.i);
                        str = s;
                        break;
                    }
                    case IR_CONST_FLOAT: {
                        char *s = alloca(256);
                        snprintf(s, 256, "%f", (double) val.value.f);
                        str = s;
                        break;
                    }
                    case IR_CONST_STRING:
                        str = val.value.s;
                        break;
                }
                if (i > 0) l = snprintf(buffer, size, ",%s", str);
                else l = snprintf(buffer, size, "%s", str);
                buffer += l;
                size -= l;
            }
        }
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
            snprintf(buffer, size, "%s = add %s, %s", FMT_VAR(instr->value.binary_op.result), FMT_VAL(instr->value.binary_op.left), FMT_VAL(instr->value.binary_op.right));
            break;
        case IR_SUB:
            snprintf(buffer, size, "%s = sub %s, %s", FMT_VAR(instr->value.binary_op.result), FMT_VAL(instr->value.binary_op.left), FMT_VAL(instr->value.binary_op.right));
            break;
        case IR_MUL:
            snprintf(buffer, size, "%s = mul %s, %s", FMT_VAR(instr->value.binary_op.result), FMT_VAL(instr->value.binary_op.left), FMT_VAL(instr->value.binary_op.right));
            break;
        case IR_DIV:
            snprintf(buffer, size, "%s = div %s, %s", FMT_VAR(instr->value.binary_op.result), FMT_VAL(instr->value.binary_op.left), FMT_VAL(instr->value.binary_op.right));
            break;
        case IR_MOD:
            snprintf(buffer, size, "%s = mod %s, %s", FMT_VAR(instr->value.binary_op.result), FMT_VAL(instr->value.binary_op.left), FMT_VAL(instr->value.binary_op.right));
            break;
        case IR_ASSIGN:
            snprintf(buffer, size, "%s = %s", FMT_VAR(instr->value.assign.result), FMT_VAL(instr->value.assign.value));
            break;
        case IR_AND:
            snprintf(buffer, size, "%s = and %s, %s", FMT_VAR(instr->value.binary_op.result), FMT_VAL(instr->value.binary_op.left), FMT_VAL(instr->value.binary_op.right));
            break;
        case IR_OR:
            snprintf(buffer, size, "%s = or %s, %s", FMT_VAR(instr->value.binary_op.result), FMT_VAL(instr->value.binary_op.left), FMT_VAL(instr->value.binary_op.right));
            break;
        case IR_SHL:
            snprintf(buffer, size, "%s = shl %s, %s", FMT_VAR(instr->value.binary_op.result), FMT_VAL(instr->value.binary_op.left), FMT_VAL(instr->value.binary_op.right));
            break;
        case IR_SHR:
            snprintf(buffer, size, "%s = shr %s, %s", FMT_VAR(instr->value.binary_op.result), FMT_VAL(instr->value.binary_op.left), FMT_VAL(instr->value.binary_op.right));
            break;
        case IR_XOR:
            snprintf(buffer, size, "%s = xor %s, %s", FMT_VAR(instr->value.binary_op.result), FMT_VAL(instr->value.binary_op.left), FMT_VAL(instr->value.binary_op.right));
            break;
        case IR_NOT:
            snprintf(buffer, size, "%s = not %s", FMT_VAR(instr->value.unary_op.result), FMT_VAL(instr->value.unary_op.operand));
            break;
        case IR_EQ:
            snprintf(buffer, size, "%s = eq %s, %s", FMT_VAR(instr->value.binary_op.result), FMT_VAL(instr->value.binary_op.left), FMT_VAL(instr->value.binary_op.right));
            break;
        case IR_NE:
            snprintf(buffer, size, "%s = ne %s, %s", FMT_VAR(instr->value.binary_op.result), FMT_VAL(instr->value.binary_op.left), FMT_VAL(instr->value.binary_op.right));
            break;
        case IR_LT:
            snprintf(buffer, size, "%s = lt %s, %s", FMT_VAR(instr->value.binary_op.result), FMT_VAL(instr->value.binary_op.left), FMT_VAL(instr->value.binary_op.right));
            break;
        case IR_LE:
            snprintf(buffer, size, "%s = le %s, %s", FMT_VAR(instr->value.binary_op.result), FMT_VAL(instr->value.binary_op.left), FMT_VAL(instr->value.binary_op.right));
            break;
        case IR_GT:
            snprintf(buffer, size, "%s = gt %s, %s", FMT_VAR(instr->value.binary_op.result), FMT_VAL(instr->value.binary_op.left), FMT_VAL(instr->value.binary_op.right));
            break;
        case IR_GE:
            snprintf(buffer, size, "%s = ge %s, %s", FMT_VAR(instr->value.binary_op.result), FMT_VAL(instr->value.binary_op.left), FMT_VAL(instr->value.binary_op.right));
            break;
        case IR_BR:
            snprintf(buffer, size, "br %s", instr->value.branch.label);
            break;
        case IR_BR_COND:
            snprintf(buffer, size, "br %s, %s", FMT_VAL(instr->value.branch.cond), instr->value.branch.label);
            break;
        case IR_CALL: {
            if (instr->value.call.result != NULL) {
                size_t offset = snprintf(buffer, size, "%s = ", FMT_VAR(*instr->value.call.result));
                buffer += offset;
                size -= offset;
            }
            size_t offset = snprintf(buffer, size, "call %s(", instr->value.call.function.name);
            buffer += offset;
            size -= offset;
            for (size_t i = 0; i < instr->value.call.num_args; i += 1) {
                offset = snprintf(buffer, size, "%s", FMT_VAL(instr->value.call.args[i]));
                buffer += offset;
                size -= offset;
                if (i < instr->value.call.num_args - 1) {
                    offset = snprintf(buffer, size, ", ");
                    buffer += offset;
                    size -= offset;
                }
            }
            snprintf(buffer, size, ")");
            break;
        }
        case IR_RET: {
            if (instr->value.ret.has_value) {
                snprintf(buffer, size, "ret %s", FMT_VAL(instr->value.ret.value));
            } else {
                snprintf(buffer, size, "ret void");
            }
            break;
        }
        case IR_ALLOCA:
            snprintf(buffer, size, "%s = alloca %s", FMT_VAR(instr->value.alloca.result), FMT_TYPE(instr->value.alloca.type));
            break;
        case IR_LOAD:
            snprintf(buffer, size, "%s = load %s", FMT_VAR(instr->value.unary_op.result), FMT_VAL(instr->value.unary_op.operand));
            break;
        case IR_STORE:
            snprintf(buffer, size, "store %s, %s", FMT_VAL(instr->value.store.value), FMT_VAL(instr->value.store.ptr));
            break;
        case IR_MEMCPY:
            snprintf(buffer, size, "memcpy %s, %s", FMT_VAR(instr->value.unary_op.result), FMT_VAL(instr->value.unary_op.operand));
            break;
        case IR_MEMSET:
            snprintf(buffer, size, "memset %s, %s, %s", FMT_VAL(instr->value.memset.ptr), FMT_VAL(instr->value.memset.value), FMT_VAL(instr->value.memset.length));
            break;
        case IR_GET_ARRAY_ELEMENT_PTR:
            snprintf(buffer, size, "%s = get_array_element_ptr %s, %s", FMT_VAR(instr->value.binary_op.result), FMT_VAL(instr->value.binary_op.left), FMT_VAL(instr->value.binary_op.right));
            break;
        case IR_GET_STRUCT_MEMBER_PTR:
            snprintf(buffer, size, "%s = get_struct_member_ptr %s, %s", FMT_VAR(instr->value.binary_op.result), FMT_VAL(instr->value.binary_op.left), FMT_VAL(instr->value.binary_op.right));
            break;
        case IR_TRUNC:
            snprintf(buffer, size, "%s = trunc %s", FMT_VAR(instr->value.unary_op.result), FMT_VAL(instr->value.unary_op.operand));
            break;
        case IR_EXT:
            snprintf(buffer, size, "%s = ext %s", FMT_VAR(instr->value.unary_op.result), FMT_VAL(instr->value.unary_op.operand));
            break;
        case IR_FTOI:
            snprintf(buffer, size, "%s = ftoi %s", FMT_VAR(instr->value.unary_op.result), FMT_VAL(instr->value.unary_op.operand));
            break;
        case IR_ITOF:
            snprintf(buffer, size, "%s = itof %s", FMT_VAR(instr->value.unary_op.result), FMT_VAL(instr->value.unary_op.operand));
            break;
        case IR_ITOP:
            snprintf(buffer, size, "%s = itop %s", FMT_VAR(instr->value.unary_op.result), FMT_VAL(instr->value.unary_op.operand));
            break;
        case IR_PTOI:
            snprintf(buffer, size, "%s = ptoi %s", FMT_VAR(instr->value.unary_op.result), FMT_VAL(instr->value.unary_op.operand));
            break;
        case IR_BITCAST:
            snprintf(buffer, size, "%s = bitcast %s", FMT_VAR(instr->value.unary_op.result), FMT_VAL(instr->value.unary_op.operand));
            break;
    }

    return start;
}

void ir_print_module(FILE *file, const ir_module_t *module) {
    // Print globals
    for (size_t i = 0; i < module->globals.size; i++) {
        ir_global_t *global = module->globals.buffer[i];
        if (global->initialized) {
            char value[512];
            ir_fmt_const(value, sizeof(value), global->value);
            fprintf(file, "global %s %s = %s\n", FMT_TYPE(global->type), global->name, value);
        } else {
            fprintf(file, "global %s %s\n", FMT_TYPE(global->type), global->name);
        }
    }

    // Print functions
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
