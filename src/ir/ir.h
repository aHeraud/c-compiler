#ifndef C_COMPILER_IR_H
#define C_COMPILER_IR_H

//// # IR Definition
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
/// - struct(fields): struct with named fields of various types
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

#include "types.h"

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
    IR_TYPE_STRUCT,
    IR_TYPE_FUNCTION
} ir_type_kind_t;

typedef struct IrType ir_type_t;

typedef struct IrTypePtr {
    const struct IrType *pointee;
} ir_type_ptr_t;

typedef struct IrTypeArray {
    const struct IrType *element;
    size_t length;
} ir_type_array_t;

typedef struct IrTypeFunction {
    const ir_type_t *return_type;
    const ir_type_t **params;
    size_t num_params;
    bool is_variadic;
} ir_type_function_t;

typedef struct IrType {
    ir_type_kind_t kind;
    union {
        ir_type_ptr_t ptr;
        ir_type_array_t array;
        ir_type_function_t function;
    };
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
static const ir_type_t IR_PTR_CHAR = { .kind = IR_TYPE_PTR, .ptr = { .pointee = &IR_I8 } };

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

    /* Memory */
    IR_ALLOCA,
    IR_LOAD,
    IR_STORE,
    IR_MEMCPY,

    /* Type Conversion */
    IR_TRUNC,
    IR_EXT,
    IR_FTOI,
    IR_ITOF,
    IR_PTOI,
    IR_ITOP,
    IR_BITCAST
} ir_opcode_t;

typedef enum IrValueKind {
    IR_VALUE_CONST,
    IR_VALUE_VAR,
} ir_value_kind_t;

typedef struct IrConst {
    enum {
        IR_CONST_INT,
        IR_CONST_FLOAT,
        IR_CONST_STRING
    } kind;
    const ir_type_t *type;
    union {
        long long i;
        long double f;
        const char* s;
    };
} ir_const_t;

typedef struct IrVar {
    const char* name;
    const ir_type_t *type;
} ir_var_t;

typedef struct IrValue {
    ir_value_kind_t kind;
    union {
        ir_const_t constant;
        ir_var_t var;
    };
} ir_value_t;

typedef struct IrGlobal {
    const char* name;
    const ir_type_t *type;
    bool initialized;
    ir_const_t value;
} ir_global_t;

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
         * - memory: load, memcpy
         *
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
            ir_var_t function;
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
    };
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
    const char* name;
    ir_global_ptr_vector_t globals;
    ir_function_ptr_vector_t functions;
} ir_module_t;

bool ir_types_equal(const ir_type_t *a, const ir_type_t *b);

/**
 * Get the size of an IR type.
 * @param type IR type
 * @return size in bits
 */
ssize_t size_of_type(const ir_type_t *type);

const ir_type_t *ir_get_type_of_value(ir_value_t value);
bool ir_is_integer_type(const ir_type_t *type);
bool ir_is_signed_integer_type(const ir_type_t *type);
bool ir_is_float_type(const ir_type_t *type);
bool ir_is_scalar_type(const ir_type_t *type);

typedef struct IrValidationError {
    const struct IrInstruction *instruction;
    const char *message;
} ir_validation_error_t;

typedef struct IrValidationErrorVector {
    ir_validation_error_t *buffer;
    size_t size;
    size_t capacity;
} ir_validation_error_vector_t;

ir_validation_error_vector_t ir_validate_function(const ir_function_definition_t *function);

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

#endif //C_COMPILER_IR_H
