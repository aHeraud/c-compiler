# c-compiler

A C compiler implementation targeting C99, written entirely in C99.

## Goals

Currently, the main goal is to get the compiler to the point where it can successfully compile itself.

## Building

### Dependencies
* CMake
* LLVM
* Python 3
* A C compiler (not this one, it can't compile itself _yet_)

### CMake Setup And Building

CMake will automatically generate the build project (platform dependent, on Linux the default will be to create a makefile).

```bash
# Running in the root of the project
# Generate the build system in a sub-directory (`cmake`)
cmake -B cmake ./

# On Linux/Unix systems, the default generator will create a makefile in the cmake build directory
# You can invoke make from the project root to build the project like so:
make -C cmake
```

## Compiling a Program

The compilation of a source program is divided into several phases that happen sequentially:

1. Scanning the input, handling pre-processing directives, and parsing the source program to generate an abstract syntax tree (AST)
2. Semantic analysis and intermediate representation (IR) code-generation
3. LLVM IR code-generation from the internal IR
4. Backend code generation for the target platform

Currently only phases 1-3 are performed by the compiler driver, which will emit a LLVM IR module. This module must then be compiled by LLVM (llc), then assembled (and also linked if you want an executable binary).

Here's a simple example on how to compile and run a program. We will generate the LLVM IR module using this compiler, compile the LLVM IR into assembly using `llc`, then use `gcc` to assemble and link an executable which we will then run:

```
achille@DESKTOP-JJFVU4U:~$ cat hello-world.c
// hello-world.c
int printf(const char *fmt, ...);
int main() {
    printf("hello world!\n");
}
achille@DESKTOP-JJFVU4U:~$ ./cc hello-world.c
achille@DESKTOP-JJFVU4U:~$ llc hello-world.ll
achille@DESKTOP-JJFVU4U:~$ gcc -s hello-world.s
achille@DESKTOP-JJFVU4U:~$ ./a.out
hello world!
achille@DESKTOP-JJFVU4U:~$
```

## Features

### Preprocessor

There is no separate preprocessor, instead the pre-processor is combined with the lexer and will automatically handle 
preprocessor directives and macro replacement.

Preprocessor feature support:
- [x] #define (partial support for function like macros)
- [ ] Variadic macros
- [x] #undef
- [x] #include
- [ ] #ifdef, #ifndef
- [ ] #if, #else, #elseif

### Intermediate Representation (IR)

The compiler has its own internal representation, which is produced as the output of the first code generation phase. 
This IR can then be lowered to another IR (implemented by the LLVM backend), or directly to the target machine code 
(not currently implemented).

For more details on the IR, see [docs/ir.md](docs/ir.md).

#### IR Example

```c
int printf(const char *fmt, ...);

// Swap two integers without using a temp variables
void swap(int *x, int *y) {
    *x = *y ^ *x;
    *y = *x ^ *y;
    *x = *y ^ *x;
}

int main() {
    int a = 5;
    int b = 12;
    printf("a: %d, b: %d\n", a, b);
    swap(&a, &b);
    printf("a: %d, b: %d\n", a, b);
    return 0;
}
```

After building with the `--emit-ir` flag set, we get the following:

```
global *[i8;14] @0 = [i8;14] "a: %d, b: %d\n"
global *[i8;14] @1 = [i8;14] "a: %d, b: %d\n"
function swap (*i32, *i32) -> void {
    **i32 %0 = alloca *i32
    **i32 %1 = alloca *i32
    store *i32 x, **i32 %0
    store *i32 y, **i32 %1
    *i32 %2 = load **i32 %1
    i32 %3 = load *i32 %2
    *i32 %4 = load **i32 %0
    i32 %5 = load *i32 %4
    i32 %6 = xor i32 %3, i32 %5
    i32 %7 = i32 %6
    *i32 %8 = load **i32 %0
    store i32 %7, *i32 %8
    *i32 %9 = load **i32 %0
    i32 %10 = load *i32 %9
    *i32 %11 = load **i32 %1
    i32 %12 = load *i32 %11
    i32 %13 = xor i32 %10, i32 %12
    i32 %14 = i32 %13
    *i32 %15 = load **i32 %1
    store i32 %14, *i32 %15
    *i32 %16 = load **i32 %1
    i32 %17 = load *i32 %16
    *i32 %18 = load **i32 %0
    i32 %19 = load *i32 %18
    i32 %20 = xor i32 %17, i32 %19
    i32 %21 = i32 %20
    *i32 %22 = load **i32 %0
    store i32 %21, *i32 %22
    ret void
}
function main () -> i32 {
    *i32 %0 = alloca i32
    *i32 %1 = alloca i32
    store i32 5, *i32 %0
    store i32 12, *i32 %1
    *i8 %2 = bitcast *[i8;14] @0
    i32 %3 = load *i32 %0
    i32 %4 = load *i32 %1
    i32 %5 = call printf(*i8 %2, i32 %3, i32 %4)
    call swap(*i32 %0, *i32 %1)
    *i8 %6 = bitcast *[i8;14] @1
    i32 %7 = load *i32 %0
    i32 %8 = load *i32 %1
    i32 %9 = call printf(*i8 %6, i32 %7, i32 %8)
    ret i32 0
}
```

### Optimizations

As you can see from the above example, the IR generated is quite suboptimal. Currently only constant-folding and some
very basic un-reachable code removal are implemented. While generating highly-optimized code is a non-goal, I do plan on
implementing some other optimizations, like promoting memory values to registers if they are never aliased, and possibly
common sub expression elimination.

#### IR Forms

The IR has 3 different forms:
1. Linear (as seen in the above example)

   The most simple form, similar to writing a program in assembly.
2. Control Flow Graph

   The code is broken up into [basic blocks](https://en.wikipedia.org/wiki/Basic_block), which are combined  to form a 
   directed graph. The compiler can output a graphical representation of the generated control flow graphs with the
   `--emit-ir-cfg` flag ([example](docs/cfg-example.svg)).
3. [Static Single-Assignment (SSA)](https://en.wikipedia.org/wiki/Static_single-assignment_form)
   
   Similar to the control flow graph form, with the additional restriction that each variable can only be assigned once.

There are utilities for converting the IR between the three forms, see src/ir/cfg.c and src/ir/ssa.c.

### Unimplemented Language Features

Specifiers/Qualifiers:
- [ ] Inline
- [ ] Const (accepted, but not enforced)
- [ ] Restrict
- [ ] Volatile
- [ ] Typedef 
- [ ] Extern
- [ ] Static
- [x] Auto
- [ ] Register

Control Flow:
- [x] While loop
- [x] Do-while loop
- [x] For loop
- [x] Break
- [x] Continue
- [x] Labeled statements, Goto
- [ ] Switch statements

Types:
- [x] Char
- [x] Short
- [x] Int
- [x] Long
- [x] Long long
- [x] Float
- [x] Double
- [ ] Long double (accepted, but not extended precision even on supported platforms)
- [ ] Complex
- [x] Arrays
- [ ] Variable-length arrays
- [ ] Constant array initializers
- [ ] Designated array initializers
- [x] Structs
- [ ] Struct initializers
- [ ] Enums
- [x] Pointers

Expressions/Operators:
- [x] Assignment expressions (excluding compound assignments, e.g. `x += 1`)
- [x] Conditional expressions (e.g. ternary operator `?`)
- [x] Increment/decrement
- [x] Logical and/or (`&&`, `||`)
- [x] Bitwise and/or (`&`, `|`)
- [x] Exclusive or (`^`)
- [x] Equality (`==`, `!=`)
- [x] Relational operators (`<`, `>`, `<=`, `>=`)
- [x] Left/right shift
- [x] Multiplicative operators (`*`, `/`, `%`)
- [ ] Cast expressions
- [x] Size of
- [x] Address of (`&`)
- [x] Dereference (`*`)
- [x] Bitwise not (`~`)
- [x] Logical not (`!`)
- [x] Function calls
- [x] Variadic functions
- [ ] Function pointers 

Control Flow:
- [x] For loop
- [x] While loop
- [x] Do while loop
- [x] Break
- [x] Continue
- [x] If/else
