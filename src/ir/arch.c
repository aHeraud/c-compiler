#include "ir/arch.h"

const ir_arch_t IR_ARCH_X86 = (ir_arch_t) {
    .name = "i386",
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
};

const ir_arch_t IR_ARCH_X86_64 = {
    .name = "amd64",
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
};

const ir_arch_t IR_ARCH_ARM32 = {
    .name = "arm32",
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
};

const ir_arch_t IR_ARCH_ARM64 = {
    .name = "arm64",
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
};
