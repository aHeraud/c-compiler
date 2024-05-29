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
                        "    return 1.0f + 2.0f;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "f32 %1 = f32 1.000000",
        "f32 %2 = f32 2.000000",
        "f32 %0 = add f32 %1, f32 %2",
        "ret f32 %0"
    }));
}

void test_ir_gen_add_i32_f32() {
    const char* input = "int main() {\n"
                        "    return 1 + 2.0f;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "f32 %1 = itof i32 1",
        "f32 %2 = f32 2.000000",
        "f32 %0 = add f32 %1, f32 %2",
        "i32 %3 = ftoi f32 %0",
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
        "store i32 a, *i32 %0",
        "*i32 %1 = alloca i32",
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
        "i32 %0 = i32 1",
        "call foo(i32 %0)",
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
        "f32 %1 = f32 1.000000",
        "store f32 %1, *f32 %0",
        "f32 %2 = load *f32 %0",
        "f64 %3 = ext f32 %2",
        "call foo(f64 %3)",
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
        "i32 %1 = i32 1",
        "store i32 %1, *i32 %0",
        "*f32 %2 = alloca f32",
        "f32 %3 = trunc f64 1.000000",
        "store f32 %3, *f32 %2",
        "**i8 %4 = alloca *i8",
        "*i8 %5 = bitcast *[i8;6]  @0",
        "store *i8 %5, **i8 %4",
        "i32 %6 = load *i32 %0",
        "i32 %7 = i32 %6",
        "f32 %8 = load *f32 %2",
        "*i8 %9 = load **i8 %4",
        "call foo(i32 %7, f32 %8, *i8 %9)",
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
        "bool %2 = eq i32 %1, i32 0",
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
        "store i32 argc, *i32 %0",
        "*i32 %1 = alloca i32",
        "i32 %2 = i32 1",
        "store i32 %2, *i32 %1",
        "*i16 %3 = alloca i16",
        "i16 %4 = trunc i32 1",
        "store i16 %4, *i16 %3",
        "i32 %5 = load *i32 %0",
        "bool %6 = eq i32 %5, i32 0",
        "br bool %6, l0",
        "i16 %7 = load *i16 %3",
        "i32 %10 = ext i16 %7",
        "i32 %9 = i32 %10",
        "br l1",
        "l0: nop",
        "i32 %8 = load *i32 %1",
        "i32 %9 = i32 %8",
        "l1: nop",
        "ret i32 %9"
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
        "i32 %1 = i32 0",
        "store i32 %1, *i32 %0",
        "l0: nop",
        "i32 %2 = load *i32 %0",
        "i32 %3 = i32 %2",
        "i32 %4 = i32 10",
        "bool %5 = lt i32 %3, i32 %4",
        "bool %6 = eq bool %5, bool 0",
        "br bool %6, l1",
        "i32 %7 = load *i32 %0",
        "i32 %9 = i32 %7",
        "i32 %10 = i32 1",
        "i32 %8 = add i32 %9, i32 %10",
        "i32 %11 = i32 %8",
        "store i32 %11, *i32 %0",
        "br l0",
        "l1: nop",
        "ret i32 0",
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
    CU_add_test(suite, "if-else statement", test_ir_gen_if_else_statement);
    CU_add_test(suite, "call expr (returns void)", test_ir_gen_call_expr_returns_void);
    CU_add_test(suite, "function arg promotion", test_ir_gen_function_arg_promotion);
    CU_add_test(suite, "implicit return (void)", test_ir_gen_implicit_return_void);
    CU_add_test(suite, "varargs call", test_ir_gen_varargs_call);
    CU_add_test(suite, "conditional expr (void)", test_ir_gen_conditional_expr_void);
    CU_add_test(suite, "conditional expr", test_ir_gen_conditional_expr_returning_int);
    CU_add_test(suite, "while loop", test_ir_while_loop);
    return CUE_SUCCESS;
}
