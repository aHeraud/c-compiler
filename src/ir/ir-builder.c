#include "util/vectors.h"
#include "ir-builder.h"

typedef struct IrFunctionBuilder {
    ir_instruction_vector_t instructions;
} ir_function_builder_t;

ir_function_builder_t *IrCreateFunctionBuilder() {
    ir_function_builder_t *builder = malloc(sizeof(ir_function_builder_t));
    size_t initial_capacity = 64;
    builder->instructions.buffer = malloc(initial_capacity * sizeof(ir_instruction_t));
    builder->instructions.size = 0;
    builder->instructions.capacity = initial_capacity;
    return builder;
}

void IrFinalizeFunctionBuilder(ir_function_builder_t *builder, ir_function_definition_t *function) {
    ir_instruction_vector_t *instructions = &builder->instructions;
    VEC_SHRINK(instructions, ir_instruction_t);
    function->instructions = instructions->buffer;
    function->num_instructions = instructions->size;
}

void IrBuildAssign(ir_function_builder_t *builder, ir_value_t value, ir_var_t result) {
    ir_instruction_t instruction = {
            .opcode = IR_ASSIGN,
            .assign = {
                    .value = value,
                    .result = result,
            }
    };
    append_ir_instruction(&builder->instructions, instruction);
}

void IrBuildAdd(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result) {
    ir_instruction_t instruction = {
            .opcode = IR_ADD,
            .add = {
                    .left = left,
                    .right = right,
                    .result = result,
            }
    };
    append_ir_instruction(&builder->instructions, instruction);
}

void IrBuildSub(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result) {
    ir_instruction_t instruction = {
            .opcode = IR_SUB,
            .sub = {
                    .left = left,
                    .right = right,
                    .result = result,
            }
    };
    append_ir_instruction(&builder->instructions, instruction);
}

void IrBuildMul(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result) {
    ir_instruction_t instruction = {
            .opcode = IR_MUL,
            .mul = {
                    .left = left,
                    .right = right,
                    .result = result,
            }
    };
    append_ir_instruction(&builder->instructions, instruction);
}

void IrBuildDiv(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result) {
    ir_instruction_t instruction = {
            .opcode = IR_DIV,
            .div = {
                    .left = left,
                    .right = right,
                    .result = result,
            }
    };
    append_ir_instruction(&builder->instructions, instruction);
}

void IrBuildMod(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result) {
    ir_instruction_t instruction = {
            .opcode = IR_MOD,
            .mod = {
                    .left = left,
                    .right = right,
                    .result = result,
            }
    };
    append_ir_instruction(&builder->instructions, instruction);
}

void IrBuildAnd(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_AND,
        .and = {
            .left = left,
            .right = right,
            .result = result,
        }
    };
    append_ir_instruction(&builder->instructions, instruction);
}

void IrBuildOr(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_OR,
        .or = {
            .left = left,
            .right = right,
            .result = result,
        }
    };
    append_ir_instruction(&builder->instructions, instruction);
}

void IrBuildShl(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_SHL,
        .shl = {
            .left = left,
            .right = right,
            .result = result,
        }
    };
    append_ir_instruction(&builder->instructions, instruction);
}

void IrBuildShr(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_SHR,
        .shr = {
            .left = left,
            .right = right,
            .result = result,
        }
    };
    append_ir_instruction(&builder->instructions, instruction);
}

void IrBuildXor(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_XOR,
        .xor = {
            .left = left,
            .right = right,
            .result = result,
        }
    };
    append_ir_instruction(&builder->instructions, instruction);
}

void IrBuildNot(ir_function_builder_t *builder, ir_value_t value, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_NOT,
        .not = {
            .value = value,
            .result = result,
        }
    };
    append_ir_instruction(&builder->instructions, instruction);
}

void IrBuildBr(ir_function_builder_t *builder, const char *label) {
    ir_instruction_t instruction = {
            .opcode = IR_BR,
            .br = {
                    .label = label,
            }
    };
    append_ir_instruction(&builder->instructions, instruction);
}

void IrBuildBrCond(ir_function_builder_t *builder, ir_value_t cond, const char *label) {
    ir_instruction_t instruction = {
            .opcode = IR_BR_COND,
            .br_cond = {
                    .cond = cond,
                    .label = label,
            }
    };
    append_ir_instruction(&builder->instructions, instruction);
}

void IrBuildReturnValue(ir_function_builder_t *builder, ir_value_t value) {
    ir_instruction_t instruction = {
        .opcode = IR_RET,
        .ret = {
            .has_value = true,
            .value = value
        }
    };
    append_ir_instruction(&builder->instructions, instruction);
}

void IrBuildReturnVoid(ir_function_builder_t *builder) {
    ir_instruction_t instruction = {
        .opcode = IR_RET,
        .ret = {
            .has_value = false,
        }
    };
    append_ir_instruction(&builder->instructions, instruction);
}

void IrBuildAlloca(ir_function_builder_t *builder, const ir_type_t *type, ir_var_t result) {
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
    append_ir_instruction(&builder->instructions, instruction);
}

void IrBuildLoad(ir_function_builder_t *builder, ir_value_t ptr, ir_var_t result) {
    if (ptr.kind == IR_VALUE_CONST) assert(ptr.constant.type->kind == IR_TYPE_PTR && "Load pointer must be a pointer");
    if (ptr.kind == IR_VALUE_VAR) assert(ptr.var.type->kind == IR_TYPE_PTR && "Load pointer must be a pointer");
    // TODO: the result type must be the element type of the pointer

    ir_instruction_t instruction = {
        .opcode = IR_LOAD,
        .load = {
            .ptr = ptr,
            .result = result,
        }
    };
    append_ir_instruction(&builder->instructions, instruction);
}

void IrBuildStore(ir_function_builder_t *builder, ir_value_t ptr, ir_value_t value) {
    if (ptr.kind == IR_VALUE_CONST) assert(ptr.constant.type->kind == IR_TYPE_PTR && "Store pointer must be a pointer");
    if (ptr.kind == IR_VALUE_VAR) assert(ptr.var.type->kind == IR_TYPE_PTR && "Store pointer must be a pointer");
    // TODO: the value type must be the element type of the pointer

    ir_instruction_t instruction = {
        .opcode = IR_STORE,
        .store = {
            .ptr = ptr,
            .value = value,
        }
    };
    append_ir_instruction(&builder->instructions, instruction);
}

void IrBuildTrunc(ir_function_builder_t *builder, ir_value_t value, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_TRUNC,
        .trunc = {
            .value = value,
            .result = result,
        }
    };
    append_ir_instruction(&builder->instructions, instruction);
}

void IrBuildExt(ir_function_builder_t *builder, ir_value_t value, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_EXT,
        .ext = {
            .value = value,
            .result = result,
        }
    };
    append_ir_instruction(&builder->instructions, instruction);
}

void IrBuildFtoI(ir_function_builder_t *builder, ir_value_t value, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_FTOI,
        .ftoi = {
            .value = value,
            .result = result,
        }
    };
}

void IrBuildItoF(ir_function_builder_t *builder, ir_value_t value, ir_var_t result) {
    ir_instruction_t instruction = {
        .opcode = IR_ITOF,
        .itof = {
            .value = value,
            .result = result,
        }
    };
    append_ir_instruction(&builder->instructions, instruction);
}

void IrBuildBitCast(ir_function_builder_t *builder, ir_value_t value, const ir_type_t *type) {
ir_instruction_t instruction = {
        .opcode = IR_BITCAST,
        .bitcast = {
            .value = value,
            .type = type,
        }
    };
    append_ir_instruction(&builder->instructions, instruction);
}