#include <malloc.h>
#include "CUnit/Basic.h"

#include "tests.h"

#include "utils/vectors.h"
#include "parser/lexer.h"

void test_simple_program(void) {
    
    char* input = "/*multi line\ncomment*/\nint main() {\n    return 0; // comment\n}";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    token_t token;
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_INT);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, "int");
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_IDENTIFIER);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, "main");
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_LPAREN);
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_RPAREN);
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_LBRACE);
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_RETURN);
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_INTEGER_CONSTANT);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, "0");
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_SEMICOLON);
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_RBRACE);
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_EOF);
}

void test_lex_float_constant_0(void) {
        char* input = "0.0";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    token_t token = lscan(&lexer);
    printf("%s\n", token.value);
    CU_ASSERT_EQUAL_FATAL(token.kind, TK_FLOATING_CONSTANT);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, "0.0");
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_EOF);
}

void test_lex_float_constant(void) {
        char* input = "42.0";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    token_t token = lscan(&lexer);
    printf("%s\n", token.value);
    CU_ASSERT_EQUAL_FATAL(token.kind, TK_FLOATING_CONSTANT);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, "42.0");
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_EOF);
}

void test_lex_float_constant_with_exponent(void) {
        char* input = "15.0e-3";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    token_t token = lscan(&lexer);
    CU_ASSERT_EQUAL_FATAL(token.kind, TK_FLOATING_CONSTANT);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, "15.0e-3");
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_EOF);
}

void test_lex_float_constant_with_exponent_and_suffix(void) {
        char* input = "15.0e-3f";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    token_t token = lscan(&lexer);
    CU_ASSERT_EQUAL_FATAL(token.kind, TK_FLOATING_CONSTANT);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, "15.0e-3f");
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_EOF);
}

void test_lex_float_constant_with_no_fractional_part_and_exponent(void) {
        char* input = "1e-3";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    token_t token = lscan(&lexer);
    CU_ASSERT_EQUAL_FATAL(token.kind, TK_FLOATING_CONSTANT);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, "1e-3");
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_EOF);
}

void test_lex_float_constant_with_no_fractional_part(void) {
        char* input = "1.";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    token_t token = lscan(&lexer);
    CU_ASSERT_EQUAL_FATAL(token.kind, TK_FLOATING_CONSTANT);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, "1.");
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_EOF);
}

void test_lex_float_constant_with_no_whole_part(void) {
        char* input = ".5";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    token_t token = lscan(&lexer);
    CU_ASSERT_EQUAL_FATAL(token.kind, TK_FLOATING_CONSTANT);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, ".5");
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_EOF);
}

void test_lex_decimal_constant(void) {
        char* input = "123456789";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    token_t token = lscan(&lexer);
    CU_ASSERT_EQUAL_FATAL(token.kind, TK_INTEGER_CONSTANT);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, "123456789");
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_EOF);
}

void test_lex_decimal_constant_with_suffix(void) {
        char* input = "42ull";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    token_t token = lscan(&lexer);
    CU_ASSERT_EQUAL_FATAL(token.kind, TK_INTEGER_CONSTANT);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, "42ull");
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_EOF);
}

void test_lex_hexadecimal_constant(void) {
        char* input = "0xFF05";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    token_t token = lscan(&lexer);
    CU_ASSERT_EQUAL_FATAL(token.kind, TK_INTEGER_CONSTANT);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, "0xFF05");
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_EOF);
}

void test_lex_floating_hexadecimal_constant(void) {
        char* input = "0x1.5p-3";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    token_t token = lscan(&lexer);
    CU_ASSERT_EQUAL_FATAL(token.kind, TK_FLOATING_CONSTANT);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, "0x1.5p-3");
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_EOF);
}

void test_lex_octal_constant(void) {
        char* input = "01234567";
    lexer_t lexer = linit("path/to/file", input, strlen(input));
    token_t token = lscan(&lexer);
    CU_ASSERT_EQUAL_FATAL(token.kind, TK_INTEGER_CONSTANT);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, "01234567");
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_EOF);
}

void test__FILE__substitution(void) {
    char* input_path = "file-substitution.c";
    char* source_buffer = "__FILE__\n";
        lexer_t lexer = linit(input_path, source_buffer, strlen(source_buffer));

    token_vector_t tokens = {.buffer = NULL, .size = 0, .capacity = 0};
    token_t token;
    while ((token = lscan(&lexer)).kind != TK_EOF) {
        append_token(&tokens.buffer, &tokens.size, &tokens.capacity, token);
    }

    CU_ASSERT_EQUAL_FATAL(tokens.size, 1)
    CU_ASSERT_EQUAL_FATAL(tokens.buffer[0].kind, TK_STRING_LITERAL)
    CU_ASSERT_STRING_EQUAL_FATAL(tokens.buffer[0].value, "file-substitution.c")

    VEC_DESTROY(&tokens);
}

void test__LINE__substitution(void) {
    char* input_path = "line-substitution.c";
    char* source_buffer = "__LINE__\n__LINE__\n__LINE__\n";
        lexer_t lexer = linit(input_path, source_buffer, strlen(source_buffer));

    token_vector_t tokens = {.buffer = NULL, .size = 0, .capacity = 0};
    token_t token;
    while ((token = lscan(&lexer)).kind != TK_EOF) {
        append_token(&tokens.buffer, &tokens.size, &tokens.capacity, token);
    }

    CU_ASSERT_EQUAL_FATAL(tokens.size, 3)
    CU_ASSERT_EQUAL_FATAL(tokens.buffer[0].kind, TK_INTEGER_CONSTANT)
    CU_ASSERT_EQUAL_FATAL(strcmp(tokens.buffer[0].value, "1"), 0)
    CU_ASSERT_EQUAL_FATAL(tokens.buffer[1].kind, TK_INTEGER_CONSTANT)
    CU_ASSERT_EQUAL_FATAL(strcmp(tokens.buffer[1].value, "2"), 0)
    CU_ASSERT_EQUAL_FATAL(tokens.buffer[2].kind, TK_INTEGER_CONSTANT)
    CU_ASSERT_EQUAL_FATAL(strcmp(tokens.buffer[2].value, "3"), 0)

    VEC_DESTROY(&tokens);
}

void test_lex_line_directive(void) {
    char *input_path = "line-directive.c";
    char *source_buffer =
        "a\n"
        "#line 42 \"new-file.c\"\n"
        "b\n";
        lexer_t lexer = linit(input_path, source_buffer, strlen(source_buffer));

    token_vector_t tokens = {.buffer = NULL, .size = 0, .capacity = 0};
    token_t token;
    while ((token = lscan(&lexer)).kind != TK_EOF) {
        append_token(&tokens.buffer, &tokens.size, &tokens.capacity, token);
    }

    CU_ASSERT_EQUAL_FATAL(tokens.size, 2)

    token_t a = tokens.buffer[0];
    CU_ASSERT_EQUAL_FATAL(a.kind, TK_IDENTIFIER)
    CU_ASSERT_STRING_EQUAL_FATAL(a.value, "a")
    CU_ASSERT_STRING_EQUAL_FATAL(a.position.path, "line-directive.c")
    CU_ASSERT_EQUAL_FATAL(a.position.line, 1)
    CU_ASSERT_EQUAL_FATAL(a.position.column, 1)

    token_t b = tokens.buffer[1];
    CU_ASSERT_EQUAL_FATAL(b.kind, TK_IDENTIFIER)
    CU_ASSERT_STRING_EQUAL_FATAL(b.value, "b")
    CU_ASSERT_STRING_EQUAL_FATAL(b.position.path, "new-file.c")
    CU_ASSERT_EQUAL_FATAL(b.position.line, 42)
    CU_ASSERT_EQUAL_FATAL(b.position.column, 1)

    VEC_DESTROY(&tokens);
}

void test_lex_alt_line_directive(void) {
    char *input_path = "line-directive.c";
    char *source_buffer =
        "a\n"
        "# 42 \"new-file.c\" 1 2 3\n"
        "b\n";
    lexer_t lexer = linit(input_path, source_buffer, strlen(source_buffer));

    token_vector_t tokens = {.buffer = NULL, .size = 0, .capacity = 0};
    token_t token;
    while ((token = lscan(&lexer)).kind != TK_EOF) {
        append_token(&tokens.buffer, &tokens.size, &tokens.capacity, token);
    }

    CU_ASSERT_EQUAL_FATAL(tokens.size, 2)

    token_t a = tokens.buffer[0];
    CU_ASSERT_EQUAL_FATAL(a.kind, TK_IDENTIFIER)
    CU_ASSERT_STRING_EQUAL_FATAL(a.value, "a")
    CU_ASSERT_STRING_EQUAL_FATAL(a.position.path, "line-directive.c")
    CU_ASSERT_EQUAL_FATAL(a.position.line, 1)
    CU_ASSERT_EQUAL_FATAL(a.position.column, 1)

    token_t b = tokens.buffer[1];
    CU_ASSERT_EQUAL_FATAL(b.kind, TK_IDENTIFIER)
    CU_ASSERT_STRING_EQUAL_FATAL(b.value, "b")
    CU_ASSERT_STRING_EQUAL_FATAL(b.position.path, "new-file.c")
    CU_ASSERT_EQUAL_FATAL(b.position.line, 42)
    CU_ASSERT_EQUAL_FATAL(b.position.column, 1)

    VEC_DESTROY(&tokens);
}

void test_lex_ignore_unknown_preprocessor_directives(void) {
    char *input_path = "test.c";
    char *source_buffer =
        "#include <stdio.h>\n"
        "int a;\n";
    lexer_t lexer = linit(input_path, source_buffer, strlen(source_buffer));

    token_vector_t tokens = {.buffer = NULL, .size = 0, .capacity = 0};
    token_t token;
    while ((token = lscan(&lexer)).kind != TK_EOF) {
        append_token(&tokens.buffer, &tokens.size, &tokens.capacity, token);
    }

    CU_ASSERT_EQUAL_FATAL(tokens.size, 3)
    CU_ASSERT_EQUAL_FATAL(tokens.buffer[0].kind, TK_INT)
    CU_ASSERT_EQUAL_FATAL(tokens.buffer[1].kind, TK_IDENTIFIER)
    CU_ASSERT_STRING_EQUAL_FATAL(tokens.buffer[1].value, "a")
    CU_ASSERT_EQUAL_FATAL(tokens.buffer[2].kind, TK_SEMICOLON)

    VEC_DESTROY(&tokens);
}

int lexer_tests_init_suite(void) {
    CU_pSuite pSuite = CU_add_suite("lexer", NULL, NULL);
    if (NULL == CU_add_test(pSuite, "lex simple test program", test_simple_program) ||
        NULL == CU_add_test(pSuite, "lex float constant (zero)", test_lex_float_constant_0) ||
        NULL == CU_add_test(pSuite, "lex float constant (non-zero)", test_lex_float_constant) ||
        NULL == CU_add_test(pSuite, "lex float constant with exponent", test_lex_float_constant_with_exponent) ||
        NULL == CU_add_test(pSuite, "lex float constant with exponent and suffix", test_lex_float_constant_with_exponent_and_suffix) ||
        NULL == CU_add_test(pSuite, "lex float constant with no fractional part", test_lex_float_constant_with_no_fractional_part) ||
        NULL == CU_add_test(pSuite, "lex float constant with no whole part", test_lex_float_constant_with_no_whole_part) ||
        NULL == CU_add_test(pSuite, "lex float constant with no fractional part and exponent", test_lex_float_constant_with_no_fractional_part_and_exponent) ||
        NULL == CU_add_test(pSuite, "lex decimal constant", test_lex_decimal_constant) ||
        NULL == CU_add_test(pSuite, "lex decimal constant with suffix", test_lex_decimal_constant_with_suffix) ||
        NULL == CU_add_test(pSuite, "lex hexadecimal constant", test_lex_hexadecimal_constant) ||
        NULL == CU_add_test(pSuite, "lex floating hexadecimal constant", test_lex_floating_hexadecimal_constant) ||
        NULL == CU_add_test(pSuite, "lex octal constant", test_lex_octal_constant) ||
        NULL == CU_add_test(pSuite, "__FILE__ substitution", test__FILE__substitution) ||
        NULL == CU_add_test(pSuite, "__LINE__ substitution", test__LINE__substitution) ||
        NULL == CU_add_test(pSuite, "line directive", test_lex_line_directive) ||
        NULL == CU_add_test(pSuite, "alt line directive", test_lex_alt_line_directive) ||
        NULL == CU_add_test(pSuite, "ignore unknown preprocessor directives", test_lex_ignore_unknown_preprocessor_directives)
    ) {
        CU_cleanup_registry();
        return CU_get_error();
    }
    return 0;
}
