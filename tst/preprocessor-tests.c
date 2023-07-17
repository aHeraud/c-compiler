#include <stdlib.h>
#include <unistd.h>
#include "CUnit/Basic.h"
#include "tests.h"
#include "lexer.h"

void test_includes_file(
        char* input_path,
        string_vector_t* user_includes_paths,
        string_vector_t* system_includes_path
) {
    FILE* file = fopen(input_path, "r");
    CU_ASSERT_PTR_NOT_NULL_FATAL(file);
    char* source_buffer = NULL;
    size_t len = 0;
    ssize_t bytes_read = getdelim( &source_buffer, &len, '\0', file);
    CU_ASSERT_FATAL(bytes_read > 0);
    fclose(file);

    lexer_t lexer = linit(input_path, source_buffer, len, user_includes_paths, system_includes_path);
    token_t token;
    token_vector_t tokens = {NULL, 0, 0};
    while ((token = lscan(&lexer)).kind != TK_EOF) {
        append_token(&tokens.buffer, &tokens.size, &tokens.capacity, token);
    }

    CU_ASSERT_EQUAL_FATAL(tokens.size, 15);
    CU_ASSERT_EQUAL_FATAL(tokens.buffer[0].kind, TK_STATIC);
    CU_ASSERT_EQUAL_FATAL(tokens.buffer[1].kind, TK_CONST);
    CU_ASSERT_EQUAL_FATAL(tokens.buffer[2].kind, TK_INT);
    CU_ASSERT_EQUAL_FATAL(tokens.buffer[3].kind, TK_IDENTIFIER);
    CU_ASSERT_STRING_EQUAL_FATAL(tokens.buffer[3].value, "b");
    CU_ASSERT_EQUAL_FATAL(tokens.buffer[4].kind, TK_ASSIGN);
    CU_ASSERT_EQUAL_FATAL(tokens.buffer[5].kind, TK_INTEGER_CONSTANT);
    CU_ASSERT_STRING_EQUAL_FATAL(tokens.buffer[5].value, "4");
    CU_ASSERT_EQUAL_FATAL(tokens.buffer[6].kind, TK_SEMICOLON);
    CU_ASSERT_EQUAL_FATAL(tokens.buffer[7].kind, TK_CONST);
    CU_ASSERT_EQUAL_FATAL(tokens.buffer[8].kind, TK_INT);
    CU_ASSERT_EQUAL_FATAL(tokens.buffer[9].kind, TK_IDENTIFIER);
    CU_ASSERT_STRING_EQUAL_FATAL(tokens.buffer[9].value, "a");
    CU_ASSERT_EQUAL_FATAL(tokens.buffer[10].kind, TK_ASSIGN);
    CU_ASSERT_EQUAL_FATAL(tokens.buffer[11].kind, TK_IDENTIFIER);
    CU_ASSERT_STRING_EQUAL_FATAL(tokens.buffer[11].value, "b");
    CU_ASSERT_EQUAL_FATAL(tokens.buffer[12].kind, TK_STAR);
    CU_ASSERT_EQUAL_FATAL(tokens.buffer[13].kind, TK_INTEGER_CONSTANT);
    CU_ASSERT_STRING_EQUAL_FATAL(tokens.buffer[13].value, "2");
    CU_ASSERT_EQUAL_FATAL(tokens.buffer[14].kind, TK_SEMICOLON);
}

void test_includes_header_relative_path() {
    char* input_path = "tst/test-input/include/a.c";
    test_includes_file(input_path, NULL, NULL);
}

void test_includes_header_additional_directory() {
    char* input_path = "tst/test-input/include/c.c";
    string_vector_t user_includes_paths = {NULL, 0, 0};
    append_ptr((void***) &user_includes_paths.buffer, &user_includes_paths.size, &user_includes_paths.capacity, "tst/test-input/include/dep");
    test_includes_file(input_path, &user_includes_paths, NULL);
}

int preprocessor_tests_init_suite() {
    CU_pSuite pSuite = CU_add_suite("preprocessor", NULL, NULL);
    if (NULL == CU_add_test(pSuite, "#include - relative path", test_includes_header_relative_path)) {
        CU_cleanup_registry();
        exit(CU_get_error());
    }
    if (NULL == CU_add_test(pSuite, "#include - additional include directory", test_includes_header_additional_directory)) {
        CU_cleanup_registry();
        exit(CU_get_error());
    }
    return 0;
}