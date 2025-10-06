#ifndef C_COMPILER_BACKEND_H
#define C_COMPILER_BACKEND_H

#include "target.h"
#include "ir/ir.h"

void llvm_gen_module(const ir_module_t *module, const target_t *target, const ir_arch_t *arch, const char* output_filename);

#endif //C_COMPILER_BACKEND_H
