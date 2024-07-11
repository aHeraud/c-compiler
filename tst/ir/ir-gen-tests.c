#include <CUnit/Basic.h>

#include "ir/ir-gen.h"
#include "ir/fmt.h"

#include "../tests.h"
#include "../test-common.h"

/// IR generation tests
/// These are extremely fragile, since they rely on the output of the IR generation matching excatly.
/// This should probably be refactored in the future.

#define PARSE(input)                                                             \
    lexer_global_context_t lexer_context = create_lexer_context();               \
    lexer_t lexer = linit("path/to/file", input, strlen(input), &lexer_context); \
    parser_t parser = pinit(lexer);                                              \
    translation_unit_t program;                                                  \
    CU_ASSERT_TRUE_FATAL(parse(&parser, &program))

// TODO: fix this abomination
#define ASSERT_IR_INSTRUCTIONS_EQ(function, _body) \
do { \
    bool size_equals = function->body.size == sizeof(_body)/sizeof(_body[0]);   \
    bool body_equals = true;                                                    \
    if (size_equals) {                                                          \
        for (int i = 0; i < function->body.size; i += 1) {                      \
            const char *instruction =                                           \
                ir_fmt_instr(alloca(512), 512, &function->body.buffer[i]);      \
            if (strcmp(_body[i], instruction) != 0) {                           \
                body_equals = false;                                            \
                fprintf(stderr, "Expected (at index %u): %s, Actual: %s\n",     \
                    i, _body[i], instruction);                                  \
                break;                                                          \
            }                                                                   \
        }                                                                       \
    }                                                                           \
    if (!body_equals || !size_equals) {                                         \
        fprintf(stderr, "Expected and actual function body not equal:\n");      \
        fprintf(stderr, "\nExpected:\n");                                       \
        for (int i = 0; i < (sizeof(_body)/sizeof(_body[0])); i += 1) {         \
            fprintf(stderr, "%s\n", _body[i]);                                  \
        }                                                                       \
        fprintf(stderr, "\nActual:\n");                                         \
        for (int i = 0; i < function->body.size; i += 1) {                      \
            char instr[512];                                                    \
            fprintf(stderr, "%s\n",                                             \
                ir_fmt_instr(instr, 512, &function->body.buffer[i]));           \
        }                                                                       \
        CU_FAIL()                                                               \
    }                                                                           \
} while (0)

void test_ir_gen_basic() {
    const char* input = "int main() {\n"
                        "    return 0;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, (const char*[]) {
        "ret i32 0"
    });
}

void test_ir_gen_add_simple() {
    const char* input = "float main() {\n"
                        "    float a = 1.0f;\n"
                        "    float b = 2.0f;\n"
                        "    return a + b;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*f32 %0 = alloca f32",
        "*f32 %1 = alloca f32",
        "store f32 1.000000, *f32 %0",
        "store f32 2.000000, *f32 %1",
        "f32 %2 = load *f32 %0",
        "f32 %3 = load *f32 %1",
        "f32 %4 = add f32 %2, f32 %3",
        "ret f32 %4"
    }));
}

void test_ir_gen_add_i32_f32() {
    const char* input = "int main() {\n"
                        "    int a = 1;\n"
                        "    float b = 2.0f;\n"
                        "    return a + b;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*i32 %0 = alloca i32",
        "*f32 %1 = alloca f32",
        "store i32 1, *i32 %0",
        "store f32 2.000000, *f32 %1",
        "i32 %2 = load *i32 %0",
        "f32 %3 = load *f32 %1",
        "f32 %4 = itof i32 %2",
        "f32 %5 = add f32 %4, f32 %3",
        "i32 %6 = ftoi f32 %5",
        "ret i32 %6"
    }));
}

void test_ir_gen_add_constants() {
    const char* input = "float main() {\n"
                        "    return 1.0f + 2.0f;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "ret f32 3.000000"
    }));
}

void test_ir_gen_sub_constants() {
    const char* input = "int main() {\n"
                        "    return 3 - 5;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "ret i32 -2"
    }));
}

void test_ir_gen_multiply_constants() {
    const char* input = "int main() {\n"
                        "    return 3 * 5;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "ret i32 15"
    }));
}

void test_ir_gen_divide_constants() {
    const char* input = "int main() {\n"
                        "    return 64 / 8;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "ret i32 8"
    }));
}

void test_ir_gen_divide_by_zero_float_constants() {
    const char* input = "float main() {\n"
                        "    return 1.0f / 0.0f;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, (const char*[]) {
        "ret f32 inf"
    });
}

void test_ir_gen_divide_by_zero_integer_constants() {
    const char* input = "int main() {\n"
                        "    return 1 / 0;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);

    // TODO: warning, undefined result
    // For now we just make sure this doesn't crash
}

void test_ir_gen_mod_constants() {
    const char* input = "int main() {\n"
                        "    return 5 % 3;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "ret i32 2"
    }));
}

void test_ir_gen_left_shift_constants() {
    const char* input = "int main() {\n"
                        "    return 4 << 2;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "ret i32 16"
    }));
}

void test_ir_gen_right_shift_constants() {
    const char* input = "int main() {\n"
                        "    return 3 >> 1;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "ret i32 1"
    }));
}

void test_ir_gen_logic_and_constants_1() {
    const char* input = "int main() {\n"
                        "    return 1 && 0;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "ret i32 0"
    }));
}

void test_ir_gen_logic_and_constants_2() {
    const char* input = "int main() {\n"
                        "    return 0 && 1;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "ret i32 0"
    }));
}

void test_ir_gen_logic_and_constants_3() {
    const char* input = "int main() {\n"
                        "    return 1 && 1;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "ret i32 1"
    }));
}

void test_ir_gen_logic_or_constants_1() {
    const char* input = "int main() {\n"
                        "    return 1 || 0;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "ret i32 1"
    }));
}

void test_ir_gen_logic_or_constants_2() {
    const char* input = "int main() {\n"
                        "    return 0 || 1;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "ret i32 1"
    }));
}

void test_ir_gen_logic_or_constants_3() {
    const char* input = "int main() {\n"
                        "    return 0 || 0;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "ret i32 0"
    }));
}

void test_ir_gen_ternary_expression_constants_1() {
    const char* input = "int main() {\n"
                        "    return 1 ? 2 : 3;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "ret i32 2"
    }));
}

void test_ir_gen_ternary_expression_constants_2() {
    const char* input = "int main() {\n"
                        "    return 0 ? 2 : 3;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "ret i32 3"
    }));
}

void test_ir_gen_addr_of_variable() {
    const char* input = "int main() {\n"
                        "    int a = 1;\n"
                        "    int *b = &a;\n"
                        "    return 0;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*i32 %0 = alloca i32",
        "**i32 %1 = alloca *i32",
        "store i32 1, *i32 %0",
        "store *i32 %0, **i32 %1",
        "ret i32 0"
    }));
}

void test_ir_gen_indirect_load() {
    const char* input = "int foo(int *a) {\n"
                        "    return *a;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "**i32 %0 = alloca *i32",
        "store *i32 a, **i32 %0",
        "*i32 %1 = load **i32 %0",
        "i32 %2 = load *i32 %1",
        "ret i32 %2"
    }));
}

void test_ir_gen_indirect_store() {
    const char* input = "int foo(int *a) {\n"
                        "    *a = 1;\n"
                        "    return 0;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "**i32 %0 = alloca *i32",
        "store *i32 a, **i32 %0",
        "i32 %1 = i32 1",
        "*i32 %2 = load **i32 %0",
        "store i32 %1, *i32 %2",
        "ret i32 0"
    }));
}

void test_ir_gen_array_load_constant_index() {
    // we use 1 as the index, because a[0] would be optimized away during ir generation
    const char* input = "int foo() {\n"
                        "    int a[2];\n"
                        "    int b = a[1];\n"
                        "}";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*[i32;2] %0 = alloca [i32;2]",
        "*i32 %1 = alloca i32",
        "*i32 %2 = get_array_element_ptr *[i32;2] %0, i32 1",
        "i32 %3 = load *i32 %2",
        "store i32 %3, *i32 %1",
        "ret i32 0"
    }));
}

void test_ir_gen_array_store_constant_index() {
    // we use 1 as the index, because a[0] would be optimized away during ir generation
    const char* input = "int foo() {\n"
                        "    int a[2];\n"
                        "    a[1] = 10;\n"
                        "}";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*[i32;2] %0 = alloca [i32;2]",
        "*i32 %1 = get_array_element_ptr *[i32;2] %0, i32 1",
        "i32 %2 = i32 10",
        "store i32 %2, *i32 %1",
        "ret i32 0"
    }));
}

void test_ir_gen_array_load_variable_index() {
    const char* input = "int foo() {\n"
                        "    int a[2];\n"
                        "    int i = 0;\n"
                        "    int b = a[i];\n"
                        "}";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*[i32;2] %0 = alloca [i32;2]",
        "*i32 %1 = alloca i32",
        "*i32 %2 = alloca i32",
        "store i32 0, *i32 %1",
        "i32 %3 = load *i32 %1",
        "*i32 %4 = get_array_element_ptr *[i32;2] %0, i32 %3",
        "i32 %5 = load *i32 %4",
        "store i32 %5, *i32 %2",
        "ret i32 0"
    }));
}

void test_ir_gen_array_index_on_ptr() {
    const char* input = "int foo(int *a) {\n"
                        "    return a[0];\n"
                        "}";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "**i32 %0 = alloca *i32",
        "store *i32 a, **i32 %0",
        "*i32 %1 = load **i32 %0",
        "*i32 %2 = get_array_element_ptr *i32 %1, i32 0",
        "i32 %3 = load *i32 %2",
        "ret i32 %3"
    }));
}

void test_ir_gen_if_else_statement() {
    const char* input =
        "int main(int a) {\n"
        "    int x;\n"
        "    if (a) {\n"
        "        x = 1;\n"
        "    } else {\n"
        "        x = 2;\n"
        "    }\n"
        "    return x;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*i32 %0 = alloca i32",
        "*i32 %1 = alloca i32",
        "store i32 a, *i32 %0",
        "i32 %2 = load *i32 %0",
        "bool %3 = eq i32 %2, i32 0",
        "br bool %3, l0",
        "i32 %4 = i32 1",
        "store i32 %4, *i32 %1",
        "br l1",
        "l0: nop",
        "i32 %5 = i32 2",
        "store i32 %5, *i32 %1",
        "l1: nop",
        "i32 %6 = load *i32 %1",
        "ret i32 %6"
    }));
}

void test_ir_gen_call_expr_returns_void() {
    const char* input =
        "void foo(int a);\n"
        "int main() {\n"
        "    foo(1);\n"
        "    return 0;\n"
        "}\n";

    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "call foo(i32 1)",
        "ret i32 0"
    }));
}

void test_ir_gen_function_arg_promotion() {
    const char* input =
        "void foo(double a);\n"
        "int main() {\n"
        "    float a = 1.0f;\n"
        "    foo(a);\n"
        "    return 0;\n"
        "}\n";

    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*f32 %0 = alloca f32",
        "store f32 1.000000, *f32 %0",
        "f32 %1 = load *f32 %0",
        "f64 %2 = ext f32 %1",
        "call foo(f64 %2)",
        "ret i32 0"
    }));
}

void test_ir_gen_varargs_call() {
    // Test calling a function with a variable number of arguments
    // Important! The varargs arguments are _NOT_ converted to the type of the last named argument, they are just
    // passed as is.
    const char* input =
        "void foo(int a, ...);\n"
        "int main() {\n"
        "    int a = 1;\n"
        "    float b = 1.0;\n"
        "    char* c = \"hello\";\n"
        "    foo(a, b, c);\n"
        "    return 0;\n"
        "}\n";

    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*i32 %0 = alloca i32",
        "*f32 %1 = alloca f32",
        "**i8 %3 = alloca *i8",
        "store i32 1, *i32 %0",
        "store f32 1.000000, *f32 %1",
        "*i8 %4 = bitcast *[i8;6] @0",
        "store *i8 %4, **i8 %3",
        "i32 %5 = load *i32 %0",
        "f32 %6 = load *f32 %1",
        "*i8 %7 = load **i8 %3",
        "call foo(i32 %5, f32 %6, *i8 %7)",
        "ret i32 0"
    }));
}

void test_ir_gen_implicit_return_void() {
    // No return statement, a return instruction should automatically be inserted
    const char* input = "void foo() {}\n";

    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "ret void"
    }));
}

void test_ir_gen_conditional_expr_void() {
    const char *input =
        "void foo();\n"
        "void bar();\n"
        "int main(int argc) {\n"
        "    argc ? foo() : bar();\n"
        "    return 0;\n"
        "}\n";

    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*i32 %0 = alloca i32",
        "store i32 argc, *i32 %0",
        "i32 %1 = load *i32 %0",
        "bool %2 = ne i32 %1, i32 0",
        "br bool %2, l0",
        "call bar()",
        "l0: nop",
        "call foo()",
        "ret i32 0"
    }));
}

void test_ir_gen_conditional_expr_returning_int() {
    const char *input =
        "int main(int argc) {\n"
        "    int a = 1;"
        "    short b = 1;"
        "    return argc ? a : b;\n"
        "}\n";

    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*i32 %0 = alloca i32",
        "*i32 %1 = alloca i32",
        "*i16 %2 = alloca i16",
        "store i32 argc, *i32 %0",
        "store i32 1, *i32 %1",
        "store i16 1, *i16 %2",
        "i32 %4 = load *i32 %0",
        "bool %5 = ne i32 %4, i32 0",
        "br bool %5, l0",
        "i16 %6 = load *i16 %2",
        "i32 %9 = ext i16 %6",
        "i32 %8 = i32 %9",
        "br l1",
        "l0: nop",
        "i32 %7 = load *i32 %1",
        "i32 %8 = i32 %7",
        "l1: nop",
        "ret i32 %8"
    }));
}

void test_ir_while_loop() {
    const char* input =
        "int main() {\n"
        "    int x = 0;\n"
        "    while (x < 10) {\n"
        "        x = x + 1;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";

    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*i32 %0 = alloca i32",
        "store i32 0, *i32 %0",
        "l0: nop",
        "i32 %1 = load *i32 %0",
        "bool %2 = lt i32 %1, i32 10",
        "bool %3 = eq bool %2, bool 0",
        "br bool %3, l1",
        "i32 %4 = load *i32 %0",
        "i32 %5 = add i32 %4, i32 1",
        "i32 %6 = i32 %5",
        "store i32 %6, *i32 %0",
        "br l0",
        "l1: nop",
        "ret i32 0"
    }));
}

void ir_gen_for_loop_empty() {
    const char* input =
        "int main() {\n"
        "    for (;;);\n"
        "    return 0;\n"
        "}\n";

    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    // You would expect to see the loop end label and a return 0 instruction here, but the ir-generator has
    // detected that it was un-reachable and removed it.
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "l0: nop",
        "br l0",
    }));
}

void ir_gen_declare_struct_type_global_scope() {
    const char* input = "struct Foo { int a; };\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);
}

void ir_gen_declare_struct_default_initializer() {
    const char* input = "int main() {"
                        "    struct Foo { int a; } foo;"
                        "}";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*struct.Foo_0 %0 = alloca struct.Foo_0",
        "ret i32 0"
    }));
}

void ir_gen_struct_set_field() {
    const char* input =
        "int main() {\n"
        "    struct Foo { int a; } foo;\n"
        "    foo.a = 4;\n"
        "    return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*struct.Foo_0 %0 = alloca struct.Foo_0",
        "*i32 %1 = get_struct_member_ptr *struct.Foo_0 %0, i32 0",
        "i32 %2 = i32 4", // TODO: this shouldn't be a variable, just a constant in the store instruction
        "store i32 %2, *i32 %1",
        "ret i32 0"
    }));
}

void ir_gen_struct_ptr_set_field() {
    const char* input =
        "struct Foo { int a; };"
        "int main(struct Foo *foo) {\n"
        "    foo->a = 1;\n"
        "    return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "**struct.Foo_0 %0 = alloca *struct.Foo_0",
        "store *struct.Foo_0 foo, **struct.Foo_0 %0",
        "*struct.Foo_0 %1 = load **struct.Foo_0 %0",
        "*i32 %2 = get_struct_member_ptr *struct.Foo_0 %1, i32 0",
        "i32 %3 = i32 1",
        "store i32 %3, *i32 %2",
        "ret i32 0"
    }));
}

void ir_gen_struct_read_field() {
    const char* input =
        "int main() {\n"
        "    struct Foo { int a; } foo;\n"
        "    int a = foo.a;"
        "    return 0;"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*struct.Foo_0 %0 = alloca struct.Foo_0",
        "*i32 %1 = alloca i32",
        "*i32 %2 = get_struct_member_ptr *struct.Foo_0 %0, i32 0",
        "i32 %3 = load *i32 %2",
        "store i32 %3, *i32 %1",
        "ret i32 0"
    }));
}

void ir_gen_struct_ptr_read_field() {
    const char* input =
        "struct Foo { int a; };"
        "int main(struct Foo *foo) {\n"
        "    int a = foo->a;\n"
        "    return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "**struct.Foo_0 %0 = alloca *struct.Foo_0",
        "*i32 %1 = alloca i32",
        "store *struct.Foo_0 foo, **struct.Foo_0 %0",
        "*struct.Foo_0 %2 = load **struct.Foo_0 %0",
        "*i32 %3 = get_struct_member_ptr *struct.Foo_0 %2, i32 0",
        "i32 %4 = load *i32 %3",
        "store i32 %4, *i32 %1",
        "ret i32 0"
    }));
}
 
int ir_gen_tests_init_suite() {
    CU_pSuite suite = CU_add_suite("IR Generation Tests", NULL, NULL);
    if (suite == NULL) {
        return CU_get_error();
    }

    CU_add_test(suite, "basic", test_ir_gen_basic);
    CU_add_test(suite, "add simple", test_ir_gen_add_simple);
    CU_add_test(suite, "add i32 + f32", test_ir_gen_add_i32_f32);
    CU_add_test(suite, "add constants", test_ir_gen_add_constants);
    CU_add_test(suite, "sub constants", test_ir_gen_sub_constants);
    CU_add_test(suite, "multiply constants", test_ir_gen_multiply_constants);
    CU_add_test(suite, "divide constants", test_ir_gen_divide_constants);
    CU_add_test(suite, "divide by zero (float constants)", test_ir_gen_divide_by_zero_float_constants);
    CU_add_test(suite, "divide by zero (integer constants)", test_ir_gen_divide_by_zero_integer_constants);
    CU_add_test(suite, "mod constants", test_ir_gen_mod_constants);
    CU_add_test(suite, "left shift constants", test_ir_gen_left_shift_constants);
    CU_add_test(suite, "right shift constants", test_ir_gen_right_shift_constants);
    CU_add_test(suite, "logic and constants 1", test_ir_gen_logic_and_constants_1);
    CU_add_test(suite, "logic and constants 2", test_ir_gen_logic_and_constants_2);
    CU_add_test(suite, "logic and constants 3", test_ir_gen_logic_and_constants_3);
    CU_add_test(suite, "logic or constants 1", test_ir_gen_logic_or_constants_1);
    CU_add_test(suite, "logic or constants 2", test_ir_gen_logic_or_constants_2);
    CU_add_test(suite, "logic or constants 3", test_ir_gen_logic_or_constants_3);
    CU_add_test(suite, "ternary expression constants 1", test_ir_gen_ternary_expression_constants_1);
    CU_add_test(suite, "ternary expression constants 2", test_ir_gen_ternary_expression_constants_2);
    CU_add_test(suite, "address of variable", test_ir_gen_addr_of_variable);
    CU_add_test(suite, "indirect load", test_ir_gen_indirect_load);
    CU_add_test(suite, "indirect store", test_ir_gen_indirect_store);
    CU_add_test(suite, "array load constant index", test_ir_gen_array_load_constant_index);
    CU_add_test(suite, "array store constant index", test_ir_gen_array_store_constant_index);
    CU_add_test(suite, "array load variable index", test_ir_gen_array_load_variable_index);
    CU_add_test(suite, "array load ptr", test_ir_gen_array_index_on_ptr);
    CU_add_test(suite, "if-else statement", test_ir_gen_if_else_statement);
    CU_add_test(suite, "call expr (returns void)", test_ir_gen_call_expr_returns_void);
    CU_add_test(suite, "function arg promotion", test_ir_gen_function_arg_promotion);
    CU_add_test(suite, "implicit return (void)", test_ir_gen_implicit_return_void);
    CU_add_test(suite, "varargs call", test_ir_gen_varargs_call);
    CU_add_test(suite, "conditional expr (void)", test_ir_gen_conditional_expr_void);
    CU_add_test(suite, "conditional expr", test_ir_gen_conditional_expr_returning_int);
    CU_add_test(suite, "while loop", test_ir_while_loop);
    CU_add_test(suite, "for loop (empty)", ir_gen_for_loop_empty);
    CU_add_test(suite, "declare struct type (global scope)", ir_gen_declare_struct_type_global_scope);
    CU_add_test(suite, "declare struct default initializer (local scope)", ir_gen_declare_struct_default_initializer);
    CU_add_test(suite, "struct set field", ir_gen_struct_set_field);
    CU_add_test(suite, "struct pointer set field", ir_gen_struct_ptr_set_field);
    CU_add_test(suite, "struct read field", ir_gen_struct_read_field);
    CU_add_test(suite, "struct pointer read field", ir_gen_struct_ptr_read_field);
    return CUE_SUCCESS;
}
