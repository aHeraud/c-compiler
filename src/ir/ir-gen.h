#ifndef C_COMPILER_IR_GEN_H
#define C_COMPILER_IR_GEN_H

#include "ast.h"
#include "ir/ir.h"
#include "../errors.h"

typedef struct IrGenResult {
    ir_module_t *module;
    compilation_error_vector_t errors;
} ir_gen_result_t;

ir_gen_result_t generate_ir(const translation_unit_t *translation_unit, const ir_arch_t *arch);

#endif //C_COMPILER_IR_GEN_H
