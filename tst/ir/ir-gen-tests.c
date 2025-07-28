#include <CUnit/Basic.h>

#include "errors.h"
#include "ir/arch.h"
#include "ir/codegen/codegen.h"
#include "ir/codegen/internal.h"
#include "ir/fmt.h"

#include "../tests.h"
#include "../test-common.h"

/// IR generation tests
/// These are a bit fragile, since they rely on the output of the IR generation matching exactly.
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
            const char instruction[1024];                                       \
            ir_fmt_instr(instruction, 1024, &function->body.buffer[i]);         \
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
            char instr[1024];                                                   \
            ir_fmt_instr(instr, 1024, &function->body.buffer[i]);               \
            fprintf(stderr, "%s\n", instr);                                     \
        }                                                                       \
        CU_FAIL()                                                               \
    }                                                                           \
} while (0)

void test_ir_gen_basic(void) {
    const char* input = "int main(void) {\n"
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

void test_ir_gen_add_simple(void) {
    const char* input = "float main(void) {\n"
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

void test_ir_gen_add_i32_f32(void) {
    const char* input = "int main(void) {\n"
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

void test_ir_gen_add_constants(void) {
    const char* input = "float main(void) {\n"
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

void test_ir_gen_sub_constants(void) {
    const char* input = "int main(void) {\n"
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

void test_ir_gen_multiply_constants(void) {
    const char* input = "int main(void) {\n"
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

void test_ir_gen_divide_constants(void) {
    const char* input = "int main(void) {\n"
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

void test_ir_gen_divide_by_zero_float_constants(void) {
    const char* input = "float main(void) {\n"
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

void test_ir_gen_divide_by_zero_integer_constants(void) {
    const char* input = "int main(void) {\n"
                        "    return 1 / 0;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);

    // TODO: warning, undefined result
    // For now we just make sure this doesn't crash
}

void test_ir_gen_mod_constants(void) {
    const char* input = "int main(void) {\n"
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

void test_ir_gen_left_shift_constants(void) {
    const char* input = "int main(void) {\n"
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

void test_ir_gen_right_shift_constants(void) {
    const char* input = "int main(void) {\n"
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

void test_ir_gen_logic_and_constants_1(void) {
    const char* input = "int main(void) {\n"
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

void test_ir_gen_logic_and_constants_2(void) {
    const char* input = "int main(void) {\n"
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

void test_ir_gen_logic_and_constants_3(void) {
    const char* input = "int main(void) {\n"
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

void test_ir_gen_logic_or_constants_1(void) {
    const char* input = "int main(void) {\n"
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

void test_ir_gen_logic_or_constants_2(void) {
    const char* input = "int main(void) {\n"
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

void test_ir_gen_logic_or_constants_3(void) {
    const char* input = "int main(void) {\n"
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

void test_ir_gen_ternary_expression_constants_1(void) {
    const char* input = "int main(void) {\n"
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

void test_ir_gen_ternary_expression_constants_2(void) {
    const char* input = "int main(void) {\n"
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

void test_ir_gen_prefix_increment_integer(void) {
    const char *input = "int main(void) {\n"
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

void test_ir_gen_postfix_increment_integer(void) {
    const char *input = "int main(void) {\n"
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

void test_ir_gen_prefix_decrement_integer(void) {
    const char *input = "int main(void) {\n"
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

void test_ir_gen_postfix_decrement_integer(void) {
    const char *input = "int main(void) {\n"
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

void test_ir_gen_postfix_increment_float(void) {
    const char *input = "int main(void) {\n"
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

void test_ir_gen_postfix_decrement_float(void) {
    const char *input = "int main(void) {\n"
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

void test_ir_gen_postfix_increment_pointer(void) {
    const char *input = "int main(void) {\n"
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

void test_ir_gen_postfix_decrement_pointer(void) {
    const char *input = "int main(void) {\n"
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

void test_ir_gen_addr_of_variable(void) {
    const char* input = "int main(void) {\n"
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

void test_ir_gen_indirect_load(void) {
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

void test_ir_gen_indirect_store(void) {
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

void test_ir_gen_ptr_increment_deref_and_write(void) {
    const char *input =
        "void test(int *ptr) {\n"
        "    *ptr++ = 4;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    const ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "**i32 %0 = alloca *i32",
        "store *i32 ptr, **i32 %0",
        "*i32 %1 = load **i32 %0",
        "*i32 %2 = get_array_element_ptr *i32 %1, i32 1",
        "store *i32 %2, **i32 %0",
        "store i32 4, *i32 %1",
        "ret void"
    }));
}

void test_ir_gen_ptr_to_ptr_copy_and_increment(void) {
    const char *input =
            "void copy(int *from, int *to) {\n"
            "    *to++ = *from++;\n"
            "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    const ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "**i32 %0 = alloca *i32",
        "**i32 %1 = alloca *i32",
        "store *i32 from, **i32 %0",
        "store *i32 to, **i32 %1",
        "*i32 %2 = load **i32 %1",
        "*i32 %3 = get_array_element_ptr *i32 %2, i32 1",
        "store *i32 %3, **i32 %1",
        "*i32 %4 = load **i32 %0",
        "*i32 %5 = get_array_element_ptr *i32 %4, i32 1",
        "store *i32 %5, **i32 %0",
        "i32 %6 = load *i32 %4",
        "store i32 %6, *i32 %2",
        "ret void"
    }));
}

void test_ir_gen_array_load_constant_index(void) {
    // we use 1 as the index, because a[0] would be optimized away during ir generation
    const char* input = "int foo(void) {\n"
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

void test_ir_gen_array_store_constant_index(void) {
    // we use 1 as the index, because a[0] would be optimized away during ir generation
    const char* input = "int foo(void) {\n"
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

void test_ir_gen_array_load_variable_index(void) {
    const char* input = "int foo(void) {\n"
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

void test_ir_gen_array_index_on_ptr(void) {
    const char* input = "int foo(int *a) {\n"
                        "    return a[0];\n"
                        "}";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0);

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

void test_ir_gen_array_unspecified_size_with_initializer(void) {
    const char *input =
        "int main(void) {\n"
        "    int a[] = {1, 2, 3};\n"
        "    return a[2];\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_EQUAL_FATAL(result.errors.size, 0);
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*[i32;3] %0 = alloca [i32;3]",
        "*i32 %1 = bitcast *[i32;0] %0",
        "*i32 %2 = get_array_element_ptr *i32 %1, i64 0",
        "store i32 1, *i32 %2",
        "*i32 %3 = get_array_element_ptr *i32 %1, i64 1",
        "store i32 2, *i32 %3",
        "*i32 %4 = get_array_element_ptr *i32 %1, i64 2",
        "store i32 3, *i32 %4",
        "*i32 %5 = get_array_element_ptr *[i32;3] %0, i32 2",
        "i32 %6 = load *i32 %5",
        "ret i32 %6"
    }));
}

void test_ir_gen_array_initializer_with_designators(void) {
    const char *input =
        "int main(void) {\n"
        "    int a[] = { 1, [4] = 4, [2] = 2, 3 };\n"
        "    return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_EQUAL_FATAL(result.errors.size, 0);
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*[i32;5] %0 = alloca [i32;5]",
        "*i32 %1 = bitcast *[i32;0] %0",
        "*i32 %2 = get_array_element_ptr *i32 %1, i64 0",
        "store i32 1, *i32 %2",
        "*i32 %3 = get_array_element_ptr *i32 %1, i64 4",
        "store i32 4, *i32 %3",
        "*i32 %4 = get_array_element_ptr *i32 %1, i64 2",
        "store i32 2, *i32 %4",
        "*i32 %5 = get_array_element_ptr *i32 %1, i64 3",
        "store i32 3, *i32 %5",
        "ret i32 0"
    }));
}

void test_ir_gen_struct_initializer(void) {
    const char *input =
            "int main(void) {\n"
            "    struct Foo { int a; int b; int c; };\n"
            "    struct Foo foo = { 1, 2, 3 };\n"
            "    return foo.b;\n"
            "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_EQUAL_FATAL(result.errors.size, 0);
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
            "*struct.Foo_0 %0 = alloca struct.Foo_0",
            "*i32 %1 = get_struct_member_ptr *struct.Foo_0 %0, i32 0",
            "store i32 1, *i32 %1",
            "*i32 %2 = get_struct_member_ptr *struct.Foo_0 %0, i32 1",
            "store i32 2, *i32 %2",
            "*i32 %3 = get_struct_member_ptr *struct.Foo_0 %0, i32 2",
            "store i32 3, *i32 %3",
            "*i32 %4 = get_struct_member_ptr *struct.Foo_0 %0, i32 1",
            "i32 %5 = load *i32 %4",
            "ret i32 %5"
    }));
}

void test_ir_gen_struct_initializer_with_designators(void) {
    const char *input =
            "int main(void) {\n"
            "    struct Foo { int a; int b; int c; };\n"
            "    struct Foo foo = { .b = 2, 3, .a = 1 };\n" // mix between designated and non-designated initializer elements
            "    return foo.b;\n"
            "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_EQUAL_FATAL(result.errors.size, 0);
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
            "*struct.Foo_0 %0 = alloca struct.Foo_0",
            "*i32 %1 = get_struct_member_ptr *struct.Foo_0 %0, i32 1",
            "store i32 2, *i32 %1",
            "*i32 %2 = get_struct_member_ptr *struct.Foo_0 %0, i32 2",
            "store i32 3, *i32 %2",
            "*i32 %3 = get_struct_member_ptr *struct.Foo_0 %0, i32 0",
            "store i32 1, *i32 %3",
            "*i32 %4 = get_struct_member_ptr *struct.Foo_0 %0, i32 1",
            "i32 %5 = load *i32 %4",
            "ret i32 %5"
    }));
}

void test_ir_gen_struct_initializer_with_designators_nested(void) {
    const char *input =
            "int main(void) {\n"
            "    struct Inner { int a; int b; };\n"
            "    struct Outer { struct Inner inner; };\n"
            "    struct Outer s = { .inner = { 1, 2 } };\n"
            "    return s.inner.b;"
            "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_EQUAL_FATAL(result.errors.size, 0);
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*struct.Outer_1 %0 = alloca struct.Outer_1",
        "*struct.Inner_0 %1 = get_struct_member_ptr *struct.Outer_1 %0, i32 0",
        "*i32 %2 = get_struct_member_ptr *struct.Inner_0 %1, i32 0",
        "store i32 1, *i32 %2",
        "*i32 %3 = get_struct_member_ptr *struct.Inner_0 %1, i32 1",
        "store i32 2, *i32 %3",
        "*struct.Inner_0 %4 = get_struct_member_ptr *struct.Outer_1 %0, i32 0",
        "*i32 %5 = get_struct_member_ptr *struct.Inner_0 %4, i32 1",
        "i32 %6 = load *i32 %5",
        "ret i32 %6"
    }));
}

void test_ir_gen_struct_initializer_with_designators_deeply_nested(void) {
    const char *input =
            "int main(void) {\n"
            "    struct Inner { int a; };\n"
            "    struct Middle { struct Inner inner; };\n"
            "    struct Outer { struct Middle middle; };\n"
            "    struct Outer s = { .middle.inner.a = 4 };\n"
            "    return s.middle.inner.a;"
            "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_EQUAL_FATAL(result.errors.size, 0);
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
            "*struct.Outer_2 %0 = alloca struct.Outer_2",
            "*struct.Middle_1 %1 = get_struct_member_ptr *struct.Outer_2 %0, i32 0",
            "*struct.Inner_0 %2 = get_struct_member_ptr *struct.Middle_1 %1, i32 0",
            "*i32 %3 = get_struct_member_ptr *struct.Inner_0 %2, i32 0",
            "store i32 4, *i32 %3",
            "*struct.Middle_1 %4 = get_struct_member_ptr *struct.Outer_2 %0, i32 0",
            "*struct.Inner_0 %5 = get_struct_member_ptr *struct.Middle_1 %4, i32 0",
            "*i32 %6 = get_struct_member_ptr *struct.Inner_0 %5, i32 0",
            "i32 %7 = load *i32 %6",
            "ret i32 %7"
    }));
}

void test_ir_gen_struct_assignment_memcpy(void) {
    const char *input =
            "int main(void) {\n"
            "    struct Foo { int a; };"
            "    struct Foo a, b;\n"
            "    a = b;\n"
            "    return 0;\n"
            "}";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_EQUAL_FATAL(result.errors.size, 0);
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
            "*struct.Foo_0 %0 = alloca struct.Foo_0",
            "*struct.Foo_0 %1 = alloca struct.Foo_0",
            "memcpy *struct.Foo_0 %0, *struct.Foo_0 %1, i64 4",
            "ret i32 0"
    }));
}

void test_ir_gen_struct_initializer_compound_literal(void) {
    const char *input =
            "int main(void) {\n"
            "    struct Foo { int a; int b; };\n"
            "    (struct Foo) { 1, 2, };"
            "    return 0;\n"
            "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_EQUAL_FATAL(result.errors.size, 0);
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
            "*struct.Foo_0 %0 = alloca struct.Foo_0",
            "*i32 %1 = get_struct_member_ptr *struct.Foo_0 %0, i32 0",
            "store i32 1, *i32 %1",
            "*i32 %2 = get_struct_member_ptr *struct.Foo_0 %0, i32 1",
            "store i32 2, *i32 %2",
            "ret i32 0"
    }));
}

void test_ir_gen_compound_literal_assign(void) {
    const char *input =
            "int main(void) {\n"
            "    struct Foo { int a; };\n"
            "    struct Foo foo;\n"
            "    foo = (struct Foo) { 1, };\n"
            "    return 0;\n"
            "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_EQUAL_FATAL(result.errors.size, 0);
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
            "*struct.Foo_0 %0 = alloca struct.Foo_0",
            "*struct.Foo_0 %1 = alloca struct.Foo_0",
            "*i32 %2 = get_struct_member_ptr *struct.Foo_0 %1, i32 0",
            "store i32 1, *i32 %2",
            "memcpy *struct.Foo_0 %0, *struct.Foo_0 %1, i64 4",
            "ret i32 0"
    }));
}

void test_ir_gen_if_else_statement(void) {
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

void test_ir_gen_call_expr_returns_void(void) {
    const char* input =
        "void foo(int a);\n"
        "int main(void) {\n"
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

void test_ir_gen_function_arg_promotion(void) {
    const char* input =
        "void foo(double a);\n"
        "int main(void) {\n"
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

void test_ir_gen_function_vararg_promotion(void) {
    const char* input =
        "int printf(const char *fmt, ...);\n"
        "int main(void) {\n"
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

void test_ir_gen_varargs_call(void) {
    // Test calling a function with a variable number of arguments
    // Important! The varargs arguments are _NOT_ converted to the type of the last named argument, they are just
    // passed as is after integer/float promotion.
    const char* input =
        "void foo(int a, ...);\n"
        "int main(void) {\n"
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

void test_ir_gen_implicit_return_void(void) {
    // No return statement, a return instruction should automatically be inserted
    const char* input = "void foo(void) {}\n";

    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "ret void"
    }));
}

void test_ir_gen_conditional_expr_void(void) {
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

void test_ir_gen_conditional_expr_returning_int(void) {
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

void test_ir_while_loop(void) {
    const char* input =
        "int main(void) {\n"
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

void test_ir_do_while_loop(void) {
    const char* input =
        "int main(void) {\n"
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

void test_ir_gen_for_loop_empty(void) {
    const char* input =
        "int main(void) {\n"
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

void test_ir_gen_declare_struct_type_global_scope(void) {
    const char* input = "struct Foo { int a; };\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    assert(result.errors.size == 0);
}

void test_ir_gen_declare_struct_default_initializer(void) {
    const char* input = "int main(void) {"
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

void test_ir_gen_struct_set_field(void) {
    const char* input =
        "int main(void) {\n"
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

void test_ir_gen_struct_ptr_set_field(void) {
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

void test_ir_gen_struct_read_field(void) {
    const char* input =
        "int main(void) {\n"
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

void test_ir_gen_struct_ptr_read_field(void) {
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

void test_ir_gen_struct_definition_scoping(void) {
    const char *input =
        "struct Foo { int a; };\n"
        "struct Foo foo;\n"
        "int main(void) {\n"
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

void test_ir_gen_anonymous_struct(void) {
    const char* input =
        "int main(void) {\n"
        "    struct { int a; } foo;\n"
        "    foo.a = 0;\n"
        "    return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*struct.__anon_struct__1_0 %0 = alloca struct.__anon_struct__1_0",
        "*i32 %1 = get_struct_member_ptr *struct.__anon_struct__1_0 %0, i32 0",
        "store i32 0, *i32 %1",
        "ret i32 0"
    }));
}

void test_ir_gen_nested_anonymous_struct(void) {
    const char* input =
            "int main(void) {\n"
            "    struct Outer { struct { int a; } inner; };\n"
            "    struct Outer val;\n"
            "    val.inner.a = 0;\n"
            "    return 0;\n"
            "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
            "*struct.Outer_0 %0 = alloca struct.Outer_0",
            "*struct.__anon_struct__1_1 %1 = get_struct_member_ptr *struct.Outer_0 %0, i32 0",
            "*i32 %2 = get_struct_member_ptr *struct.__anon_struct__1_1 %1, i32 0",
            "store i32 0, *i32 %2",
            "ret i32 0"
    }));
}

void test_ir_gen_sizeof_type_primitive(void) {
    // sizeof(type) is a compile time constant, so it can be a global initializer
    const char *input = "int size = sizeof(int);\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    CU_ASSERT_EQUAL_FATAL(result.module->globals.size, 1);
    const ir_global_t *size = result.module->globals.buffer[0];
    CU_ASSERT_TRUE_FATAL(size->initialized && size->value.kind == IR_CONST_INT)
    CU_ASSERT_EQUAL_FATAL(size->value.value.i, 4) // int = i32 on x86_64
}

void test_ir_gen_sizeof_type_struct(void) {
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
    CU_ASSERT_EQUAL_FATAL(size->value.value.i, 8)
}

void test_ir_gen_sizeof_unary_expression(void) {
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
    CU_ASSERT_EQUAL_FATAL(size->value.value.i, 4)
}

void test_ir_gen_unary_local_not_constexpr(void) {
    const char *input =
        "int main(void) {\n"
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

void test_ir_gen_unary_local_not(void) {
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

void test_ir_gen_unary_negative_const_int(void) {
    const char *input =
        "int main(void) {\n"
        "    return -1;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "ret i32 -1"
    }));
}

void test_ir_gen_unary_negative_const_float(void) {
    const char *input =
        "float main(void) {\n"
        "    return -1.0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "ret f32 -1.000000"
    }));
}

void test_ir_gen_unary_negative_int(void) {
    const char *input =
        "int main(int a) {\n"
        "    return -a;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*i32 %0 = alloca i32",
        "store i32 a, *i32 %0",
        "i32 %1 = load *i32 %0",
        "i32 %2 = sub i32 0, i32 %1",
        "ret i32 %2"
    }));
}

void test_ir_gen_unary_negative_float(void) {
    const char *input =
        "float main(float a) {\n"
        "    return -a;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*f32 %0 = alloca f32",
        "store f32 a, *f32 %0",
        "f32 %1 = load *f32 %0",
        "f32 %2 = sub f32 0.000000, f32 %1",
        "ret f32 %2"
    }));
}

void test_ir_gen_label_and_goto(void) {
    const char *input =
        "int main(void) {\n"
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

void test_ir_gen_forward_goto(void) {
    const char *input =
        "int main(void) {\n"
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

void test_ir_gen_while_break(void) {
    const char *input =
        "int main(void) {\n"
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

void test_ir_gen_do_while_break(void) {
    const char *input =
        "int main(void) {\n"
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

void test_ir_gen_for_break(void) {
    const char *input =
        "int main(void) {\n"
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

void test_ir_gen_while_continue(void) {
    const char *input =
        "int main(void) {\n"
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

void test_ir_gen_do_while_continue(void) {
    const char *input =
        "int main(void) {\n"
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

void test_ir_gen_for_continue(void) {
    const char *input =
        "int main(void) {\n"
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

void ir_test_compound_assign_add(void) {
    const char *input =
        "int main(void) {\n"
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

void ir_test_compound_assign_sub(void) {
    const char *input =
        "int main(void) {\n"
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

void ir_test_compound_assign_mul(void) {
    const char *input =
        "int main(void) {\n"
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

void ir_test_compound_assign_div(void) {
    const char *input =
        "int main(void) {\n"
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

void ir_test_compound_assign_mod(void) {
    const char *input =
        "int main(void) {\n"
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

void ir_test_compound_assign_and(void) {
    const char *input =
        "int main(void) {\n"
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

void ir_test_compound_assign_or(void) {
    const char *input =
        "int main(void) {\n"
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

void ir_test_compound_assign_xor(void) {
    const char *input =
        "int main(void) {\n"
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

void ir_test_compound_assign_shl(void) {
    const char *input =
        "int main(void) {\n"
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

void ir_test_compound_assign_shr(void) {
    const char *input =
        "int main(void) {\n"
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

void ir_test_cast_expression(void) {
    const char *input =
        "int main(void) {\n"
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

void test_ir_gen_empty_switch(void) {
    const char *input =
        "int main(void) {\n"
        "    switch (1);\n"
        "    return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    const ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
            "switch i32 1, l0, {  }",
            "l0: nop",
            "ret i32 0"
    }));
}

void test_ir_gen_switch(void) {
    const char *input =
        "int foo(int bar) {\n"
        "    switch(bar) {\n"
        "        case 1: /* fall-through */;\n"
        "        case 2:\n"
        "            break;\n"
        "        default:\n"
        "            return 1;\n"
        "    }\n"
        "    return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    const ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*i32 %0 = alloca i32",
        "store i32 bar, *i32 %0",
        "i32 %1 = load *i32 %0",
        "switch i32 %1, l3, { 1: l1, 2: l2 }",
        "l1: nop",
        "l2: nop",
        "br l0",
        /* I would have expected these labels/return statements to be in the opposite order, but this is equivalent */
        "l0: nop",
        "ret i32 0",
        "l3: nop",
        "ret i32 1"
    }));
}

void test_ir_gen_switch_default_fallthrough(void) {
    const char *input =
            "int foo(int bar) {\n"
            "    switch(bar) {\n"
            "        case 0: break;\n"
            "        default:\n"
            "            bar = 0;\n"
            "    }\n"
            "    return 0;\n"
            "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    const ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*i32 %0 = alloca i32",
        "store i32 bar, *i32 %0",
        "i32 %1 = load *i32 %0",
        "switch i32 %1, l2, { 0: l1 }",
        "l1: nop",
        "br l0",
        "l2: nop",
        "store i32 0, *i32 %0",
        "l0: nop",
        "ret i32 0"
    }));
}

void test_ir_gen_loop_inside_switch(void) {
    const char *input =
            "int foo(int bar) {\n"
            "    switch(bar) {\n"
            "        case 0:\n"
            "            while (bar) { continue; }\n"
            "            break;\n"
            "        case 1:\n"
            "            while (bar) { break; }\n"
            "            break;\n"
            "    }\n"
            "    return 0;\n"
            "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    const ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*i32 %0 = alloca i32",
        "store i32 bar, *i32 %0",
        "i32 %1 = load *i32 %0",
        "switch i32 %1, l0, { 0: l1, 1: l5 }",
        // case 1:
        "l5: nop",
        // while
        "l6: nop",
        // bar != 0
        "i32 %4 = load *i32 %0",
        "bool %5 = eq i32 %4, i32 0",
        "br bool %5, l8",
        "br l8",
        "l8: nop",
        "br l0",
        "l0: nop",
        "ret i32 0",
        "l1: nop",
        "l2: nop",
        // while
        "i32 %2 = load *i32 %0",
        "bool %3 = eq i32 %2, i32 0",
        "br bool %3, l4",
        "br l3",
        "l3: nop",
        "br l2",
        "l4: nop",
        "br l0"
    }));
}

void test_ir_gen_global_initializer_constant_propagation(void) {
    const char *input =
            "const int a = 14;\n"
            "const int b = a + 1;\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    const ir_global_t *b = result.module->globals.buffer[1];
    CU_ASSERT_TRUE_FATAL(b->initialized)
    CU_ASSERT_TRUE_FATAL(b->value.kind == IR_CONST_INT)
    CU_ASSERT_TRUE_FATAL(b->value.value.i == 15)
}

void test_ir_gen_constant_propagation(void) {
    const char *input =
            "int foo(void) {\n"
            "    const int a = 1;\n"
            "    const int b = 2;\n"
            "    const int c = a + b;\n"
            "    return a + b + c;\n"
            "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    const ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
            "*i32 %0 = alloca i32",
            "*i32 %1 = alloca i32",
            "*i32 %2 = alloca i32",
            "store i32 1, *i32 %0",
            "store i32 2, *i32 %1",
            "store i32 3, *i32 %2",
            "ret i32 6"
    }));
}

void test_ir_gen_enum_declare_assign_use(void) {
    const char *input =
            "int main(void) {\n"
            "    enum Foo { A } foo = A;\n"
            "    return foo;\n"
            "}";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    const ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
            "*i32 %1 = alloca i32",
            "store i32 0, *i32 %1",
            "i32 %2 = load *i32 %1",
            "ret i32 %2"
    }));
}

void test_ir_gen_enum_assign_to_int_var(void) {
    const char *input =
            "int main(void) {\n"
            "    enum Foo { A };\n"
            "    int foo = A;\n"
            "    return foo;\n"
            "}";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0)
    const ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
            "*i32 %1 = alloca i32",
            "store i32 0, *i32 %1",
            "i32 %2 = load *i32 %1",
            "ret i32 %2"
    }));
}

void test_ir_gen_global_array_initializer_list(void) {
    // Global array initializers should be constants, the resulting array should be stored in the resulting
    // ir module's globals table.
    const char *input = "int a[] = { 1, 2, 3 };";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0);
    CU_ASSERT_TRUE_FATAL(result.module->globals.size == 1);
    ir_global_t *a = result.module->globals.buffer[0];
    // The global is a pointer to [i32; 3]
    CU_ASSERT_TRUE_FATAL(a->type->kind == IR_TYPE_PTR &&
                         a->type->value.ptr.pointee->kind == IR_TYPE_ARRAY &&
                         a->type->value.ptr.pointee->value.array.length == 3 &&
                         a->type->value.ptr.pointee->value.array.element->kind == IR_TYPE_I32);
    CU_ASSERT_TRUE_FATAL(a->initialized);
    CU_ASSERT_TRUE_FATAL(a->value.kind == IR_CONST_ARRAY);
    CU_ASSERT_TRUE_FATAL(a->value.value.array.length == 3);
    CU_ASSERT_TRUE_FATAL(a->value.type->kind == IR_TYPE_ARRAY &&
                         a->value.type->value.array.length == 3 &&
                         a->value.type->value.array.element->kind == IR_TYPE_I32);
    CU_ASSERT_TRUE_FATAL(a->value.value.array.values[0].kind == IR_CONST_INT && a->value.value.array.values[0].value.i == 1);
    CU_ASSERT_TRUE_FATAL(a->value.value.array.values[1].kind == IR_CONST_INT && a->value.value.array.values[1].value.i == 2);
    CU_ASSERT_TRUE_FATAL(a->value.value.array.values[2].kind == IR_CONST_INT && a->value.value.array.values[2].value.i == 3);
}

void test_ir_global_array_initializer_list_with_excess_elements(void) {
    const char *input = "int a[2] = { 1, 2, 3 };";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0);
    CU_ASSERT_TRUE_FATAL(result.module->globals.size == 1);
    ir_global_t *a = result.module->globals.buffer[0];
    // The global is a pointer to [i32; 2]
    CU_ASSERT_TRUE_FATAL(a->type->kind == IR_TYPE_PTR &&
                         a->type->value.ptr.pointee->kind == IR_TYPE_ARRAY &&
                         a->type->value.ptr.pointee->value.array.length == 2 &&
                         a->type->value.ptr.pointee->value.array.element->kind == IR_TYPE_I32);
    CU_ASSERT_TRUE_FATAL(a->initialized);
    CU_ASSERT_TRUE_FATAL(a->value.kind == IR_CONST_ARRAY);
    CU_ASSERT_TRUE_FATAL(a->value.value.array.length == 2);
    CU_ASSERT_TRUE_FATAL(a->value.type->kind == IR_TYPE_ARRAY &&
                         a->value.type->value.array.length == 2 &&
                         a->value.type->value.array.element->kind == IR_TYPE_I32);
    CU_ASSERT_TRUE_FATAL(a->value.value.array.values[0].kind == IR_CONST_INT && a->value.value.array.values[0].value.i == 1);
    CU_ASSERT_TRUE_FATAL(a->value.value.array.values[1].kind == IR_CONST_INT && a->value.value.array.values[1].value.i == 2);
}

void test_ir_global_array_initializer_list_with_fewer_elements(void) {
    const char *input = "int a[3] = { 1, 2 };";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0);
    CU_ASSERT_TRUE_FATAL(result.module->globals.size == 1);
    ir_global_t *a = result.module->globals.buffer[0];
    // The global is a pointer to [i32; 3]
    CU_ASSERT_TRUE_FATAL(a->type->kind == IR_TYPE_PTR &&
                         a->type->value.ptr.pointee->kind == IR_TYPE_ARRAY &&
                         a->type->value.ptr.pointee->value.array.length == 3 &&
                         a->type->value.ptr.pointee->value.array.element->kind == IR_TYPE_I32);
    // Even though the array has length of 3, the constant initializer only has a length of 2
    CU_ASSERT_TRUE_FATAL(a->initialized);
    CU_ASSERT_TRUE_FATAL(a->value.kind == IR_CONST_ARRAY);
    CU_ASSERT_TRUE_FATAL(a->value.value.array.length == 3);
    CU_ASSERT_TRUE_FATAL(a->value.type->kind == IR_TYPE_ARRAY &&
                         a->value.type->value.array.length == 3 &&
                         a->value.type->value.array.element->kind == IR_TYPE_I32);
    CU_ASSERT_TRUE_FATAL(a->value.value.array.values[0].kind == IR_CONST_INT && a->value.value.array.values[0].value.i == 1);
    CU_ASSERT_TRUE_FATAL(a->value.value.array.values[1].kind == IR_CONST_INT && a->value.value.array.values[1].value.i == 2);
}

void test_ir_sizeof_global_array_size_inferred_from_initializer(void) {
    const char *input =
            "int a[] = { 0, 0 };\n"
            "int main() {\n"
            "    return sizeof(a);\n"
            "}";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0);
    CU_ASSERT_TRUE_FATAL(result.module->globals.size == 1);
    const ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
            "ret i32 8"
    }));
}

void test_ir_global_array_nested_designated_initializer_list(void) {
    const char *input = "int a[2][2] = { [1][1] = 1 };";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0);
    CU_ASSERT_TRUE_FATAL(result.module->globals.size == 1);
    ir_global_t *a = result.module->globals.buffer[0];
    // The global is a pointer to [i32; [i32; 2]]
    CU_ASSERT_TRUE_FATAL(a->type->kind == IR_TYPE_PTR &&
                         a->type->value.ptr.pointee->kind == IR_TYPE_ARRAY &&
                         a->type->value.ptr.pointee->value.array.length == 2 &&
                         a->type->value.ptr.pointee->value.array.element->kind == IR_TYPE_ARRAY);

    // a[0] = { 0, 0 }
    ir_const_t a_0 = a->value.value.array.values[0];
    CU_ASSERT_TRUE_FATAL(a_0.value.array.length == 2 &&
        a_0.value.array.values[0].value.i == 0 &&
        a_0.value.array.values[1].value.i == 0);

    // a[1] = { 0, 1 }
    ir_const_t a_1 = a->value.value.array.values[1];
    CU_ASSERT_TRUE_FATAL(a_1.value.array.length == 2 &&
        a_1.value.array.values[0].value.i == 0 &&
        a_1.value.array.values[1].value.i == 1);
}

void test_ir_global_struct_initializer_list(void) {
    const char *input = "struct Foo { int a; int b; } foo = { 1, 2 };";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0);
    CU_ASSERT_TRUE_FATAL(result.module->globals.size == 1);
    ir_global_t *foo = result.module->globals.buffer[0];
    CU_ASSERT_TRUE_FATAL(foo->type->kind == IR_TYPE_PTR &&
                         foo->type->value.ptr.pointee->kind == IR_TYPE_STRUCT_OR_UNION);
    CU_ASSERT_TRUE_FATAL(foo->value.kind == IR_CONST_STRUCT &&
                         foo->value.value._struct.length == 2);
    ir_const_t a = foo->value.value._struct.fields[0];
    CU_ASSERT_TRUE_FATAL(a.kind == IR_CONST_INT &&
                         a.value.i == 1);
    ir_const_t b = foo->value.value._struct.fields[1];
    CU_ASSERT_TRUE_FATAL(b.kind == IR_CONST_INT &&
                         b.value.i == 2);
}

void test_ir_global_array_of_structs_initializer_list(void) {
    const char *input =
        "struct Foo { int a; };\n"
        "struct Foo foo[1] = { { 1, }, };\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0);
    CU_ASSERT_TRUE_FATAL(result.module->globals.size == 1);
    ir_global_t *foo = result.module->globals.buffer[0];
    CU_ASSERT_TRUE_FATAL(foo->type->kind == IR_TYPE_PTR &&
                         foo->type->value.ptr.pointee->kind == IR_TYPE_ARRAY &&
                         foo->type->value.ptr.pointee->value.array.length == 1);
    ir_const_t element = foo->value.value.array.values[0];
    CU_ASSERT_TRUE_FATAL(element.kind == IR_CONST_STRUCT &&
                         element.value._struct.length == 1 &&
                         element.value._struct.fields[0].kind == IR_CONST_INT);
}

void test_ir_forward_struct_declaration_ptr(void) {
    // forward declaration of a struct, so a pointer to it can be created without having the full definition
    const char *input =
        "struct Foo;\n"
        "struct Foo *foo;\n"
        "struct Foo {\n"
        "    int a;\n"
        "};\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0);
    CU_ASSERT_TRUE_FATAL(result.module->globals.size == 1);
    CU_ASSERT_TRUE_FATAL(result.module->globals.buffer[0]->type->kind == IR_TYPE_PTR);
}

void test_ir_recursive_struct_field(void) {
    const char *input =
        "struct Foo {\n"
        "    struct Foo *foo;\n"
        "};\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0);
}

void test_ir_union_inside_struct_inside_struct(void) {
    const char *input =
        "typedef union {\n"
        "   int v32;\n"
        "   struct {\n"
        "       char a;\n"
        "       char b;\n"
        "       char c;\n"
        "       char d;\n"
        "   } v8;\n"
        "} u1;\n"
        "struct s1\n"
        "{\n"
        "   u1 a;\n"
        "};\n"
        "struct s1 s;\n"
        "int main() {\n"
        "    s.a.v32 = 0xFF00FF00;\n"
        "    return 0;\n"
        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0);
    CU_ASSERT_TRUE_FATAL(result.module->globals.size == 1);
    const ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "*union.__anon_struct__2_1 %0 = get_struct_member_ptr *struct.s1_0 @3, i32 0",
        "*i32 %1 = get_struct_member_ptr *union.__anon_struct__2_1 %0, i32 0",
        "store i32 4278255360, *i32 %1",
        "ret i32 0"
    }));
}

void test_ir_sizeof_typedef() {
    const char *input =
       "typedef long size_t;\n"
       "int main(void) {\n"
       "    return sizeof(size_t);\n"
       "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program, &IR_ARCH_X86_64);
    CU_ASSERT_TRUE_FATAL(result.errors.size == 0);
    CU_ASSERT_TRUE_FATAL(result.module->functions.size == 1);
    const ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "ret i32 8"
    }));
}

int ir_gen_tests_init_suite(void) {
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
    CU_add_test(suite, "ptr increment deref and write", test_ir_gen_ptr_increment_deref_and_write);
    CU_add_test(suite, "ptr to ptr copy and increment", test_ir_gen_ptr_to_ptr_copy_and_increment);
    CU_add_test(suite, "array load constant index", test_ir_gen_array_load_constant_index);
    CU_add_test(suite, "array store constant index", test_ir_gen_array_store_constant_index);
    CU_add_test(suite, "array load variable index", test_ir_gen_array_load_variable_index);
    CU_add_test(suite, "array load ptr", test_ir_gen_array_index_on_ptr);
    CU_add_test(suite, "array (unspecified size) with initializer", test_ir_gen_array_unspecified_size_with_initializer);
    CU_add_test(suite, "array initializer with designators", test_ir_gen_array_initializer_with_designators);
    CU_add_test(suite, "struct initializer", test_ir_gen_struct_initializer);
    CU_add_test(suite, "struct initializer with designators", test_ir_gen_struct_initializer_with_designators);
    CU_add_test(suite, "struct initializer with designators - nested inner struct", test_ir_gen_struct_initializer_with_designators_nested);
    CU_add_test(suite, "struct initializer with designators - deeply nested", test_ir_gen_struct_initializer_with_designators_deeply_nested);
    CU_add_test(suite, "struct assignment memcpy", test_ir_gen_struct_assignment_memcpy);
    CU_add_test(suite, "struct initializer - compound literal", test_ir_gen_struct_initializer_compound_literal);
    CU_add_test(suite, "compound literal assign", test_ir_gen_compound_literal_assign);
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
    CU_add_test(suite, "anonymous struct - nested", test_ir_gen_nested_anonymous_struct);
    CU_add_test(suite, "sizeof type primitive", test_ir_gen_sizeof_type_primitive);
    CU_add_test(suite, "sizeof type struct", test_ir_gen_sizeof_type_struct);
    CU_add_test(suite, "sizeof unary expression", test_ir_gen_sizeof_unary_expression);
    CU_add_test(suite, "unary logical not (constant)", test_ir_gen_unary_local_not_constexpr);
    CU_add_test(suite, "unary logical not", test_ir_gen_unary_local_not);
    CU_add_test(suite, "unary negative (constant int)", test_ir_gen_unary_negative_const_int);
    CU_add_test(suite, "unary negative (constant float)", test_ir_gen_unary_negative_const_float);
    CU_add_test(suite, "unary negative int", test_ir_gen_unary_negative_int);
    CU_add_test(suite, "unary negative float", test_ir_gen_unary_negative_float);
    CU_add_test(suite, "goto", test_ir_gen_label_and_goto);
    CU_add_test(suite, "goto (forward)", test_ir_gen_forward_goto);
    CU_add_test(suite, "break (while)", test_ir_gen_while_break);
    CU_add_test(suite, "break (do-while)", test_ir_gen_do_while_break);
    CU_add_test(suite, "break (for)", test_ir_gen_for_break);
    CU_add_test(suite, "continue (while)", test_ir_gen_while_continue);
    CU_add_test(suite, "continue (do-while)", test_ir_gen_do_while_continue);
    CU_add_test(suite, "continue (for)", test_ir_gen_for_continue);
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
    CU_add_test(suite, "switch statement (empty)", test_ir_gen_empty_switch);
    CU_add_test(suite, "switch statement", test_ir_gen_switch);
    CU_add_test(suite, "switch statement (default fallthrough)", test_ir_gen_switch_default_fallthrough);
    CU_add_test(suite, "loop inside switch statement", test_ir_gen_loop_inside_switch);
    CU_add_test(suite, "global initializer constant propagation", test_ir_gen_global_initializer_constant_propagation);
    CU_add_test(suite, "constant propagation", test_ir_gen_constant_propagation);
    CU_add_test(suite, "enum declare assign use", test_ir_gen_enum_declare_assign_use);
    CU_add_test(suite, "enum assign to int var", test_ir_gen_enum_assign_to_int_var);
    CU_add_test(suite, "global array initializer list", test_ir_gen_global_array_initializer_list);
    CU_add_test(suite, "global array initializer list - with excess elements", test_ir_global_array_initializer_list_with_excess_elements);
    CU_add_test(suite, "global array initializer list - with fewer element", test_ir_global_array_initializer_list_with_fewer_elements);
    CU_add_test(suite, "global array initializer list - size inferred from initializer - sizeof", test_ir_sizeof_global_array_size_inferred_from_initializer);
    CU_add_test(suite, "global array initializer list - nested designated initializer", test_ir_global_array_nested_designated_initializer_list);
    CU_add_test(suite, "global struct initializer list", test_ir_global_struct_initializer_list);
    CU_add_test(suite, "global array initializer list - array of structs", test_ir_global_array_of_structs_initializer_list);
    CU_add_test(suite, "struct forward declaration - ptr", test_ir_forward_struct_declaration_ptr);
    CU_add_test(suite, "struct recursive field", test_ir_recursive_struct_field);
    CU_add_test(suite, "union inside struct inside struct", test_ir_union_inside_struct_inside_struct);
    CU_add_test(suite, "sizeof typedef", test_ir_sizeof_typedef);
    return CUE_SUCCESS;
}
