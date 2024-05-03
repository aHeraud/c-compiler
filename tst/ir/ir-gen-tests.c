#include <CUnit/Basic.h>

#include "ir/ir-gen.h"

#include "../tests.h"
#include "../test-common.h"

#define PARSE(input) \
    lexer_global_context_t lexer_context = create_lexer_context(); \
    lexer_t lexer = linit("path/to/file", input, strlen(input), &lexer_context); \
    parser_t parser = pinit(lexer); \
    translation_unit_t program; \
    CU_ASSERT_TRUE_FATAL(parse(&parser, &program))

#define ASSERT_IR_INSTRUCTIONS_EQ(function, body) \
do { \
    CU_ASSERT_EQUAL_FATAL(function->num_instructions, sizeof(body)/sizeof(body[0])) \
    for (int i = 0; i < function->num_instructions; i += 1) { \
        const char *instruction = format_ir_instruction(alloca(512), 512, &function->instructions[i]); \
        if (strcmp(body[i], instruction) != 0) { \
            fprintf(stderr, "Expected (at index %u): %s, Actual: %s\n", i, body[i], instruction); \
            CU_FAIL("Instructions do not match") \
        } \
    } \
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
                        "    return 1 + 2;\n"
                        "}\n";
    PARSE(input)
    ir_gen_result_t result = generate_ir(&program);
    assert(result.errors.size == 0);

    ir_function_definition_t *function = result.module->functions.buffer[0];
    ASSERT_IR_INSTRUCTIONS_EQ(function, ((const char*[]) {
        "i32 t1 = i32 1",
        "i32 t2 = i32 2",
        "i32 t0 = add i32 t1, i32 t2",
        "ret i32 t0"
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
        "ret f32 t0"
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

    return CUE_SUCCESS;
}
