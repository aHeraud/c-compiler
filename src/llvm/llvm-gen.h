#ifndef C_COMPILER_LLVM_GEN_H
#define C_COMPILER_LLVM_GEN_H
#include "target.h"

void llvm_gen_module(const ir_module_t *module, const target_t *target, const char* output_filename);

#endif //C_COMPILER_LLVM_GEN_H
