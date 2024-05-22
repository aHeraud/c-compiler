#include <malloc.h>
#include "CUnit/Basic.h"
#include "tests.h"
#include "lexer.h"

static lexer_global_context_t create_context() {
    return (lexer_global_context_t) {
            .user_include_paths = NULL,
            .system_include_paths = NULL,
            .macro_definitions = hash_table_create_string_keys(16),
    };
}

void test_simple_program() {
    lexer_global_context_t context = create_context();

    char* input = "/*multi line\ncomment*/\nint main() {\n    return 0; // comment\n}";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    token_t token;
    //CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_COMMENT);
    //CU_ASSERT_STRING_EQUAL_FATAL(token.value, "/*multi line\ncomment*/");
    //CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_NEWLINE);
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_INT);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, "int");
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_IDENTIFIER);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, "main");
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_LPAREN);
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_RPAREN);
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_LBRACE);
    //CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_NEWLINE);
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_RETURN);
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_INTEGER_CONSTANT);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, "0");
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_SEMICOLON);
    //CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_COMMENT);
    //CU_ASSERT_STRING_EQUAL_FATAL(token.value, "// comment");
    //CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_NEWLINE);
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_RBRACE);
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_EOF);
}

void test_lex_float_constant() {
    lexer_global_context_t context = create_context();
    char* input = "42.0";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    token_t token = lscan(&lexer);
    printf("%s\n", token.value);
    CU_ASSERT_EQUAL_FATAL(token.kind, TK_FLOATING_CONSTANT);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, "42.0");
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_EOF);
}

void test_lex_float_constant_with_exponent() {
    lexer_global_context_t context = create_context();
    char* input = "15.0e-3";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    token_t token = lscan(&lexer);
    CU_ASSERT_EQUAL_FATAL(token.kind, TK_FLOATING_CONSTANT);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, "15.0e-3");
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_EOF);
}

void test_lex_float_constant_with_exponent_and_suffix() {
    lexer_global_context_t context = create_context();
    char* input = "15.0e-3f";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    token_t token = lscan(&lexer);
    CU_ASSERT_EQUAL_FATAL(token.kind, TK_FLOATING_CONSTANT);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, "15.0e-3f");
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_EOF);
}

void test_lex_float_constant_with_no_fractional_part_and_exponent() {
    lexer_global_context_t context = create_context();
    char* input = "1e-3";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    token_t token = lscan(&lexer);
    CU_ASSERT_EQUAL_FATAL(token.kind, TK_FLOATING_CONSTANT);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, "1e-3");
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_EOF);
}

void test_lex_float_constant_with_no_fractional_part() {
    lexer_global_context_t context = create_context();
    char* input = "1.";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    token_t token = lscan(&lexer);
    CU_ASSERT_EQUAL_FATAL(token.kind, TK_FLOATING_CONSTANT);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, "1.");
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_EOF);
}

void test_lex_float_constant_with_no_whole_part() {
    lexer_global_context_t context = create_context();
    char* input = ".5";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    token_t token = lscan(&lexer);
    CU_ASSERT_EQUAL_FATAL(token.kind, TK_FLOATING_CONSTANT);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, ".5");
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_EOF);
}

void test_lex_decimal_constant() {
    lexer_global_context_t context = create_context();
    char* input = "123456789";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    token_t token = lscan(&lexer);
    CU_ASSERT_EQUAL_FATAL(token.kind, TK_INTEGER_CONSTANT);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, "123456789");
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_EOF);
}

void test_lex_decimal_constant_with_suffix() {
    lexer_global_context_t context = create_context();
    char* input = "42ull";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    token_t token = lscan(&lexer);
    CU_ASSERT_EQUAL_FATAL(token.kind, TK_INTEGER_CONSTANT);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, "42ull");
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_EOF);
}

void test_lex_hexadecimal_constant() {
    lexer_global_context_t context = create_context();
    char* input = "0xFF05";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    token_t token = lscan(&lexer);
    CU_ASSERT_EQUAL_FATAL(token.kind, TK_INTEGER_CONSTANT);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, "0xFF05");
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_EOF);
}

void test_lex_floating_hexadecimal_constant() {
    lexer_global_context_t context = create_context();
    char* input = "0x1.5p-3";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    token_t token = lscan(&lexer);
    CU_ASSERT_EQUAL_FATAL(token.kind, TK_FLOATING_CONSTANT);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, "0x1.5p-3");
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_EOF);
}

void test_lex_octal_constant() {
    lexer_global_context_t context = create_context();
    char* input = "01234567";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    token_t token = lscan(&lexer);
    CU_ASSERT_EQUAL_FATAL(token.kind, TK_INTEGER_CONSTANT);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, "01234567");
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_EOF);
}

int lexer_tests_init_suite() {
    CU_pSuite pSuite = CU_add_suite("lexer", NULL, NULL);
    if (NULL == CU_add_test(pSuite, "lex simple test program", test_simple_program) ||
        NULL == CU_add_test(pSuite, "lex float constant", test_lex_float_constant) ||
        NULL == CU_add_test(pSuite, "lex float constant with exponent", test_lex_float_constant_with_exponent) ||
        NULL == CU_add_test(pSuite, "lex float constant with exponent and suffix", test_lex_float_constant_with_exponent_and_suffix) ||
        NULL == CU_add_test(pSuite, "lex float constant with no fractional part", test_lex_float_constant_with_no_fractional_part) ||
        NULL == CU_add_test(pSuite, "lex float constant with no whole part", test_lex_float_constant_with_no_whole_part) ||
        NULL == CU_add_test(pSuite, "lex float constant with no fractional part and exponent", test_lex_float_constant_with_no_fractional_part_and_exponent) ||
        NULL == CU_add_test(pSuite, "lex decimal constant", test_lex_decimal_constant) ||
        NULL == CU_add_test(pSuite, "lex decimal constant with suffix", test_lex_decimal_constant_with_suffix) ||
        NULL == CU_add_test(pSuite, "lex hexadecimal constant", test_lex_hexadecimal_constant) ||
        NULL == CU_add_test(pSuite, "lex floating hexadecimal constant", test_lex_floating_hexadecimal_constant) ||
        NULL == CU_add_test(pSuite, "lex octal constant", test_lex_octal_constant)
    ) {
        CU_cleanup_registry();
        return CU_get_error();
    }
    return 0;
}
