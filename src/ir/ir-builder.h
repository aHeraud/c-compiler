#ifndef C_COMPILER_IR_BUILDER_H
#define C_COMPILER_IR_BUILDER_H

#include "ir/ir.h"

typedef struct IrFunctionBuilder ir_function_builder_t;

ir_function_builder_t *IrCreateFunctionBuilder();
void IrFinalizeFunctionBuilder(ir_function_builder_t *builder, ir_function_definition_t *function);

/* Arithmetic */
void IrBuildAdd(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);
void IrBuildSub(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);
void IrBuildMul(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);
void IrBuildDiv(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);
void IrBuildMod(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);

/* Assignment */
void IrBuildAssign(ir_function_builder_t *builder, ir_value_t value, ir_var_t result);

/* Bitwise */
void IrBuildAnd(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);
void IrBuildOr(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);
void IrBuildShl(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);
void IrBuildShr(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);
void IrBuildXor(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);
void IrBuildNot(ir_function_builder_t *builder, ir_value_t value, ir_var_t result);

/* Comparison */
void IrBuildEq(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);
void IrBuildNe(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);
void IrBuildLt(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);
void IrBuildLe(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);
void IrBuildGt(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);
void IrBuildGe(ir_function_builder_t *builder, ir_value_t left, ir_value_t right, ir_var_t result);

/* Control Flow */
void IrBuildBr(ir_function_builder_t *builder, const char *label);
void IrBuildBrCond(ir_function_builder_t *builder, ir_value_t cond, const char *label);
//void IrBuildCall(ir_function_builder_t *builder, ?);
void IrBuildReturnValue(ir_function_builder_t *builder, ir_value_t a);
void IrBuildReturnVoid(ir_function_builder_t *builder);

/* Memory */
void IrBuildAlloca(ir_function_builder_t *builder, const ir_type_t *type, ir_var_t result);
void IrBuildLoad(ir_function_builder_t *builder, ir_value_t ptr, ir_var_t result);
void IrBuildStore(ir_function_builder_t *builder, ir_value_t ptr, ir_value_t value);

/* Type Conversion */
void IrBuildTrunc(ir_function_builder_t *builder, ir_value_t value, ir_var_t result);
void IrBuildExt(ir_function_builder_t *builder, ir_value_t a, ir_var_t result);
void IrBuildFtoI(ir_function_builder_t *builder, ir_value_t a, ir_var_t result);
void IrBuildItoF(ir_function_builder_t *builder, ir_value_t a, ir_var_t result);
void IrBuildBitCast(ir_function_builder_t *builder, ir_value_t a, const ir_type_t *type);

#endif //C_COMPILER_IR_BUILDER_H
