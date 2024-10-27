#include "ir-builder.h"

struct IrInstructionNode {
    ir_instruction_t instruction;
    ir_instruction_node_t *prev;
    ir_instruction_node_t *next;
};

struct IrFunctionBuilder {
    /**
     * Number of instructions in the list.
     */
    size_t length;
    ir_instruction_node_t *head;
    ir_instruction_node_t *tail;
    /**
     * Pointer to the node _after_ which we will insert the next instruction.
     * If this is NULL, the next instruction will be appended at the beginning of the list.
     */
    ir_instruction_node_t *cursor;
};

ir_function_builder_t *ir_builder_create(void) {
    ir_function_builder_t *builder = malloc(sizeof(ir_function_builder_t));
    *builder = (ir_function_builder_t) {
        .length = 0,
        .head = NULL,
        .tail = NULL,
        .cursor = NULL,
    };
    return builder;
}

ir_instruction_vector_t ir_builder_finalize(ir_function_builder_t *builder) {
    ir_instruction_vector_t instructions = {
        .buffer = malloc(builder->length * sizeof(ir_instruction_t)),
        .size = 0,
        .capacity = builder->length,
    };

    ir_instruction_node_t *node = builder->head;
    while (node != NULL) {
        append_ir_instruction(&instructions, node->instruction);
        ir_instruction_node_t *next = node->next;
        free(node);
        node = next;
    }

    free(builder);

    return instructions;
}

void ir_builder_destroy(ir_function_builder_t *builder) {
    ir_instruction_node_t *node = builder->head;
    while (node != NULL) {
        ir_instruction_node_t *next = node->next;
        free(node);
        node = next;
    }
    free(builder);
}

void ir_builder_position_at_beginning(ir_function_builder_t *builder) {
    builder->cursor = NULL;
}

void ir_builder_position_at_end(ir_function_builder_t *builder) {
    builder->cursor = builder->tail;
}

void ir_builder_position_before(ir_function_builder_t *builder, ir_instruction_node_t *node) {
    builder->cursor = node->prev;
}

void ir_builder_position_after(ir_function_builder_t *builder, ir_instruction_node_t *node) {
    builder->cursor = node;
}

ir_instruction_node_t *ir_builder_get_position(const ir_function_builder_t *builder) {
    return builder->cursor;
}

void ir_builder_clear_after(ir_function_builder_t *builder, ir_instruction_node_t *position) {
    if (position == NULL) return;

    ir_instruction_node_t *node = position->next;
    while (node != NULL) {
        ir_instruction_node_t *next = node->next;
        free(node);
        node = next;
    }

    position->next = NULL;
}

ir_instruction_t *ir_builder_get_instruction(ir_instruction_node_t *instruction_node) {
    return &instruction_node->instruction;
}

ir_instruction_node_t *ir_builder_insert_instruction(
    ir_function_builder_t *builder, ir_instruction_t instruction
) {
    ir_instruction_node_t *node = malloc(sizeof(ir_instruction_node_t));
    *node = (ir_instruction_node_t) {
        .instruction = instruction,
        .prev = NULL,
        .next = NULL,
    };

    if (builder->cursor == NULL) {
        if (builder->head == NULL) {
            builder->head = node;
            builder->tail = node;
        } else {
            node->next = builder->head;
            builder->head->prev = node;
            builder->head = node;
        }
    } else {
        node->prev = builder->cursor;
        node->next = builder->cursor->next;
        if (builder->cursor->next != NULL) {
            builder->cursor->next->prev = node;
        } else {
            builder->tail = node;
        }
        builder->cursor->next = node;
    }

    builder->length += 1;
    builder->cursor = node;
    return node;
}

ir_instruction_node_t *ir_build_assign(ir_function_builder_t *builder, ir_value_t value, ir_var_t result) {
    ir_instruction_t instruction = {
            .opcode = IR_ASSIGN,
            .value.assign = {
                    .value = value,
                    .result = result,
            }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_nop(ir_function_builder_t *builder, const char* label) {
    ir_instruction_t instruction = {
        .opcode = IR_NOP,
        .label = label,
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_add(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_ADD,
        .value.binary_op = {
            .left = left,
            .right = right,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_sub(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_SUB,
        .value.binary_op = {
            .left = left,
            .right = right,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_mul(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_MUL,
        .value.binary_op = {
            .left = left,
            .right = right,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_div(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_DIV,
        .value.binary_op = {
            .left = left,
            .right = right,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_mod(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_MOD,
        .value.binary_op = {
            .left = left,
            .right = right,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_and(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_AND,
        .value.binary_op = {
            .left = left,
            .right = right,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_or(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_OR,
        .value.binary_op = {
            .left = left,
            .right = right,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_shl(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_SHL,
        .value.binary_op = {
            .left = left,
            .right = right,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_shr(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_SHR,
        .value.binary_op = {
            .left = left,
            .right = right,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_xor(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_XOR,
        .value.binary_op = {
            .left = left,
            .right = right,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_not(ir_function_builder_t *builder, ir_value_t value, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_NOT,
        .value.unary_op = {
            .operand = value,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_eq(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_EQ,
        .value.binary_op = {
            .left = left,
            .right = right,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_ne(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_NE,
        .value.binary_op = {
            .left = left,
            .right = right,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_lt(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_LT,
        .value.binary_op = {
            .left = left,
            .right = right,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_le(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_LE,
        .value.binary_op = {
            .left = left,
            .right = right,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_gt(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_GT,
        .value.binary_op = {
            .left = left,
            .right = right,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_ge(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_GE,
        .value.binary_op = {
            .left = left,
            .right = right,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_br(ir_function_builder_t *builder, const char *label) {
    ir_instruction_t instruction = {
            .opcode = IR_BR,
            .value.branch = {
                .label = label,
                .has_cond = false,
            }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_br_cond(ir_function_builder_t *builder, ir_value_t cond, const char *label) {
    ir_instruction_t instruction = {
        .opcode = IR_BR_COND,
        .value.branch = {
            .cond = cond,
            .has_cond = true,
            .label = label,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_call(ir_function_builder_t *builder, ir_var_t function, ir_value_t *args, size_t num_args, ir_var_t *result) {
    ir_instruction_t instruction = {
        .opcode = IR_CALL,
        .value.call = {
            .function = function,
            .args = args,
            .num_args = num_args,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_ret(ir_function_builder_t *builder, ir_value_t a) {
    ir_instruction_t instruction = {
        .opcode = IR_RET,
        .value.ret = {
            .has_value = true,
            .value = a
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_ret_void(ir_function_builder_t *builder) {
    ir_instruction_t instruction = {
        .opcode = IR_RET,
        .value.ret = {
            .has_value = false,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_switch(ir_function_builder_t *builder, ir_value_t value, char *default_label) {
    ir_instruction_t instruction = {
        .opcode = IR_SWITCH,
        .value.switch_ = {
            .value = value,
            .cases = VEC_INIT,
            .default_label = default_label
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_alloca(ir_function_builder_t *builder, const ir_type_t *type, ir_var_t result) {
    assert(type != NULL && "Alloca type must not be NULL");
    assert(result.type->kind == IR_TYPE_PTR && "Alloca result type must be a pointer");
    // TODO: the result type must be a pointer to the value type

    ir_instruction_t instruction = {
        .opcode = IR_ALLOCA,
        .value.alloca = {
            .type = type,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_load(ir_function_builder_t *builder, ir_value_t ptr, ir_var_t result) {
    if (ptr.kind == IR_VALUE_CONST) assert(ptr.constant.type->kind == IR_TYPE_PTR && "Load pointer must be a pointer");
    if (ptr.kind == IR_VALUE_VAR) assert(ptr.var.type->kind == IR_TYPE_PTR && "Load pointer must be a pointer");

    ir_instruction_t instruction = {
        .opcode = IR_LOAD,
        .value.unary_op = {
            .operand = ptr,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_store(ir_function_builder_t *builder, ir_value_t ptr, ir_value_t value) {
    if (ptr.kind == IR_VALUE_CONST) assert(ptr.constant.type->kind == IR_TYPE_PTR && "Store pointer must be a pointer");
    if (ptr.kind == IR_VALUE_VAR) assert(ptr.var.type->kind == IR_TYPE_PTR && "Store pointer must be a pointer");

    ir_instruction_t instruction = {
        .opcode = IR_STORE,
        .value.store = {
            .ptr = ptr,
            .value = value,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_memcpy(ir_function_builder_t *builder, ir_value_t dest_ptr,  ir_value_t src_ptr, ir_value_t length) {
    const ir_type_t *dest_type = ir_get_type_of_value(dest_ptr);
    const ir_type_t *src_type = ir_get_type_of_value(src_ptr);
    assert(dest_type->kind == IR_TYPE_PTR || dest_type->kind == IR_TYPE_ARRAY);
    assert(src_type->kind == IR_TYPE_PTR || src_type->kind == IR_TYPE_ARRAY);
    ir_instruction_t  instruction = {
        .opcode = IR_MEMCPY,
        .value.memcpy = {
            .dest = dest_ptr,
            .src = src_ptr,
            .length = length,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_get_array_element_ptr(ir_function_builder_t *builder, ir_value_t ptr, ir_value_t index, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_GET_ARRAY_ELEMENT_PTR,
        .value.binary_op = {
            .left = ptr,
            .right = index,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_get_struct_member_ptr(ir_function_builder_t *builder, ir_value_t ptr, int index, ir_var_t result) {
    ir_value_t index_val = {
        .kind = IR_VALUE_CONST,
        .constant = {
            .kind = IR_CONST_INT,
            .type = &IR_I32,
            .value.i = index
        }
    };

    ir_instruction_t instruction = {
        .opcode = IR_GET_STRUCT_MEMBER_PTR,
        .value.binary_op = {
            .left = ptr,
            .right = index_val,
            .result = result
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_trunc(ir_function_builder_t *builder, ir_value_t value, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_TRUNC,
        .value.unary_op = {
            .operand = value,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_ext(ir_function_builder_t *builder, ir_value_t a, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_EXT,
        .value.unary_op = {
            .operand = a,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_ftoi(ir_function_builder_t *builder, ir_value_t a, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_FTOI,
        .value.unary_op = {
            .operand = a,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_itof(ir_function_builder_t *builder, ir_value_t a, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_ITOF,
        .value.unary_op = {
            .operand = a,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_itop(ir_function_builder_t *builder, ir_value_t a, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_ITOP,
        .value.unary_op = {
            .operand = a,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_ptoi(ir_function_builder_t *builder, ir_value_t a, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_PTOI,
        .value.unary_op = {
            .operand = a,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_bitcast(ir_function_builder_t *builder, ir_value_t value, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_BITCAST,
        .value.unary_op = {
            .operand = value,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}
