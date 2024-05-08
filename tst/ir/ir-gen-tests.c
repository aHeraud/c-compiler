#include <CUnit/Basic.h>

#include "ir/ir-gen.h"

#include "../tests.h"
#include "../test-common.h"

#define PARSE(input)                                                             \
    lexer_global_context_t lexer_context = create_lexer_context();               \
    lexer_t lexer = linit("path/to/file", input, strlen(input), &lexer_context); \
    parser_t parser = pinit(lexer);                                              \
    translation_unit_t program;                                                  \
    CU_ASSERT_TRUE_FATAL(parse(&parser, &program))

#define ASSERT_IR_INSTRUCTIONS_EQ(function, body) \
do { \
    bool size_equals = function->num_instructions == sizeof(body)/sizeof(body[0]); \
    bool body_equals = true;                                                    \
    if (size_equals) {                                                          \
        for (int i = 0; i < function->num_instructions; i += 1) {               \
            const char *instruction =                                           \
                ir_fmt_instr(alloca(512), 512, &function->instructions[i]);     \
            if (strcmp(body[i], instruction) != 0) {                            \
                body_equals = false;                                            \
                fprintf(stderr, "Expected (at index %u): %s, Actual: %s\n",     \
                    i, body[i], instruction);                                   \
            }                                                                   \
        }                                                                       \
    }                                                                           \
    if (!body_equals || !size_equals) {                                         \
        fprintf(stderr, "Expected and actual function body not equal:\n");      \
        fprintf(stderr, "\nExpected:\n");                                       \
        for (int i = 0; i < (sizeof(body)/sizeof(body[0])); i += 1) {           \
            fprintf(stderr, "%s\n", body[i]);                                   \
        }                                                                       \
        fprintf(stderr, "\nActual:\n");                                         \
        for (int i = 0; i < function->num_instructions; i += 1) {               \
            char instr[512];                                                    \
            fprintf(stderr, "%s\n",                                             \
                ir_fmt_instr(instr, 512, &function->instructions[i]));          \
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
        "f32 t1 = f32 1.000000",
        "f32 t2 = f32 2.000000",
        "f32 t0 = add f32 t1, f32 t2",
        "ret f32 t0"
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
        "f32 t1 = itof i32 1",
        "f32 t2 = f32 2.000000",
        "f32 t0 = add f32 t1, f32 t2",
        "i32 t3 = ftoi f32 t0",
        "ret i32 t3"
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
        "*i32 t0 = alloca i32",
        "store i32 a, *i32 t0",
        "*i32 t1 = alloca i32",
        "i32 t2 = load *i32 t0",
        "bool t3 = eq i32 t2, i32 0",
        "br bool t3, l0",
        "i32 t4 = i32 1",
        "store i32 t4, *i32 t1",
        "br l1",
        "l0: nop",
        "i32 t5 = i32 2",
        "store i32 t5, *i32 t1",
        "l1: nop",
        "i32 t6 = load *i32 t1",
        "ret i32 t6"
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

    return CUE_SUCCESS;
}
