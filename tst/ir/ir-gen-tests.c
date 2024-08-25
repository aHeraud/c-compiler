#include <CUnit/Basic.h>

#include "ir/ir-gen.h"
#include "ir/fmt.h"

#include "../tests.h"
#include "../test-common.h"
#include "ir/arch.h"

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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);

    // TODO: warning, undefined result
    // For now we just make sure this doesn't crash
}

void test_ir_gen_mod_constants() {
    const char* input = "int main() {\n"
                        "    return 5 % 3;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "ret i32 3"
    }));
}

void test_ir_gen_prefix_increment_integer() {
    const char *input = "int main() {\n"
                        "    int a = 1;\n"
                        "    int b = ++a;\n"
                        "    return 0;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*i32 %0 = alloca i32",       // %0 = ptr to a
        "*i32 %1 = alloca i32",       // %1 = ptr to b
        "store i32 1, *i32 %0",       // a = 1
        "i32 %2 = load *i32 %0",      // %2 = a
        "i32 %3 = add i32 %2, i32 1", // %3 = a + 1
        "store i32 %3, *i32 %0",      // a = a + 1
        "store i32 %3, *i32 %1",      // b = a + 1
        "ret i32 0"
    }));
}

void test_ir_gen_postfix_increment_integer() {
    const char *input = "int main() {\n"
                        "    int a = 1;\n"
                        "    int b = a++;\n"
                        "    return 0;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*i32 %0 = alloca i32",       // %0 = ptr to a
        "*i32 %1 = alloca i32",       // %1 = ptr to b
        "store i32 1, *i32 %0",       // a = 1
        "i32 %2 = load *i32 %0",      // %2 = a
        "i32 %3 = add i32 %2, i32 1", // %3 = a + 1
        "store i32 %3, *i32 %0",      // a = a + 1
        "store i32 %2, *i32 %1",      // b = %2 (a before increment)
        "ret i32 0"
    }));
}

void test_ir_gen_prefix_decrement_integer() {
    const char *input = "int main() {\n"
                        "    int a = 1;\n"
                        "    int b = --a;\n"
                        "    return 0;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*i32 %0 = alloca i32",       // %0 = ptr to a
        "*i32 %1 = alloca i32",       // %1 = ptr to b
        "store i32 1, *i32 %0",       // a = 1
        "i32 %2 = load *i32 %0",      // %2 = a
        "i32 %3 = sub i32 %2, i32 1", // %3 = a - 1
        "store i32 %3, *i32 %0",      // a = a + 1
        "store i32 %3, *i32 %1",      // b = a + 1
        "ret i32 0"
    }));
}

void test_ir_gen_postfix_decrement_integer() {
    const char *input = "int main() {\n"
                        "    int a = 1;\n"
                        "    int b = a--;\n"
                        "    return 0;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*i32 %0 = alloca i32",       // %0 = ptr to a
        "*i32 %1 = alloca i32",       // %1 = ptr to b
        "store i32 1, *i32 %0",       // a = 1
        "i32 %2 = load *i32 %0",      // %2 = a
        "i32 %3 = sub i32 %2, i32 1", // %3 = a - 1
        "store i32 %3, *i32 %0",      // a = a + 1
        "store i32 %2, *i32 %1",      // b = %2 (a before increment)
        "ret i32 0"
    }));
}

void test_ir_gen_postfix_increment_float() {
    const char *input = "int main() {\n"
                        "    float a = 1.0f;\n"
                        "    float b = a++;\n"
                        "    return 0;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*f32 %0 = alloca f32",              // %0 = ptr to a
        "*f32 %1 = alloca f32",              // %1 = ptr to b
        "store f32 1.000000, *f32 %0",       // a = 1
        "f32 %2 = load *f32 %0",             // %2 = a
        "f32 %3 = add f32 %2, f32 1.000000", // %3 = a + 1
        "store f32 %3, *f32 %0",             // a = a + 1
        "store f32 %2, *f32 %1",             // b = %2 (a before increment)
        "ret i32 0"
    }));
}

void test_ir_gen_postfix_decrement_float() {
    const char *input = "int main() {\n"
                        "    float a = 1.0f;\n"
                        "    float b = a--;\n"
                        "    return 0;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*f32 %0 = alloca f32",              // %0 = ptr to a
        "*f32 %1 = alloca f32",              // %1 = ptr to b
        "store f32 1.000000, *f32 %0",       // a = 1
        "f32 %2 = load *f32 %0",             // %2 = a
        "f32 %3 = sub f32 %2, f32 1.000000", // %3 = a + 1
        "store f32 %3, *f32 %0",             // a = a + 1
        "store f32 %2, *f32 %1",             // b = %2 (a before increment)
        "ret i32 0"
    }));
}

void test_ir_gen_postfix_increment_pointer() {
    const char *input = "int main() {\n"
                        "    int x = 0;\n"
                        "    int *a = &x;\n"
                        "    int *b = a++;\n"
                        "    return 0;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*i32 %0 = alloca i32",
        "**i32 %1 = alloca *i32",                         // %1 = ptr to a
        "**i32 %2 = alloca *i32",                         // %2 = ptr to b
        "store i32 0, *i32 %0",
        "store *i32 %0, **i32 %1",
        "*i32 %3 = load **i32 %1",                        // %3 = a
        "*i32 %4 = get_array_element_ptr *i32 %3, i32 1", // %4 = a + 1
        "store *i32 %4, **i32 %1",                        // a = a + 1
        "store *i32 %3, **i32 %2",                        // b = a before incrementing
        "ret i32 0"
    }));
}

void test_ir_gen_postfix_decrement_pointer() {
    const char *input = "int main() {\n"
                        "    int x = 0;\n"
                        "    int *a = &x;\n"
                        "    int *b = a--;\n"
                        "    return 0;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*i32 %0 = alloca i32",
        "**i32 %1 = alloca *i32",                         // %1 = ptr to a
        "**i32 %2 = alloca *i32",                         // %2 = ptr to b
        "store i32 0, *i32 %0",
        "store *i32 %0, **i32 %1",
        "*i32 %3 = load **i32 %1",                        // %3 = a
        "*i32 %4 = get_array_element_ptr *i32 %3, i32 -1", // %4 = a + 1
        "store *i32 %4, **i32 %1",                        // a = a + 1
        "store *i32 %3, **i32 %2",                        // b = a before decrementing
        "ret i32 0"
    }));
}

void test_ir_gen_addr_of_variable() {
    const char* input = "int main() {\n"
                        "    int a = 1;\n"
                        "    int *b = &a;\n"
                        "    return 0;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "**i32 %0 = alloca *i32",
        "store *i32 a, **i32 %0",
        "*i32 %1 = load **i32 %0",
        "store i32 1, *i32 %1",
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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*[i32;2] %0 = alloca [i32;2]",
        "*i32 %1 = get_array_element_ptr *[i32;2] %0, i32 1",
        "store i32 10, *i32 %1",
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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*i32 %0 = alloca i32",
        "*i32 %1 = alloca i32",
        "store i32 a, *i32 %0",
        "i32 %2 = load *i32 %0",
        "bool %3 = eq i32 %2, i32 0",
        "br bool %3, l0",
        "store i32 1, *i32 %1",
        "br l1",
        "l0: nop",
        "store i32 2, *i32 %1",
        "l1: nop",
        "i32 %4 = load *i32 %1",
        "ret i32 %4"
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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
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

void test_ir_gen_function_vararg_promotion() {
    const char* input =
        "int printf(const char *fmt, ...);\n"
        "int main() {\n"
        "    float a = 1.0f;\n"
        "    char b = 75;\n"
        "    short c = 1024;\n"
        "    printf(\"%f, %d, %d\\n\", a, b, c);\n"
        "}\n";

    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*f32 %0 = alloca f32",
        "*i8 %1 = alloca i8",
        "*i16 %3 = alloca i16",
        "store f32 1.000000, *f32 %0",
        "store i8 75, *i8 %1",
        "store i16 1024, *i16 %3",
        "*i8 %5 = bitcast *[i8;12] @0",
        "f32 %6 = load *f32 %0",
        "f64 %7 = ext f32 %6",
        "i8 %8 = load *i8 %1",
        "i32 %9 = ext i8 %8",
        "i16 %10 = load *i16 %3",
        "i32 %11 = ext i16 %10",
        "i32 %12 = call printf(*i8 %5, f64 %7, i32 %9, i32 %11)",
        "ret i32 0"
    }));
}

void test_ir_gen_varargs_call() {
    // Test calling a function with a variable number of arguments
    // Important! The varargs arguments are _NOT_ converted to the type of the last named argument, they are just
    // passed as is after integer/float promotion.
    const char* input =
        "void foo(int a, ...);\n"
        "int main() {\n"
        "    int a = 1;\n"
        "    double b = 1.0;\n"
        "    char* c = \"hello\";\n"
        "    foo(a, b, c);\n"
        "    return 0;\n"
        "}\n";

    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*i32 %0 = alloca i32",
        "*f64 %1 = alloca f64",
        "**i8 %2 = alloca *i8",
        "store i32 1, *i32 %0",
        "store f64 1.000000, *f64 %1",
        "*i8 %3 = bitcast *[i8;6] @0",
        "store *i8 %3, **i8 %2",
        "i32 %4 = load *i32 %0",
        "f64 %5 = load *f64 %1",
        "*i8 %6 = load **i8 %2",
        "call foo(i32 %4, f64 %5, *i8 %6)",
        "ret i32 0"
    }));
}

void test_ir_gen_implicit_return_void() {
    // No return statement, a return instruction should automatically be inserted
    const char* input = "void foo() {}\n";

    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
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
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*i32 %0 = alloca i32",
        "store i32 0, *i32 %0",
        "l0: nop",
        "i32 %1 = load *i32 %0",
        "bool %2 = lt i32 %1, i32 10",
        "bool %3 = eq bool %2, bool 0",
        "br bool %3, l2",
        "i32 %4 = load *i32 %0",
        "i32 %5 = add i32 %4, i32 1",
        "store i32 %5, *i32 %0",
        "l1: nop",
        "br l0",
        "l2: nop",
        "ret i32 0"
    }));
}

void test_ir_do_while_loop() {
    const char* input =
        "int main() {\n"
        "    int x = 0;\n"
        "    do {\n"
        "        x = x + 1;\n"
        "    } while (x < 10);\n"
        "    return 0;\n"
        "}\n";

    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*i32 %0 = alloca i32",          // x
        "store i32 0, *i32 %0",          // x = 0
        "l0: nop",                       // do {
        "i32 %1 = load *i32 %0",         // load x
        "i32 %2 = add i32 %1, i32 1",    // temp = x + 1
        "store i32 %2, *i32 %0",         // store temp, x
        "l1: nop",
        "i32 %3 = load *i32 %0",         // load x
        "bool %4 = lt i32 %3, i32 10",   // %5 = x < 10
        "bool %5 = eq bool %4, bool 0",  // invert the condition
        "br bool %5, l2",                // if x < 10 == false, exit loop
        "br l0",                         // go to the start of the loop
        "l2: nop",
        "ret i32 0"
    }));
}

void test_ir_gen_for_loop_empty() {
    const char* input =
        "int main() {\n"
        "    for (;;);\n"
        "    return 0;\n"
        "}\n";

    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    assert(result.errors.size == 0);

    // You would expect to see the loop end label and a return 0 instruction here, but the ir-generator has
    // detected that it was un-reachable and removed it.
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "l0: nop",
        "l1: nop",
        "br l0",
    }));
}

void test_ir_gen_declare_struct_type_global_scope() {
    const char* input = "struct Foo { int a; };\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    assert(result.errors.size == 0);
}

void test_ir_gen_declare_struct_default_initializer() {
    const char* input = "int main() {"
                        "    struct Foo { int a; } foo;"
                        "}";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*struct.Foo_0 %0 = alloca struct.Foo_0",
        "ret i32 0"
    }));
}

void test_ir_gen_struct_set_field() {
    const char* input =
        "int main() {\n"
        "    struct Foo { int a; } foo;\n"
        "    foo.a = 4;\n"
        "    return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*struct.Foo_0 %0 = alloca struct.Foo_0",
        "*i32 %1 = get_struct_member_ptr *struct.Foo_0 %0, i32 0",
        "store i32 4, *i32 %1",
        "ret i32 0"
    }));
}

void test_ir_gen_struct_ptr_set_field() {
    const char* input =
        "struct Foo { int a; };"
        "int main(struct Foo *foo) {\n"
        "    foo->a = 1;\n"
        "    return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "**struct.Foo_0 %0 = alloca *struct.Foo_0",
        "store *struct.Foo_0 foo, **struct.Foo_0 %0",
        "*struct.Foo_0 %1 = load **struct.Foo_0 %0",
        "*i32 %2 = get_struct_member_ptr *struct.Foo_0 %1, i32 0",
        "store i32 1, *i32 %2",
        "ret i32 0"
    }));
}

void test_ir_gen_struct_read_field() {
    const char* input =
        "int main() {\n"
        "    struct Foo { int a; } foo;\n"
        "    int a = foo.a;"
        "    return 0;"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
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

void test_ir_gen_struct_ptr_read_field() {
    const char* input =
        "struct Foo { int a; };"
        "int main(struct Foo *foo) {\n"
        "    int a = foo->a;\n"
        "    return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
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

void test_ir_gen_struct_definition_scoping() {
    const char *input =
        "struct Foo { int a; };\n"
        "struct Foo foo;\n"
        "int main() {\n"
        "    struct Foo { double b; };\n" // hides the Foo tag declared in the global scope
        "    foo.a = 1;\n"                // the type of foo is Foo { int a; } so this still works
        "    return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*i32 %0 = get_struct_member_ptr *struct.Foo_0 @1, i32 0",
        "store i32 1, *i32 %0",
        "ret i32 0"
    }));
}

void test_ir_gen_anonymous_struct() {
    const char* input =
        "int main() {\n"
        "    struct { int a; } foo;\n"
        "    foo.a = 0;\n"
        "    return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*struct.__anon_tag_0_0 %0 = alloca struct.__anon_tag_0_0",
        "*i32 %1 = get_struct_member_ptr *struct.__anon_tag_0_0 %0, i32 0",
        "store i32 0, *i32 %1",
        "ret i32 0"
    }));
}

void test_ir_gen_sizeof_type_primitive() {
    // sizeof(type) is a compile time constant, so it can be a global initializer
    const char *input = "int size = sizeof(int);\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    CU_ASSERT_EQUAL_FATAL(result.module->globals.size, 1);
    const ir_global_t *size = result.module->globals.buffer[0];
    CU_ASSERT_TRUE_FATAL(size->initialized && size->value.kind == IR_CONST_INT)
    CU_ASSERT_EQUAL_FATAL(size->value.i, 4) // int = i32 on x86_64
}

void test_ir_gen_sizeof_type_struct() {
    // sizeof(type) is a compile time constant, so it can be a global initializer
    const char *input =
        "struct Foo { char a; int b; };\n"
        "int size = sizeof(struct Foo);\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    CU_ASSERT_EQUAL_FATAL(result.module->globals.size, 1);
    const ir_global_t *size = result.module->globals.buffer[0];
    CU_ASSERT_TRUE_FATAL(size->initialized && size->value.kind == IR_CONST_INT)
    // expected size is 8: 1 for the char, 3 for padding to align the int, and 4 for the int
    CU_ASSERT_EQUAL_FATAL(size->value.i, 8)
}

void test_ir_gen_sizeof_unary_expression() {
    const char *input =
        "float val = 0;\n"
        "int size = sizeof(val)\n;";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    CU_ASSERT_EQUAL_FATAL(result.module->globals.size, 2);
    const ir_global_t *size = result.module->globals.buffer[1];
    CU_ASSERT_TRUE_FATAL(size->initialized && size->value.kind == IR_CONST_INT)
    // float on x86_64 = f32 == 4 bytes
    CU_ASSERT_EQUAL_FATAL(size->value.i, 4)
}

void test_ir_gen_unary_local_not_constexpr() {
    const char *input =
        "int main() {\n"
        "    int a = !4;\n"
        "    int b = !0;\n"
        "    return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*i32 %0 = alloca i32",
        "*i32 %1 = alloca i32",
        "store i32 0, *i32 %0",
        "store i32 1, *i32 %1",
        "ret i32 0"
    }));
}

void test_ir_gen_unary_local_not() {
    const char *input =
        "int main(int a) {\n"
        "    int b = !a;\n"
        "    return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*i32 %0 = alloca i32",
        "*i32 %1 = alloca i32",
        "store i32 a, *i32 %0",
        "i32 %2 = load *i32 %0",
        "bool %3 = eq i32 %2, i32 0",
        "i32 %4 = ext bool %3",
        "store i32 %4, *i32 %1",
        "ret i32 0"
    }));
}

void test_ir_gen_label_and_goto() {
    const char *input =
        "int main() {\n"
        "    int a = 0;\n"
        "    lbl: a = 1;\n"
        "    goto lbl;\n"
        "    return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*i32 %0 = alloca i32",
        "store i32 0, *i32 %0",
        "l0: nop",
        "store i32 1, *i32 %0",
        "br l0"
    }));
}

void test_ir_forward_goto() {
    const char *input =
        "int main() {\n"
        "    goto end;\n"
        "    int a = 1;\n"
        "    return a;\n"
        "    end: return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*i32 %0 = alloca i32",
        "br l0",
        "l0: nop",
        "ret i32 0"
    }));
}

void test_ir_while_break() {
    const char *input =
        "int main() {\n"
        "    while (1) {\n"
        "        break;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    // This looks a bit funky, but I think its due to eliminating unreachable nodes from the cfg then translating
    // back to linear form
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "l0: nop",
        "bool %0 = eq i32 1, i32 0",
        "br bool %0, l2",
        "br l2",
        "l2: nop",
        "ret i32 0"
    }));
}

void test_ir_do_while_break() {
    const char *input =
        "int main() {\n"
        "    do {\n"
        "        break;\n"
        "    } while (1);\n"
        "    return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    // Note that the entire condition check is removed due to being un-reachable in the CFG
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "l0: nop",
        "br l2",
        "l2: nop",
        "ret i32 0"
    }));
}

void ir_test_for_break() {
    const char *input =
        "int main() {\n"
        "    for (;1;) {\n"
        "        break;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "l0: nop",
        "bool %0 = eq i32 1, i32 0",
        "br bool %0, l2",
        "br l2",
        "l2: nop",
        "ret i32 0"
    }));
}

void ir_test_while_continue() {
    const char *input =
        "int main() {\n"
        "    while (1) {\n"
        "        continue;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "l0: nop",
        "bool %0 = eq i32 1, i32 0",
        "br bool %0, l2",
        "br l1",
        "l1: nop",
        "br l0",
        "l2: nop",
        "ret i32 0"
    }));
}

void ir_test_do_while_continue() {
    const char *input =
        "int main() {\n"
        "    do {\n"
        "        continue;\n"
        "    } while (1);\n"
        "    return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "l0: nop",                    // do {
        "br l1",                      //    continue;
        "l1: nop",                    // }
        "bool %0 = eq i32 1, i32 0",  //
        "br bool %0, l2",             // if condition is false, exit loop
        "br l0",                      // go to start of loop
        "l2: nop",
        "ret i32 0"
    }));
}

void ir_test_for_continue() {
    const char *input =
        "int main() {\n"
        "    for (;1;) {\n"
        "        continue;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "l0: nop",
        "bool %0 = eq i32 1, i32 0",
        "br bool %0, l2",
        "br l1",
        "l1: nop",
        "br l0",
        "l2: nop",
        "ret i32 0"
    }));
}

void ir_test_compound_assign_add() {
    const char *input =
        "int main() {\n"
            "int a = 0;\n"
            "a += 1;\n"
            "return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        // int a = 0;
        "*i32 %0 = alloca i32",
        "store i32 0, *i32 %0",
        // a += 1;
        "i32 %1 = load *i32 %0",
        "i32 %2 = add i32 %1, i32 1",
        "store i32 %2, *i32 %0",
        // return 0;
        "ret i32 0"
    }));
}

void ir_test_compound_assign_sub() {
    const char *input =
        "int main() {\n"
            "int a = 0;\n"
            "a -= 1;\n"
            "return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        // int a = 0;
        "*i32 %0 = alloca i32",
        "store i32 0, *i32 %0",
        // a -= 1;
        "i32 %1 = load *i32 %0",
        "i32 %2 = sub i32 %1, i32 1",
        "store i32 %2, *i32 %0",
        // return 0;
        "ret i32 0"
    }));
}

void ir_test_compound_assign_mul() {
    const char *input =
        "int main() {\n"
            "int a = 1;\n"
            "a *= 2;\n"
            "return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        // int a = 1;
        "*i32 %0 = alloca i32",
        "store i32 1, *i32 %0",
        // a *= 2;
        "i32 %1 = load *i32 %0",
        "i32 %2 = mul i32 %1, i32 2",
        "store i32 %2, *i32 %0",
        // return 0;
        "ret i32 0"
    }));
}

void ir_test_compound_assign_div() {
    const char *input =
        "int main() {\n"
            "int a = 1;\n"
            "a /= 2;\n"
            "return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        // int a = 1;
        "*i32 %0 = alloca i32",
        "store i32 1, *i32 %0",
        // a /= 2;
        "i32 %1 = load *i32 %0",
        "i32 %2 = div i32 %1, i32 2",
        "store i32 %2, *i32 %0",
        // return 0;
        "ret i32 0"
    }));
}

void ir_test_compound_assign_mod() {
    const char *input =
        "int main() {\n"
            "int a = 1;\n"
            "a %= 2;\n"
            "return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        // int a = 1;
        "*i32 %0 = alloca i32",
        "store i32 1, *i32 %0",
        // a %= 2;
        "i32 %1 = load *i32 %0",
        "i32 %2 = mod i32 %1, i32 2",
        "store i32 %2, *i32 %0",
        // return 0;
        "ret i32 0"
    }));
}

void ir_test_compound_assign_and() {
    const char *input =
        "int main() {\n"
            "int a = 1;\n"
            "a &= 2;\n"
            "return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        // int a = 1;
        "*i32 %0 = alloca i32",
        "store i32 1, *i32 %0",
        // a &= 2;
        "i32 %1 = load *i32 %0",
        "i32 %2 = and i32 %1, i32 2",
        "store i32 %2, *i32 %0",
        // return 0;
        "ret i32 0"
    }));
}

void ir_test_compound_assign_or() {
    const char *input =
        "int main() {\n"
            "int a = 1;\n"
            "a |= 2;\n"
            "return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        // int a = 1;
        "*i32 %0 = alloca i32",
        "store i32 1, *i32 %0",
        // a |= 2;
        "i32 %1 = load *i32 %0",
        "i32 %2 = or i32 %1, i32 2",
        "store i32 %2, *i32 %0",
        // return 0;
        "ret i32 0"
    }));
}

void ir_test_compound_assign_xor() {
    const char *input =
        "int main() {\n"
            "int a = 1;\n"
            "a ^= 2;\n"
            "return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        // int a = 1;
        "*i32 %0 = alloca i32",
        "store i32 1, *i32 %0",
        // a |= 2;
        "i32 %1 = load *i32 %0",
        "i32 %2 = xor i32 %1, i32 2",
        "store i32 %2, *i32 %0",
        // return 0;
        "ret i32 0"
    }));
}

void ir_test_compound_assign_shl() {
    const char *input =
        "int main() {\n"
            "int a = 1;\n"
            "a <<= 2;\n"
            "return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        // int a = 1;
        "*i32 %0 = alloca i32",
        "store i32 1, *i32 %0",
        // a <<= 2;
        "i32 %1 = load *i32 %0",
        "i32 %2 = shl i32 %1, i32 2",
        "store i32 %2, *i32 %0",
        // return 0;
        "ret i32 0"
    }));
}

void ir_test_compound_assign_shr() {
    const char *input =
        "int main() {\n"
            "int a = 1;\n"
            "a >>= 2;\n"
            "return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        // int a = 1;
        "*i32 %0 = alloca i32",
        "store i32 1, *i32 %0",
        // a >>= 2;
        "i32 %1 = load *i32 %0",
        "i32 %2 = shr i32 %1, i32 2",
        "store i32 %2, *i32 %0",
        // return 0;
        "ret i32 0"
    }));
}

void ir_test_cast_expression() {
    const char *input =
        "int main() {\n"
            "int a = 2;\n"
            "double d = (float) a;\n"
            "return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
            // int a
            "*i32 %0 = alloca i32",
            // double d
            "*f64 %1 = alloca f64",
            // a = 2
            "store i32 2, *i32 %0",
            // d = (float) a
            "i32 %2 = load *i32 %0",
            "f32 %3 = itof i32 %2",
            "f64 %4 = ext f32 %3",
            "store f64 %4, *f64 %1",
            // return 0;
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
    CU_add_test(suite, "prefix-increment integer", test_ir_gen_prefix_increment_integer);
    CU_add_test(suite, "postfix-increment integer", test_ir_gen_postfix_increment_integer);
    CU_add_test(suite, "prefix-decrement integer", test_ir_gen_prefix_decrement_integer);
    CU_add_test(suite, "postfix-decrement integer", test_ir_gen_postfix_decrement_integer);
    CU_add_test(suite, "postfix-increment float", test_ir_gen_postfix_increment_float);
    CU_add_test(suite, "postfix-decrement float", test_ir_gen_postfix_decrement_float);
    CU_add_test(suite, "postfix-increment pointer", test_ir_gen_postfix_increment_pointer);
    CU_add_test(suite, "postfix-decrement pointer", test_ir_gen_postfix_decrement_pointer);
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
    CU_add_test(suite, "test_ir_gen_function_vararg_promotion", test_ir_gen_function_vararg_promotion);
    CU_add_test(suite, "implicit return (void)", test_ir_gen_implicit_return_void);
    CU_add_test(suite, "varargs call", test_ir_gen_varargs_call);
    CU_add_test(suite, "conditional expr (void)", test_ir_gen_conditional_expr_void);
    CU_add_test(suite, "conditional expr", test_ir_gen_conditional_expr_returning_int);
    CU_add_test(suite, "while loop", test_ir_while_loop);
    CU_add_test(suite, "do-while loop", test_ir_do_while_loop);
    CU_add_test(suite, "for loop (empty)", test_ir_gen_for_loop_empty);
    CU_add_test(suite, "declare struct type (global scope)", test_ir_gen_declare_struct_type_global_scope);
    CU_add_test(suite, "declare struct default initializer (local scope)", test_ir_gen_declare_struct_default_initializer);
    CU_add_test(suite, "struct set field", test_ir_gen_struct_set_field);
    CU_add_test(suite, "struct pointer set field", test_ir_gen_struct_ptr_set_field);
    CU_add_test(suite, "struct read field", test_ir_gen_struct_read_field);
    CU_add_test(suite, "struct pointer read field", test_ir_gen_struct_ptr_read_field);
    CU_add_test(suite, "struct definition scoping", test_ir_gen_struct_definition_scoping);
    CU_add_test(suite, "anonymous struct", test_ir_gen_anonymous_struct);
    CU_add_test(suite, "sizeof type primitive", test_ir_gen_sizeof_type_primitive);
    CU_add_test(suite, "sizeof type struct", test_ir_gen_sizeof_type_struct);
    CU_add_test(suite, "sizeof unary expression", test_ir_gen_sizeof_unary_expression);
    CU_add_test(suite, "unary logical not (constant)", test_ir_gen_unary_local_not_constexpr);
    CU_add_test(suite, "unary logical not", test_ir_gen_unary_local_not);
    CU_add_test(suite, "goto", test_ir_gen_label_and_goto);
    CU_add_test(suite, "goto (forward)", test_ir_forward_goto);
    CU_add_test(suite, "break (while)", test_ir_while_break);
    CU_add_test(suite, "break (do-while)", test_ir_do_while_break);
    CU_add_test(suite, "break (for)", ir_test_for_break);
    CU_add_test(suite, "continue (while)", ir_test_while_continue);
    CU_add_test(suite, "continue (do-while)", ir_test_do_while_continue);
    CU_add_test(suite, "continue (for)", ir_test_for_continue);
    CU_add_test(suite, "compound assignment (add)", ir_test_compound_assign_add);
    CU_add_test(suite, "compound assignment (sub)", ir_test_compound_assign_sub);
    CU_add_test(suite, "compound assignment (mul)", ir_test_compound_assign_mul);
    CU_add_test(suite, "compound assignment (div)", ir_test_compound_assign_div);
    CU_add_test(suite, "compound assignment (mod)", ir_test_compound_assign_mod);
    CU_add_test(suite, "compound assignment (shl)", ir_test_compound_assign_shl);
    CU_add_test(suite, "compound assignment (shr)", ir_test_compound_assign_shr);
    CU_add_test(suite, "compound assignment (and)", ir_test_compound_assign_and);
    CU_add_test(suite, "compound assignment (or)", ir_test_compound_assign_or);
    CU_add_test(suite, "compound assignment (xor)", ir_test_compound_assign_xor);
    CU_add_test(suite, "cast expression", ir_test_cast_expression);
    return CUE_SUCCESS;
}
