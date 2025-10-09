#ifndef C_COMPILER_IR_H
#define C_COMPILER_IR_H

#include <stdbool.h>
#include <stdint.h>

#include "utils/hashtable.h"
#include "utils/vectors.h"

/// # IR Definition
///
/// Internal intermediate representation (IR) for the compiler.
/// This is a simple typed three address code representation of the input program. After parsing and building the
/// ast, the compiler will convert the ast into this IR representation, which could either be directly lowered to
/// machine code, or converted into SSA form for optimizations.
///
/// The IR is generated during/after typechecking/semantic analysis and is assumed to be well formed by later phases,
/// so there is no need to check for type errors or other semantic errors while processing the IR.
///
/// ## IR Types
/// The IR uses a simplified type system that is a subset of the full C type system from the parser and semantic analysis
/// phases.
///
/// The IR types are:
/// - void
/// - bool
/// - i8, i16, i32, i6: signed integers
/// - u8, u16, u32, u64: unsigned integers
/// - f32, f64: floating point numbers
/// - ptr(pointee): pointer to another type
/// - array(length, type): fixed size array of another type
/// - struct(fields): struct/union with named fields of various types
/// - function(return_type, args): function with return type and argument types
///
/// ## IR Values
///
/// There are two kinds of IR values, constants and variables. Both have an associated IR type, constants have a value
/// and variables have a name. Local variable names start with `%` and global variable names start with `@`.
///
/// Examples:
/// - Constant integer: `i32 42`
/// - Local variable: f64`%1`
/// - Global variable: i32 `@foo`
///
/// ## IR Operations
///
/// ### Assignment
///
/// The assignment operation takes a value and assigns it to a variable. The value and variable must have the same type.
/// ASSIGN a, b - Assign the value of b to a `a = b`
///
/// ### Arithmetic
///
/// The arithmetic operations all take two operands and produce a result. Unless otherwise noted the operands can
/// be either integer or floating point values, but must both have the same type (promotions and conversions are
/// explicitly represented as instructions in the IR). The result of the operation has the same type as the operands.
///
/// The arithmetic operations are:
/// - ADD: Add a and b - `i8 c = add i32 a, i32 b`
/// - SUB: Subtract b from a - `i32 c = sub i32 a, i32 b`
/// - MUL: Multiply a and b: `i32 c = mul i32 a, i32 b`
/// - DIV: Divide a by b: `i32 c = div i32 a, i32 b`
/// - MOD: Remainder of a divided by b: `i32 c = mod i32 a, i32 b`
///
/// ### Bitwise
///
/// The binary bitwise operations all take two integer operands and produce an integer result.
///
/// The binary bitwise operations are:
/// - AND: Bitwise AND of a and b: `c = a & b`
/// - OR:  Bitwise OR of a and b: `c = a | b`
/// - SHL: Shift a left by b bits `c = a << b`
/// - SHR: Shift a right by b bits `c = a >> b`
///    + If a is signed, the b most significant bits are filled with the value of the sign bit
///    + If a is unsigned, the b most significant bits are filled with 0
///    + a and b do not need to have the same width, the result (c) will have the same width as a
/// - XOR: Bitwise XOR of a and b: `c = a ^ b`
///
/// There is also one unary bitwise operation:
/// - NOT: Bitwise NOT of a: `b = ~a`
///
/// ### Comparison
///
/// The comparison operations all take two operands and produce a boolean result. The operands can be either integer
/// or floating point values, but must both have the same type.
///
/// The comparison operations are:
/// - EQ: Equal: `c = a == b`
/// - NE: Not equal: `c = a != b`
/// - LT: Less than: `c = a < b`
/// - LE: Less than or equal: `c = a <= b`
/// - GT: Greater than: `c = a > b`
/// - GE: Greater than or equal: `c = a >= b`
///
/// ### Control Flow
///
/// - br label - Unconditional branch to label
/// - br_cond a, label - Conditional branch to label if a is true
///    + a must be a boolean value
/// - call: Call function f with arguments a, b, ... `c = f(a, b)`
///    + The arguments must match the function signature
///    + The return value assignment is optional, and is only valid if the function returns a non-void value
///    + Variadic functions are supported
/// - ret a - Return a from the function
///    + a must have the same type as the function return type
///
/// ### Memory
/// - alloca: Allocate memory on the stack for a value of type a `*i32 b = alloca i32 a`
///    + a must be a non-void type
///    + b will be a pointer to the allocated memory
/// - load: Load the value from a pointer a into b `b = *a`
///    + a must be a pointer type
///    + b must be the same type as the pointer target
/// - store: Store the value b into the pointer a `*a = b`
///    + a must be a pointer type
///    + b must be the same type as the pointer target
/// - memcpy: Copy intrinsic, copy the value from a to b `memcpy dest, src`
///    + If src and dest are different sizes, the smaller size is used.
///    + dest must be a pointer, array, or struct type
/// - memset: Memset intrinsic, fills the destination with len elements of the provided value `memset dest, val, len`
///    + dest must be a pointer or array
/// - get_array_element_pointer: Get a pointer to an element in an array: `get_array_element_pointer ptr, index, result`
///    + ptr must be a pointer
///    + index must be an integer
///    + result must be a pointer, with the same type as ptr
/// - get_struct_member_pointer: Get a pointer to a field of a struct: `get_struct_element_pointer ptr, index, result`
///    + ptr must be a pointer to a struct or union
///    + index refers to the target field of the struct, there must be a field with that index
///    + index must be a constant integer
///    + result must be a pointer, with a pointee type of the field being accessed
///
/// ### Type Conversion
///
/// - trunc a, b - Truncate a to the specified size `i8 b = trunc i32 a`
///    + a and b can either be both integer types or both floating point types
///    + The result type must be smaller than the type of a
/// - ext a, b - Extend a to the specified size `i32 b = ext i16 a`
///     + a and b can either be both integer types or both floating point types
///     + The result type must be larger than the type of a
///     + If a is signed, the sign bit is extended, otherwise the new bits are filled with 0
/// - ftoi a, b - Convert a floating point value to an integer `i32 b = ftoi f32 a`
/// - itof a, b - Convert an integer value to a floating point `f32 b = itof i32 a`
/// - ptoi a, b - Convert a pointer to an integer `i64 b = ptoi *i32 a`
/// - itop a, b - Convert an integer to a pointer `*i32 b = itop i64 a`
/// - bitcast a, type - Bitcast a to the specified type `b = (type) a`
///     + a and type must have the same size
///     + b will have the same bit pattern as a

typedef enum IrTypeKind {
    IR_TYPE_VOID,
    IR_TYPE_BOOL,
    IR_TYPE_I8,
    IR_TYPE_I16,
    IR_TYPE_I32,
    IR_TYPE_I64,
    IR_TYPE_U8,
    IR_TYPE_U16,
    IR_TYPE_U32,
    IR_TYPE_U64,
    IR_TYPE_F32,
    IR_TYPE_F64,
    IR_TYPE_PTR,
    IR_TYPE_ARRAY,
    IR_TYPE_STRUCT_OR_UNION,
    IR_TYPE_FUNCTION
} ir_type_kind_t;

struct IrType;

typedef struct IrTypePtr {
    const struct IrType *pointee;
} ir_type_ptr_t;

typedef struct IrTypeArray {
    const struct IrType *element;
    size_t length;
} ir_type_array_t;

typedef struct IrTypeFunction {
    const struct IrType *return_type;
    const struct IrType **params;
    size_t num_params;
    bool is_variadic;
} ir_type_function_t;

typedef struct IrStructField {
    int index;
    const char* name;
    const struct IrType *type;
} ir_struct_field_t;

VEC_DEFINE(IrStructFieldPtrVector, ir_struct_field_ptr_vector_t, ir_struct_field_t*)

typedef struct IrTypeStruct {
    const char* id;
    ir_struct_field_ptr_vector_t fields;
    hash_table_t field_map; // Map from field name -> field
    bool is_union;
} ir_type_struct_t;

typedef struct IrType {
    ir_type_kind_t kind;
    union {
        ir_type_ptr_t ptr;
        ir_type_array_t array;
        ir_type_function_t function;
        ir_type_struct_t struct_or_union;
    } value;
} ir_type_t;

// Common IR types
static const ir_type_t IR_VOID = { .kind = IR_TYPE_VOID };
static const ir_type_t IR_BOOL = { .kind = IR_TYPE_BOOL };
static const ir_type_t IR_I8 =  { .kind = IR_TYPE_I8 };
static const ir_type_t IR_I16 = { .kind = IR_TYPE_I16 };
static const ir_type_t IR_I32 = { .kind = IR_TYPE_I32 };
static const ir_type_t IR_I64 = { .kind = IR_TYPE_I64 };
static const ir_type_t IR_U8 =  { .kind = IR_TYPE_U8 };
static const ir_type_t IR_U16 = { .kind = IR_TYPE_U16 };
static const ir_type_t IR_U32 = { .kind = IR_TYPE_U32 };
static const ir_type_t IR_U64 = { .kind = IR_TYPE_U64 };
static const ir_type_t IR_F32 = { .kind = IR_TYPE_F32 };
static const ir_type_t IR_F64 = { .kind = IR_TYPE_F64 };
static const ir_type_t IR_PTR_CHAR = { .kind = IR_TYPE_PTR, .value.ptr = { .pointee = &IR_I8 } };

// Some architectures do have byte sizes that aren't 8-bits, but we will only support 8-bit bytes to keep things
// simple. Most code assumes that char/uint8_t are exactly 8-bits anyways (the posix standard requires CHAR_BIT == 8).
#define BYTE_SIZE 8

/**
 * Architecture details needed for ir codegen.
 * The ir itself is architecture agnostic, with the exception of pointer <--> int conversions due to
 * different pointer sizes, and type sizes (mostly potential differences in unpacked struct/union types due to
 * alignment requirements).
 * A few things are needed to correctly translate the input program into ir:
 * 1. What ir type each c primitive (e.g. char/short/int/long) maps to
 * 2. The size of a pointer on the target arch
 * 3. The alignment requirements for different ir types (for struct/union padding)
 *    In practice, types are all self-aligned (alignment = size of the type in bytes) on the most common architectures
 *    (x86, arm, risc-v, mips (?)), though this is probably not always true for dsps/embedded systems.
 */
typedef struct IrArch {
    /**
     * Architecture name, e.g. "x86_64" or "aarch64"
     */
    const char *name;
    /**
     * Alternate architecture name
     */
    const char *alt_name;
    /**
     * IR type corresponding to the c type `unsigned char`
     */
    const ir_type_t *uchar;
    /**
     * IR type corresponding to the c type `signed char`
     */
    const ir_type_t *schar;
    /**
     * IR type corresponding to the c type `unsigned short`
     */
    const ir_type_t *ushort;
    /**
     * IR type corresponding to the c type `signed short`
     */
    const ir_type_t *sshort;
    /**
     * IR type corresponding to the c type `unsigned int`
     */
    const ir_type_t *uint;
    /**
     * IR type corresponding to the c type `signed int`
     */
    const ir_type_t *sint;
    /**
     * IR type corresponding to the c type `unsigned long`
     */
    const ir_type_t *ulong;
    /**
     * IR type corresponding to the c type `signed long`
     */
    const ir_type_t *slong;
    /**
     * IR type corresponding to the c type `unsigned long long`
     */
    const ir_type_t *ulonglong;
    /**
     * IR type corresponding to the c type `signed long long`
     */
    const ir_type_t *slonglong;
    /**
     * IR type corresponding to the c type `float`
     */
    const ir_type_t *_float;
    /**
     * IR type corresponding to the c type `double`
     */
    const ir_type_t *_double;
    /**
     * IR type corresponding to the c type `long double`
     */
    const ir_type_t *_long_double;
    /**
     * The unsigned int type with the same size as a pointer on the target architecture.
     * Needed to determine the size of pointers mainly.
     */
    const ir_type_t *ptr_int_type;
    /**
     * Alignment of 8-bit integers (in bytes)
     */
    const int int8_alignment;
    /**
     * Alignment of 16-bit integers (in bytes)
     */
    const int int16_alignment;
    /**
     * Alignment of 32-bit integers (in bytes)
     */
    const int int32_alignment;
    /**
     * Alignment of 64-bit integers (in bytes)
     */
    const int int64_alignment;
    /**
     * Alignment of 32-bit floats (in bytes)
     */
    const int f32_alignment;
    /**
     * Alignment of 64-bit floats (in bytes)
     */
    const int f64_alignment;
} ir_arch_t;

typedef enum IrOpcode {
    IR_NOP,

    /* Arithmetic */
    IR_ADD,
    IR_SUB,
    IR_MUL,
    IR_DIV,
    IR_MOD,

    /* Assignment */
    IR_ASSIGN,

    /* Bitwise */
    IR_AND,
    IR_OR,
    IR_SHL,
    IR_SHR,
    IR_XOR,
    IR_NOT,

    /* Comparison */
    IR_EQ,
    IR_NE,
    IR_LT,
    IR_LE,
    IR_GT,
    IR_GE,

    /* Control Flow */
    IR_BR,
    IR_BR_COND,
    IR_CALL,
    IR_RET,
    IR_SWITCH,

    /* Memory */
    IR_ALLOCA,
    IR_LOAD,
    IR_STORE,
    IR_MEMCPY,
    IR_MEMSET,
    IR_GET_ARRAY_ELEMENT_PTR,
    IR_GET_STRUCT_MEMBER_PTR,

    /* Type Conversion */
    IR_TRUNC,
    IR_EXT,
    IR_FTOI,
    IR_ITOF,
    IR_PTOI,
    IR_ITOP,
    IR_BITCAST,

    /* Vararg Support */
    IR_VA_START,
    IR_VA_END,
    IR_VA_ARG,
    IR_VA_COPY,
} ir_opcode_t;

typedef enum IrValueKind {
    IR_VALUE_CONST,
    IR_VALUE_VAR,
} ir_value_kind_t;

typedef struct IrConst {
    enum {
        IR_CONST_ARRAY,
        IR_CONST_INT,
        IR_CONST_FLOAT,
        IR_CONST_STRING,
        IR_CONST_STRUCT,
        IR_CONST_GLOBAL_POINTER,
    } kind;
    const ir_type_t *type;
    union {
        struct {
            struct IrConst *values;
            size_t length;
        } array;
        long long i;
        long double f;
        const char* s;
        struct {
            bool is_union;
            int union_field_index;
            struct IrConst *fields;
            size_t length;
        } _struct;
        const char* global_name;
    } value;
} ir_const_t;

typedef struct IrVar {
    const char* name;
    const ir_type_t *type;
} ir_var_t;

typedef struct IrValue {
    ir_value_kind_t kind;
    // union {
        ir_const_t constant;
        ir_var_t var;
    // };
} ir_value_t;

typedef struct IrGlobal {
    const char* name;
    const ir_type_t *type;
    bool initialized;
    ir_const_t value;
} ir_global_t;

typedef struct IrSwitchCase {
    ir_const_t const_val;
    char *label;
} ir_switch_case_t;
VEC_DEFINE(IrSwitchCaseVector, ir_switch_case_vector_t, ir_switch_case_t)

typedef struct IrInstruction {
    ir_opcode_t opcode;
    const char* label;
    union {
        struct {
            ir_value_t value;
            ir_var_t result;
        } assign;
        /**
         * Inputs/outputs for all binary operations that produce a result
         * Those are:
         * - arithmetic: add, sub, mul, div, mod
         * - logical: and, or, shl, shr, xor
         * - comparison: eq, ne, lt, le, gt, ge
         * - memory: get_array_element_ptr, get_struct_member_ptr
         */
        struct {
            ir_value_t left;
            ir_value_t right;
            ir_var_t result;
        } binary_op;
        /**
         * Inputs/outputs for all unary operations that produce a result
         * Those are:
         * - bitwise: not
         * - type conversion: trunc, ext, ftoi, itof, ptoi, itop, bitcast
         * - memory: load
         */
        struct {
            ir_value_t operand;
            ir_var_t result;
        } unary_op;
        struct {
            const char* label;
            bool has_cond;
            ir_value_t cond;
        } branch;
        struct {
            ir_value_t function;
            ir_value_t *args;
            size_t num_args;
            ir_var_t *result; // optional
        } call;
        struct {
            bool has_value;
            ir_value_t value;
        } ret;
        struct {
            const ir_type_t *type;
            ir_var_t result;
        } alloca;
        struct {
            ir_value_t ptr;
            ir_value_t value;
        } store;
        struct {
            ir_value_t ptr;
            ir_value_t value;
            ir_value_t length;
        } memset;
        struct {
            ir_value_t dest;
            ir_value_t src;
            ir_value_t length;
        } memcpy;
        struct {
            ir_value_t value;
            ir_switch_case_vector_t cases;
            char *default_label;
        } switch_;
        struct {
            ir_value_t va_list_src;  // va list ptr
            ir_value_t va_list_dest; // for va copy
            ir_var_t result;         // for va_arg
            ir_type_t *type;        // for va_arg
        } va;
    } value;
} ir_instruction_t;

typedef struct IrInstructionVector {
    ir_instruction_t *buffer;
    size_t size;
    size_t capacity;
} ir_instruction_vector_t;

typedef struct IrFunctionDefinition {
    const char* name;
    const ir_type_t *type;
    ir_var_t *params;
    size_t num_params;
    bool is_variadic;
    ir_instruction_vector_t body;
} ir_function_definition_t;

typedef struct IrInstructionPtrVector {
    ir_instruction_t **buffer;
    size_t size;
    size_t capacity;
} ir_instruction_ptr_vector_t;

typedef struct IrFunctionPtrVector {
    ir_function_definition_t **buffer;
    size_t size;
    size_t capacity;
} ir_function_ptr_vector_t;

typedef struct IrGlobalPtrVector {
    ir_global_t **buffer;
    size_t size;
    size_t capacity;
} ir_global_ptr_vector_t;

void append_ir_instruction(ir_instruction_vector_t *vector, ir_instruction_t instruction);

typedef struct IrModule {
    const char *name;
    const ir_arch_t *arch;
    ir_global_ptr_vector_t globals;
    // Struct/union type definitions
    // Map of name (IR name, not source name) -> type
    hash_table_t type_map;
    ir_function_ptr_vector_t functions;
} ir_module_t;

bool ir_types_equal(const ir_type_t *a, const ir_type_t *b);

/**
 * Get the size of an IR type in bits.
 * @param type IR type
 * @return size in bits
 */
ssize_t ir_size_of_type_bits(const ir_arch_t *arch, const ir_type_t *type);

/**
 * Get the size of an IR type in bytes.
 * @param type IR type
 * @return size in bytes
 */
ssize_t ir_size_of_type_bytes(const ir_arch_t *arch, const ir_type_t *type);

/**
 * Get the alignment of an IR type in bytes.
 * @param arch target architecture
 * @param type ir type
 * @return alignment of the type in bytes
 */
int ir_get_alignment(const ir_arch_t *arch, const ir_type_t *type);

const ir_type_t *ir_get_type_of_value(ir_value_t value);
bool ir_is_integer_type(const ir_type_t *type);
bool ir_is_signed_integer_type(const ir_type_t *type);
bool ir_is_float_type(const ir_type_t *type);
bool ir_is_scalar_type(const ir_type_t *type);

/**
 * Add padding to a struct so that all members start at an offset that is a multiple of their
 * architecture-specific alignment requirements.
 * For more details on how struct padding works, read "The Lost Art of Structure Packing" by Eric S. Raymond:
 * http://www.catb.org/esr/structure-packing/.
 * @param arch target architecture
 * @param source un-padded struct type (will not be modified)
 * @return Returns a new struct type with padding between members
 */
ir_type_struct_t ir_pad_struct(const ir_arch_t *arch, const ir_type_struct_t *source);

typedef struct IrValidationError {
    const struct IrInstruction *instruction;
    const char *message;
} ir_validation_error_t;

typedef struct IrValidationErrorVector {
    ir_validation_error_t *buffer;
    size_t size;
    size_t capacity;
} ir_validation_error_vector_t;

ir_validation_error_vector_t ir_validate_function(const ir_module_t *module, const ir_function_definition_t *function);

/**
 * Returns a pointer to each variable reference in the instruction.
 * @param instr Instruction to search
 * @param uses Array to store the found uses (must be large enough to store all uses)
 * @param uses_max Length of the uses array
 * @return Number of uses found in the provided instruction
 */
size_t ir_get_uses(ir_instruction_t *instr, ir_var_t **uses, size_t uses_max);

/**
 * Returns a pointer to the variable definition in the instruction.
 * @param instr Instruction to search
 * @return Pointer to the variable definition in the instruction
 *         (or NULL if the instruction does not contain any definitions)
 */
ir_var_t *ir_get_def(ir_instruction_t *instr);

/**
 * Sorts the global definitions of a module so that dependencies are defined before their uses.
 * @param module
 */
void ir_sort_global_definitions(ir_module_t *module);

#endif //C_COMPILER_IR_H
