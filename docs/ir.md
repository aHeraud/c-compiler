# IR Definition

Internal intermediate representation (IR) for the compiler.
This is a simple typed three address code representation of the input program. After parsing and building the
ast, the compiler will convert the ast into this IR representation, which could either be directly lowered to
machine code, or converted into SSA form for optimizations.

The IR is generated during/after typechecking/semantic analysis and is assumed to be well formed by later phases,
so there is no need to check for type errors or other semantic errors while processing the IR.

## IR Types
The IR uses a simplified type system that is a subset of the full C type system from the parser and semantic analysis
phases.

The IR types are:
- void
- bool
- i8, i16, i32, i6: signed integers
- u8, u16, u32, u64: unsigned integers
- f32, f64: floating point numbers
- ptr(pointee): pointer to another type
- array(length, type): fixed size array of another type
- struct(fields): struct/union with named fields of various types
- function(return_type, args): function with return type and argument types

## IR Values

There are two kinds of IR values, constants and variables. Both have an associated IR type, constants have a value
and variables have a name. Local variable names start with `%` and global variable names start with `@`.

Examples:
- Constant integer: `i32 42`
- Local variable: f64`%1`
- Global variable: i32 `@foo`

## IR Operations

### Assignment

The assignment operation takes a value and assigns it to a variable. The value and variable must have the same type.
ASSIGN a, b - Assign the value of b to a `a = b`

### Arithmetic

The arithmetic operations all take two operands and produce a result. Unless otherwise noted the operands can
be either integer or floating point values, but must both have the same type (promotions and conversions are
explicitly represented as instructions in the IR). The result of the operation has the same type as the operands.

The arithmetic operations are:
- ADD: Add a and b - `i8 c = add i32 a, i32 b`
- SUB: Subtract b from a - `i32 c = sub i32 a, i32 b`
- MUL: Multiply a and b: `i32 c = mul i32 a, i32 b`
- DIV: Divide a by b: `i32 c = div i32 a, i32 b`
- MOD: Remainder of a divided by b: `i32 c = mod i32 a, i32 b`

### Bitwise

The binary bitwise operations all take two integer operands and produce an integer result.

The binary bitwise operations are:
- AND: Bitwise AND of a and b: `c = a & b`
- OR:  Bitwise OR of a and b: `c = a | b`
- SHL: Shift a left by b bits `c = a << b`
- SHR: Shift a right by b bits `c = a >> b`
   + If a is signed, the b most significant bits are filled with the value of the sign bit
   + If a is unsigned, the b most significant bits are filled with 0
   + a and b do not need to have the same width, the result (c) will have the same width as a
- XOR: Bitwise XOR of a and b: `c = a ^ b`

There is also one unary bitwise operation:
- NOT: Bitwise NOT of a: `b = ~a`

### Comparison

The comparison operations all take two operands and produce a boolean result. The operands can be either integer
or floating point values, but must both have the same type.

The comparison operations are:
- EQ: Equal: `c = a == b`
- NE: Not equal: `c = a != b`
- LT: Less than: `c = a < b`
- LE: Less than or equal: `c = a <= b`
- GT: Greater than: `c = a > b`
- GE: Greater than or equal: `c = a >= b`

### Control Flow

- br label - Unconditional branch to label
- br_cond a, label - Conditional branch to label if a is true
   + a must be a boolean value
- call: Call function f with arguments a, b, ... `c = f(a, b)`
   + The arguments must match the function signature
   + The return value assignment is optional, and is only valid if the function returns a non-void value
   + Variadic functions are supported
- ret a - Return a from the function
   + a must have the same type as the function return type

### Memory
- alloca: Allocate memory on the stack for a value of type a `*i32 b = alloca i32 a`
   + a must be a non-void type
   + b will be a pointer to the allocated memory
- load: Load the value from a pointer a into b `b = *a`
   + a must be a pointer type
   + b must be the same type as the pointer target
- store: Store the value b into the pointer a `*a = b`
   + a must be a pointer type
   + b must be the same type as the pointer target
- memcpy: Copy intrinsic, copy the value from a to b `memcpy dest, src`
   + If src and dest are different sizes, the smaller size is used.
   + dest must be a pointer, array, or struct type
- memset: Memset intrinsic, fills the destination with len elements of the provided value `memset dest, val, len`
   + dest must be a pointer or array
- get_array_element_pointer: Get a pointer to an element in an array: `get_array_element_pointer ptr, index, result`
   + ptr must be a pointer
   + index must be an integer
   + result must be a pointer, with the same type as ptr
- get_struct_member_pointer: Get a pointer to a field of a struct: `get_struct_element_pointer ptr, index, result`
   + ptr must be a pointer to a struct or union
   + index refers to the target field of the struct, there must be a field with that index
   + index must be a constant integer
   + result must be a pointer, with a pointee type of the field being accessed

### Type Conversion

- trunc a, b - Truncate a to the specified size `i8 b = trunc i32 a`
   + a and b can either be both integer types or both floating point types
   + The result type must be smaller than the type of a
- ext a, b - Extend a to the specified size `i32 b = ext i16 a`
    + a and b can either be both integer types or both floating point types
    + The result type must be larger than the type of a
    + If a is signed, the sign bit is extended, otherwise the new bits are filled with 0
- ftoi a, b - Convert a floating point value to an integer `i32 b = ftoi f32 a`
- itof a, b - Convert an integer value to a floating point `f32 b = itof i32 a`
- ptoi a, b - Convert a pointer to an integer `i64 b = ptoi *i32 a`
- itop a, b - Convert an integer to a pointer `*i32 b = itop i64 a`
- bitcast a, type - Bitcast a to the specified type `b = (type) a`
    + a and type must have the same size
    + b will have the same bit pattern as a