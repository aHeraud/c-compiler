#include "ir-builder.h"

typedef struct IrInstructionNode {
    ir_instruction_t instruction;
    ir_instruction_node_t *prev;
    ir_instruction_node_t *next;
} ir_instruction_node_t;

typedef struct IrFunctionBuilder {
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
} ir_function_builder_t;

ir_function_builder_t *ir_builder_create() {
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
            .assign = {
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
        .binary_op = {
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
        .binary_op = {
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
        .binary_op = {
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
        .binary_op = {
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
        .binary_op = {
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
        .binary_op = {
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
        .binary_op = {
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
        .binary_op = {
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
        .binary_op = {
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
        .binary_op = {
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
        .unary_op = {
            .operand = value,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_eq(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_EQ,
        .binary_op = {
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
        .binary_op = {
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
        .binary_op = {
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
        .binary_op = {
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
        .binary_op = {
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
        .binary_op = {
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
            .branch = {
                .label = label,
                .has_cond = false,
            }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_br_cond(ir_function_builder_t *builder, ir_value_t cond, const char *label) {
    ir_instruction_t instruction = {
        .opcode = IR_BR_COND,
        .branch = {
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
        .call = {
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
        .ret = {
            .has_value = true,
            .value = a
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_ret_void(ir_function_builder_t *builder) {
    ir_instruction_t instruction = {
        .opcode = IR_RET,
        .ret = {
            .has_value = false,
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
        .alloca = {
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
        .unary_op = {
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
        .store = {
            .ptr = ptr,
            .value = value,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_get_array_element_ptr(ir_function_builder_t *builder, ir_value_t ptr, ir_value_t index, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_GET_ARRAY_ELEMENT_PTR,
        .binary_op = {
            .left = ptr,
            .right = index,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_trunc(ir_function_builder_t *builder, ir_value_t value, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_TRUNC,
        .unary_op = {
            .operand = value,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_ext(ir_function_builder_t *builder, ir_value_t a, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_EXT,
        .unary_op = {
            .operand = a,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_ftoi(ir_function_builder_t *builder, ir_value_t a, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_FTOI,
        .unary_op = {
            .operand = a,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_itof(ir_function_builder_t *builder, ir_value_t a, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_ITOF,
        .unary_op = {
            .operand = a,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_itop(ir_function_builder_t *builder, ir_value_t a, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_ITOP,
        .unary_op = {
            .operand = a,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_ptoi(ir_function_builder_t *builder, ir_value_t a, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_PTOI,
        .unary_op = {
            .operand = a,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}

ir_instruction_node_t *ir_build_bitcast(ir_function_builder_t *builder, ir_value_t value, ir_var_t result) {
ir_instruction_t instruction = {
        .opcode = IR_BITCAST,
        .unary_op = {
            .operand = value,
            .result = result,
        }
    };
    return ir_builder_insert_instruction(builder, instruction);
}