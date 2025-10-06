#ifndef C_COMPILER_IR_TARGET_H
#define C_COMPILER_IR_TARGET_H

#include "target.h"
#include "ir/ir.h"

extern const ir_arch_t IR_ARCH_X86;
extern const ir_arch_t IR_ARCH_X86_64;
extern const ir_arch_t IR_ARCH_ARM32;
extern const ir_arch_t IR_ARCH_ARM64;

const ir_arch_t *get_ir_arch(const target_t *target);

#endif
