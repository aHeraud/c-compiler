#include <stdio.h>
#include <string.h>
#include "ir/ir.h"
#include "ir/fmt.h"

#define _fmt_snprintf_or_err(res, err, buffer, size, ...)       \
    do {                                                        \
        int _written = snprintf((buffer), (size), __VA_ARGS__); \
        if (_written < 0) {                                     \
            goto err;                                           \
        } else if (_written < size) {                           \
            (size) -= _written;                                 \
            (buffer) += _written;                               \
            (res) += _written;                                  \
        } else {                                                \
            /* not enough space in the buffer */                \
            (buffer) += (size) - 1;                             \
            (size) = 0;                                         \
            (res) += _written;                                  \
        }                                                       \
    } while (0)

int ir_fmt_type(char *buffer, size_t size, const ir_type_t *type) {
    int result = 0;
    switch (type->kind) {
        case IR_TYPE_VOID:
            _fmt_snprintf_or_err(result, err, buffer, size, "void");
            break;
        case IR_TYPE_BOOL:
            _fmt_snprintf_or_err(result, err, buffer, size, "bool");
            break;
        case IR_TYPE_I8:
            _fmt_snprintf_or_err(result, err, buffer, size, "i8");
            break;
        case IR_TYPE_I16:
            _fmt_snprintf_or_err(result, err, buffer, size, "i16");
            break;
        case IR_TYPE_I32:
            _fmt_snprintf_or_err(result, err, buffer, size, "i32");
            break;
        case IR_TYPE_I64:
            _fmt_snprintf_or_err(result, err, buffer, size, "i64");
            break;
        case IR_TYPE_U8:
            _fmt_snprintf_or_err(result, err, buffer, size, "u8");
            break;
        case IR_TYPE_U16:
            _fmt_snprintf_or_err(result, err, buffer, size, "u16");
            break;
        case IR_TYPE_U32:
            _fmt_snprintf_or_err(result, err, buffer, size, "u32");
            break;
        case IR_TYPE_U64:
            _fmt_snprintf_or_err(result, err, buffer, size, "u64");
            break;
        case IR_TYPE_F32:
            _fmt_snprintf_or_err(result, err, buffer, size, "f32");
            break;
        case IR_TYPE_F64:
            _fmt_snprintf_or_err(result, err, buffer, size, "f64");
            break;
        case IR_TYPE_PTR: {
            char _type[256];
            ir_fmt_type(_type, 256, type->value.ptr.pointee);
            _fmt_snprintf_or_err(result, err, buffer, size, "*%s", _type);
            break;
        }
        case IR_TYPE_ARRAY: {
            char element[256];
            ir_fmt_type(element, sizeof(element), type->value.array.element);
            _fmt_snprintf_or_err(result, err, buffer, size, "[%s;%lu]", element, (unsigned long) type->value.array.length);
            break;
        }
        case IR_TYPE_STRUCT_OR_UNION:
            // This just prints the name of the struct, not the full definition
            _fmt_snprintf_or_err(result, err, buffer, size, "%s.%s", type->value.struct_or_union.is_union ? "union" : "struct", type->value.struct_or_union.id);
            break;
        case IR_TYPE_FUNCTION: {
            char param_list[1024] = { 0 };
            char *curr = param_list;
            for (size_t i = 0; i < type->value.function.num_params; i++) {
                const ir_type_t *param = type->value.function.params[i];
                char _param_type[256];
                ir_fmt_type(alloca(256), 256, param);
                curr += snprintf(curr, param_list + sizeof(param_list) - curr, "%s", _param_type);
                if (i < type->value.function.num_params - 1) {
                    curr += snprintf(curr, param_list + sizeof(param_list) - curr, ", ");
                }
            }
            char _type[256];
            ir_fmt_type(_type, 256, type->value.function.return_type);
            _fmt_snprintf_or_err(result, err, buffer, size, "(%s) -> %s", param_list, _type);
            break;
        }
    }
    return result;
err:
    return -1;
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

int ir_fmt_const_no_type(char *buffer, size_t size, ir_const_t constant);

int ir_fmt_const(char *buffer, size_t size, ir_const_t constant) {
    int result = 0;
    char _type[1024];
    ir_fmt_type(_type, 256, constant.type);
    _fmt_snprintf_or_err(result, err, buffer, size, "%s ", _type);
    result += ir_fmt_const_no_type(buffer, size, constant);
    return result;
    err:
        return -1;
}

int ir_fmt_const_no_type(char *buffer, size_t size, ir_const_t constant) {
    int result = 0;
    switch (constant.kind) {
        case IR_CONST_INT:
            _fmt_snprintf_or_err(result, err, buffer, size, "%lli", constant.value.i);
            break;
        case IR_CONST_FLOAT:
            _fmt_snprintf_or_err(result, err, buffer, size, "%Lf", constant.value.f);
            break;
        case IR_CONST_STRING: {
            char *str = ir_fmt_const_string(constant.value.s);
            _fmt_snprintf_or_err(result, err, buffer, size, "\"%s\"", str);
            free(str);
            break;
        }
        case IR_CONST_ARRAY:
        case IR_CONST_STRUCT:
        {
            size_t length;
            const ir_const_t *values;
            if (constant.kind == IR_CONST_ARRAY) {
                length = constant.value.array.length;
                values = constant.value.array.values;
            } else {
                length = constant.value._struct.length;
                values = constant.value._struct.fields;
            }

            _fmt_snprintf_or_err(result, err, buffer, size, "{");
            for (int i = 0; i < length; i += 1) {
                if (i > 0) _fmt_snprintf_or_err(result, err, buffer, size, ",");
                ir_const_t val = values[i];
                int written = ir_fmt_const(buffer, size, val);
                if (written == -1) goto err;
                if (written >= size) {
                    // buffer too small
                    buffer += size - 1;
                    result += written;
                    size = 0;
                } else {
                    size -= written;
                    buffer += written;
                    result += written;
                }
            }
            _fmt_snprintf_or_err(result, err, buffer, size, "}");
        }
        case IR_CONST_GLOBAL_POINTER:
            _fmt_snprintf_or_err(result, err, buffer, size, "%s", constant.value.global_name);
            break;
    }
    return result;
err:
    return -1;
}

int ir_fmt_var(char *buffer, size_t size, const ir_var_t var) {
    char type[512];
    ir_fmt_type(type, 512, var.type);
    return snprintf(buffer, size, "%s %s", type, var.name);
}

int ir_fmt_val(char *buffer, size_t size, const ir_value_t value) {
    switch (value.kind) {
        case IR_VALUE_CONST:
            return ir_fmt_const(buffer, size, value.constant);
        case IR_VALUE_VAR:
            return ir_fmt_var(buffer, size, value.var);
        default:
            return 0;
    }
}

// TODO: handle errors/buffer too small
#define FMT_VAL(buffer, size, val) (ir_fmt_val(buffer, size, val), buffer)
#define FMT_VAR(buffer, size, var) (ir_fmt_var(buffer, size, var), buffer)
#define FMT_TYPE(buffer, size, type) (ir_fmt_type(buffer, size, type), buffer)

int ir_fmt_instr(char *buffer, size_t size, const ir_instruction_t *instr) {
    int result = 0;
    if (instr->label != NULL) _fmt_snprintf_or_err(result, err, buffer, size, "%s: ", instr->label);

    char a[512];
    char b[512];
    char c[512];

    switch (instr->opcode) {
        case IR_NOP:
            _fmt_snprintf_or_err(result, err, buffer, size, "nop");
            break;
        case IR_ADD:
            _fmt_snprintf_or_err(result, err, buffer, size, "%s = add %s, %s", FMT_VAR(a, 512, instr->value.binary_op.result), FMT_VAL(b, 512, instr->value.binary_op.left), FMT_VAL(c, 512, instr->value.binary_op.right));
            break;
        case IR_SUB:
            _fmt_snprintf_or_err(result, err, buffer, size, "%s = sub %s, %s", FMT_VAR(a, 512, instr->value.binary_op.result), FMT_VAL(b, 512, instr->value.binary_op.left), FMT_VAL(c, 512, instr->value.binary_op.right));
            break;
        case IR_MUL:
            _fmt_snprintf_or_err(result, err, buffer, size, "%s = mul %s, %s", FMT_VAR(a, 512, instr->value.binary_op.result), FMT_VAL(b, 512, instr->value.binary_op.left), FMT_VAL(c, 512, instr->value.binary_op.right));
            break;
        case IR_DIV:
            _fmt_snprintf_or_err(result, err, buffer, size, "%s = div %s, %s", FMT_VAR(a, 512, instr->value.binary_op.result), FMT_VAL(b, 512, instr->value.binary_op.left), FMT_VAL(c, 512, instr->value.binary_op.right));
            break;
        case IR_MOD:
            _fmt_snprintf_or_err(result, err, buffer, size, "%s = mod %s, %s", FMT_VAR(a, 512, instr->value.binary_op.result), FMT_VAL(b, 512, instr->value.binary_op.left), FMT_VAL(c, 512, instr->value.binary_op.right));
            break;
        case IR_ASSIGN:
            _fmt_snprintf_or_err(result, err, buffer, size, "%s = %s", FMT_VAR(a, 512, instr->value.assign.result), FMT_VAL(b, 512, instr->value.assign.value));
            break;
        case IR_AND:
            _fmt_snprintf_or_err(result, err, buffer, size, "%s = and %s, %s", FMT_VAR(a, 512, instr->value.binary_op.result), FMT_VAL(b, 512, instr->value.binary_op.left), FMT_VAL(c, 512, instr->value.binary_op.right));
            break;
        case IR_OR:
            _fmt_snprintf_or_err(result, err, buffer, size, "%s = or %s, %s", FMT_VAR(a, 512, instr->value.binary_op.result), FMT_VAL(b, 512, instr->value.binary_op.left), FMT_VAL(c, 512, instr->value.binary_op.right));
            break;
        case IR_SHL:
            _fmt_snprintf_or_err(result, err, buffer, size, "%s = shl %s, %s", FMT_VAR(a, 512, instr->value.binary_op.result), FMT_VAL(b, 512, instr->value.binary_op.left), FMT_VAL(c, 512, instr->value.binary_op.right));
            break;
        case IR_SHR:
            _fmt_snprintf_or_err(result, err, buffer, size, "%s = shr %s, %s", FMT_VAR(a, 512, instr->value.binary_op.result), FMT_VAL(b, 512, instr->value.binary_op.left), FMT_VAL(c, 512, instr->value.binary_op.right));
            break;
        case IR_XOR:
            _fmt_snprintf_or_err(result, err, buffer, size, "%s = xor %s, %s", FMT_VAR(a, 512, instr->value.binary_op.result), FMT_VAL(b, 512, instr->value.binary_op.left), FMT_VAL(c, 512, instr->value.binary_op.right));
            break;
        case IR_NOT:
            _fmt_snprintf_or_err(result, err, buffer, size, "%s = not %s", FMT_VAR(a, 512, instr->value.unary_op.result), FMT_VAL(b, 512, instr->value.unary_op.operand));
            break;
        case IR_EQ:
            _fmt_snprintf_or_err(result, err, buffer, size, "%s = eq %s, %s", FMT_VAR(a, 512, instr->value.binary_op.result), FMT_VAL(b, 512, instr->value.binary_op.left), FMT_VAL(c, 512, instr->value.binary_op.right));
            break;
        case IR_NE:
            _fmt_snprintf_or_err(result, err, buffer, size, "%s = ne %s, %s", FMT_VAR(a, 512, instr->value.binary_op.result), FMT_VAL(b, 512, instr->value.binary_op.left), FMT_VAL(c, 512, instr->value.binary_op.right));
            break;
        case IR_LT:
            _fmt_snprintf_or_err(result, err, buffer, size, "%s = lt %s, %s", FMT_VAR(a, 512, instr->value.binary_op.result), FMT_VAL(b, 512, instr->value.binary_op.left), FMT_VAL(c, 512, instr->value.binary_op.right));
            break;
        case IR_LE:
            _fmt_snprintf_or_err(result, err, buffer, size, "%s = le %s, %s", FMT_VAR(a, 512, instr->value.binary_op.result), FMT_VAL(b, 512, instr->value.binary_op.left), FMT_VAL(c, 512, instr->value.binary_op.right));
            break;
        case IR_GT:
            _fmt_snprintf_or_err(result, err, buffer, size, "%s = gt %s, %s", FMT_VAR(a, 512, instr->value.binary_op.result), FMT_VAL(b, 512, instr->value.binary_op.left), FMT_VAL(c, 512, instr->value.binary_op.right));
            break;
        case IR_GE:
            _fmt_snprintf_or_err(result, err, buffer, size, "%s = ge %s, %s", FMT_VAR(a, 512, instr->value.binary_op.result), FMT_VAL(b, 512, instr->value.binary_op.left), FMT_VAL(c, 512, instr->value.binary_op.right));
            break;
        case IR_BR:
            _fmt_snprintf_or_err(result, err, buffer, size, "br %s", instr->value.branch.label);
            break;
        case IR_BR_COND:
            _fmt_snprintf_or_err(result, err, buffer, size, "br %s, %s", FMT_VAL(a, 512, instr->value.branch.cond), instr->value.branch.label);
            break;
        case IR_CALL: {
            if (instr->value.call.result != NULL)
                _fmt_snprintf_or_err(result, err, buffer, size, "%s = ", FMT_VAR(a, 512, *instr->value.call.result));
            if (instr->value.call.function.kind == IR_VALUE_VAR) {
                _fmt_snprintf_or_err(result, err, buffer, size, "call %s(", instr->value.call.function.var.name);
            } else {
                char fname[512];
                ir_fmt_const_no_type(fname, 512, instr->value.call.function.constant);
                _fmt_snprintf_or_err(result, err, buffer, size, "call %s(", fname);
            }
            for (size_t i = 0; i < instr->value.call.num_args; i += 1) {
                _fmt_snprintf_or_err(result, err, buffer, size, "%s", FMT_VAL(a, 512, instr->value.call.args[i]));
                if (i < instr->value.call.num_args - 1) {
                    _fmt_snprintf_or_err(result, err, buffer, size, ", ");
                }
            }
            _fmt_snprintf_or_err(result, err, buffer, size, ")");
            break;
        }
        case IR_RET: {
            if (instr->value.ret.has_value) {
                _fmt_snprintf_or_err(result, err, buffer, size, "ret %s", FMT_VAL(a, 512, instr->value.ret.value));
            } else {
                _fmt_snprintf_or_err(result, err, buffer, size, "ret void");
            }
            break;
        }
        case IR_ALLOCA:
            _fmt_snprintf_or_err(result, err, buffer, size, "%s = alloca %s", FMT_VAR(a, 512, instr->value.alloca.result), FMT_TYPE(b, 512, instr->value.alloca.type));
            break;
        case IR_LOAD:
            _fmt_snprintf_or_err(result, err, buffer, size, "%s = load %s", FMT_VAR(a, 512, instr->value.unary_op.result), FMT_VAL(b, 512, instr->value.unary_op.operand));
            break;
        case IR_STORE:
            _fmt_snprintf_or_err(result, err, buffer, size, "store %s, %s", FMT_VAL(a, 512, instr->value.store.value), FMT_VAL(b, 512, instr->value.store.ptr));
            break;
        case IR_MEMCPY:
            _fmt_snprintf_or_err(result, err, buffer, size, "memcpy %s, %s, %s", FMT_VAL(a, 512, instr->value.memcpy.dest), FMT_VAL(b, 512, instr->value.memcpy.src), FMT_VAL(c, 512, instr->value.memcpy.length));
            break;
        case IR_MEMSET:
            _fmt_snprintf_or_err(result, err, buffer, size, "memset %s, %s, %s", FMT_VAL(a, 512, instr->value.memset.ptr), FMT_VAL(b, 512, instr->value.memset.value), FMT_VAL(c, 512, instr->value.memset.length));
            break;
        case IR_GET_ARRAY_ELEMENT_PTR:
            _fmt_snprintf_or_err(result, err, buffer, size, "%s = get_array_element_ptr %s, %s", FMT_VAR(a, 512, instr->value.binary_op.result), FMT_VAL(b, 512, instr->value.binary_op.left), FMT_VAL(c, 512, instr->value.binary_op.right));
            break;
        case IR_GET_STRUCT_MEMBER_PTR:
            _fmt_snprintf_or_err(result, err, buffer, size, "%s = get_struct_member_ptr %s, %s", FMT_VAR(a, 512, instr->value.binary_op.result), FMT_VAL(b, 512, instr->value.binary_op.left), FMT_VAL(c, 512, instr->value.binary_op.right));
            break;
        case IR_TRUNC:
            _fmt_snprintf_or_err(result, err, buffer, size, "%s = trunc %s", FMT_VAR(a, 512, instr->value.unary_op.result), FMT_VAL(b, 512, instr->value.unary_op.operand));
            break;
        case IR_EXT:
            _fmt_snprintf_or_err(result, err, buffer, size, "%s = ext %s", FMT_VAR(a, 512, instr->value.unary_op.result), FMT_VAL(b, 512, instr->value.unary_op.operand));
            break;
        case IR_FTOI:
            _fmt_snprintf_or_err(result, err, buffer, size, "%s = ftoi %s", FMT_VAR(a, 512, instr->value.unary_op.result), FMT_VAL(b, 512, instr->value.unary_op.operand));
            break;
        case IR_ITOF:
            _fmt_snprintf_or_err(result, err, buffer, size, "%s = itof %s", FMT_VAR(a, 512, instr->value.unary_op.result), FMT_VAL(b, 512, instr->value.unary_op.operand));
            break;
        case IR_ITOP:
            _fmt_snprintf_or_err(result, err, buffer, size, "%s = itop %s", FMT_VAR(a, 512, instr->value.unary_op.result), FMT_VAL(b, 512, instr->value.unary_op.operand));
            break;
        case IR_PTOI:
            _fmt_snprintf_or_err(result, err, buffer, size, "%s = ptoi %s", FMT_VAR(a, 512, instr->value.unary_op.result), FMT_VAL(b, 512, instr->value.unary_op.operand));
            break;
        case IR_BITCAST:
            _fmt_snprintf_or_err(result, err, buffer, size, "%s = bitcast %s", FMT_VAR(a, 512, instr->value.unary_op.result), FMT_VAL(b, 512, instr->value.unary_op.operand));
            break;
        case IR_SWITCH: {
            _fmt_snprintf_or_err(result, err, buffer, size, "switch %s, %s, { ", FMT_VAL(a, 512, instr->value.switch_.value), instr->value.switch_.default_label);
            for (int i = 0; i < instr->value.switch_.cases.size; i += 1) {
                ir_switch_case_t switch_case = instr->value.switch_.cases.buffer[i];
                _fmt_snprintf_or_err(result, err, buffer, size, "%s%lli: %s", i == 0 ? "" : ", ",
                    switch_case.const_val.value.i, switch_case.label);
            }
            _fmt_snprintf_or_err(result, err, buffer, size, " }");
            break;
        }
        case IR_VA_START: {
            _fmt_snprintf_or_err(result, err, buffer, size, "va_start %s", FMT_VAL(a, 512, instr->value.va.va_list_src));
            break;
        }
        case IR_VA_END: {
            _fmt_snprintf_or_err(result, err, buffer, size, "va_end %s", FMT_VAL(a, 512, instr->value.va.va_list_src));
            break;
        }
        case IR_VA_ARG: {
            _fmt_snprintf_or_err(result, err, buffer, size, "%s = va_arg %s, %s", FMT_VAR(a, 512, instr->value.va.result), FMT_VAL(b, 512, instr->value.va.va_list_src), FMT_TYPE(c, 512, instr->value.va.type));
            break;
        }
        case IR_VA_COPY: {
            _fmt_snprintf_or_err(result, err, buffer, size, "va_copy %s, %s", FMT_VAL(a, 512, instr->value.va.va_list_src), FMT_VAL(b, 512, instr->value.va.va_list_dest));
            break;
        }
    }

    return result;
err:
    return -1;
}

void ir_print_module(FILE *file, const ir_module_t *module) {
    char temp[1024];
    // Print globals
    for (size_t i = 0; i < module->globals.size; i++) {
        ir_global_t *global = module->globals.buffer[i];
        if (global->initialized) {
            char value[512];
            ir_fmt_const(value, sizeof(value), global->value);
            fprintf(file, "global %s %s = %s\n", FMT_TYPE(temp, 1024, global->type), global->name, value);
        } else {
            fprintf(file, "global %s %s\n", FMT_TYPE(temp, 1024, global->type), global->name);
        }
    }

    // Print functions
    for (size_t i = 0; i < module->functions.size; i++) {
        ir_function_definition_t *function = module->functions.buffer[i];
        fprintf(file, "function %s %s", function->name, FMT_TYPE(temp, 1024, function->type));
        fprintf(file, " {\n");
        char buffer[1024];
        for (size_t j = 0; j < function->body.size; j++) {
            const ir_instruction_t *instr = &function->body.buffer[j];
            ir_fmt_instr(buffer, sizeof(buffer) / sizeof(char), instr);
            fprintf(file, "    %s\n", buffer);
        }
        fprintf(file, "}\n");
    }
}
