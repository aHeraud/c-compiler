#ifndef C_COMPILER_IR_BUILDER_H
#define C_COMPILER_IR_BUILDER_H

#include "ir/ir.h"

typedef struct IrInstructionNode ir_instruction_node_t;
typedef struct IrFunctionBuilder ir_function_builder_t;

ir_function_builder_t *ir_builder_create();
ir_instruction_vector_t ir_builder_finalize(ir_function_builder_t *builder);
void ir_builder_destroy(ir_function_builder_t *builder);

/* Utilities to set the position of the next instruction */

/**
 * Positions the builder at the beginning of the function.
 * @param builder
 */
void ir_builder_position_at_beginning(ir_function_builder_t *builder);

/**
 * Positions the builder at the end of the function.
 * This is the default position when the builder is created.
 * @param builder
 */
void ir_builder_position_at_end(ir_function_builder_t *builder);

/**
 * Positions the builder before the specified instruction.
 * @param builder
 * @param node
 */
void ir_builder_position_before(ir_function_builder_t *builder, ir_instruction_node_t *instruction);

/**
 * Positions the builder after the specified instruction.
 * @param builder
 * @param node
 */
void ir_builder_position_after(ir_function_builder_t *builder, ir_instruction_node_t *instruction);

/**
 * Get the current position of the builder.
 * @param builder
 */
ir_instruction_node_t *ir_builder_get_position(const ir_function_builder_t *builder);

/**
 * Clear all instructions after the specified position.
 * @param builder
 * @param position
 */
void ir_builder_clear_after(ir_function_builder_t *builder, ir_instruction_node_t *position);

/* No-op */
ir_instruction_node_t *ir_build_nop(ir_function_builder_t *builder, const char* label);

/* Arithmetic */
ir_instruction_node_t *ir_build_add(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);
ir_instruction_node_t *ir_build_sub(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);
ir_instruction_node_t *ir_build_mul(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);
ir_instruction_node_t *ir_build_div(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);
ir_instruction_node_t *ir_build_mod(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);

/* Assignment */
ir_instruction_node_t *ir_build_assign(ir_function_builder_t *builder, ir_value_t value, ir_var_t result);

/* Bitwise */
ir_instruction_node_t *ir_build_and(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);
ir_instruction_node_t *ir_build_or(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);
ir_instruction_node_t *ir_build_shl(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);
ir_instruction_node_t *ir_build_shr(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);
ir_instruction_node_t *ir_build_xor(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);
ir_instruction_node_t *ir_build_not(ir_function_builder_t *builder, ir_value_t value, ir_var_t result);

/* Comparison */
ir_instruction_node_t *ir_build_eq(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);
ir_instruction_node_t *ir_build_ne(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);
ir_instruction_node_t *ir_build_lt(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);
ir_instruction_node_t *ir_build_le(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);
ir_instruction_node_t *ir_build_gt(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);
ir_instruction_node_t *ir_build_ge(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);

/* Control Flow */
ir_instruction_node_t *ir_build_br(ir_function_builder_t *builder, const char *label);
ir_instruction_node_t *ir_build_br_cond(ir_function_builder_t *builder, ir_value_t cond, const char *label);
ir_instruction_node_t *ir_build_call(ir_function_builder_t *builder, ir_var_t function, ir_value_t *args, size_t num_args, ir_var_t *result);
ir_instruction_node_t *ir_build_ret(ir_function_builder_t *builder, ir_value_t a);
ir_instruction_node_t *ir_build_ret_void(ir_function_builder_t *builder);

/* Memory */
ir_instruction_node_t *ir_build_alloca(ir_function_builder_t *builder, const ir_type_t *type, ir_var_t result);
ir_instruction_node_t *ir_build_load(ir_function_builder_t *builder, ir_value_t ptr, ir_var_t result);
ir_instruction_node_t *ir_build_store(ir_function_builder_t *builder, ir_value_t ptr, ir_value_t value);

/* Type Conversion */
ir_instruction_node_t *ir_build_trunc(ir_function_builder_t *builder, ir_value_t value, ir_var_t result);
ir_instruction_node_t *ir_build_ext(ir_function_builder_t *builder, ir_value_t a, ir_var_t result);
ir_instruction_node_t *ir_build_ftoi(ir_function_builder_t *builder, ir_value_t a, ir_var_t result);
ir_instruction_node_t *ir_build_itof(ir_function_builder_t *builder, ir_value_t a, ir_var_t result);
ir_instruction_node_t *ir_build_itop(ir_function_builder_t *builder, ir_value_t a, ir_var_t result);
ir_instruction_node_t *ir_build_ptoi(ir_function_builder_t *builder, ir_value_t a, ir_var_t result);
ir_instruction_node_t *ir_build_bitcast(ir_function_builder_t *builder, ir_value_t value, ir_var_t result);

#endif //C_COMPILER_IR_BUILDER_H
