#include "target.h"
#include "ir/arch.h"

#include <string.h>

const ir_arch_t IR_ARCH_X86 = {
    .name = "i386",
    .alt_name = "x86",
    .uchar = &IR_U8,
    .schar = &IR_I8,
    .ushort = &IR_U16,
    .sshort = &IR_I16,
    .uint = &IR_U32,
    .sint = &IR_I32,
    .ulong = &IR_U32, // or u64?
    .slong = &IR_I32, // or i64?
    .ulonglong = &IR_U64,
    .slonglong = &IR_I64,
    ._float = &IR_F32,
    ._double = &IR_F64,
    ._long_double = &IR_F64, // TODO: F80?
    .ptr_int_type = &IR_I32,
    .int8_alignment = 1,
    .int16_alignment = 2,
    .int32_alignment = 4,
    .int64_alignment = 8,
    .f32_alignment = 4,
    .f64_alignment = 8,
};

const ir_arch_t IR_ARCH_X86_64 = {
    .name = "amd64",
    .alt_name = "x86_64",
    .uchar = &IR_U8,
    .schar = &IR_I8,
    .ushort = &IR_U16,
    .sshort = &IR_I16,
    .uint = &IR_U32,
    .sint = &IR_I32,
    .ulong = &IR_U64,
    .slong = &IR_I64,
    .ulonglong = &IR_U64, // TODO: U128?
    .slonglong = &IR_I64, // TODO: I128?
    ._float = &IR_F32,
    ._double = &IR_F64,
    ._long_double = &IR_F64, // TODO: F80
    .ptr_int_type = &IR_I64,
    .int8_alignment = 1,
    .int16_alignment = 2,
    .int32_alignment = 4,
    .int64_alignment = 8,
    .f32_alignment = 4,
    .f64_alignment = 8,
};

const ir_arch_t IR_ARCH_ARM32 = {
    .name = "arm32",
    .alt_name = "aarch32",
    .uchar = &IR_U8,
    .schar = &IR_I8,
    .ushort = &IR_U16,
    .sshort = &IR_I16,
    .uint = &IR_U32,
    .sint = &IR_I32,
    .ulong = &IR_U32, // or u64?
    .slong = &IR_I32, // or i64?
    .ulonglong = &IR_U64, // TODO: U128
    .slonglong = &IR_I64, // TODO: I128
    ._float = &IR_F32,
    ._double = &IR_F64,
    ._long_double = &IR_F64,
    .ptr_int_type = &IR_I32,
    .int8_alignment = 1,
    .int16_alignment = 2,
    .int32_alignment = 4,
    .int64_alignment = 8,
    .f32_alignment = 4,
    .f64_alignment = 8,
};

const ir_arch_t IR_ARCH_ARM64 = {
    .name = "arm64",
    .alt_name = "aarch64",
    .uchar = &IR_U8,
    .schar = &IR_I8,
    .ushort = &IR_U16,
    .sshort = &IR_I16,
    .uint = &IR_U32,
    .sint = &IR_I32,
    .ulong = &IR_U64, // or u64?
    .slong = &IR_I64, // or i64?
    .ulonglong = &IR_U64, // TODO: U128?
    .slonglong = &IR_I64, // TODO: I128?
    ._float = &IR_F32,
    ._double = &IR_F64,
    ._long_double = &IR_F64,
    .ptr_int_type = &IR_I64,
    .int8_alignment = 1,
    .int16_alignment = 2,
    .int32_alignment = 4,
    .int64_alignment = 8,
    .f32_alignment = 4,
    .f64_alignment = 8,
};

const ir_arch_t *ARCH_LIST[] = {
    &IR_ARCH_X86,
    &IR_ARCH_X86_64,
    &IR_ARCH_ARM32,
    &IR_ARCH_ARM64,
};

const ir_arch_t *get_ir_arch(const target_t *target) {
    if (target == NULL) return NULL;

    for (int i = 0; i < sizeof (ARCH_LIST) / sizeof (ir_arch_t*); i += 1) {
        if (strcmp(target->arch, ARCH_LIST[i]->name) == 0 || strcmp(target->arch, ARCH_LIST[i]->alt_name) == 0)
            return ARCH_LIST[i];
    }

    return NULL;
}