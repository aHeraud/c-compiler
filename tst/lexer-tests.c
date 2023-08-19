#include <malloc.h>
#include "CUnit/Basic.h"
#include "tests.h"
#include "lexer.h"

void test_simple_program() {
    lexer_global_context_t context = {
            .user_include_paths = NULL,
            .system_include_paths = NULL,
            .macro_definitions = {
                    .size = 0,
                    .num_buckets = 10,
                    .buckets = calloc(10, sizeof(hashtable_entry_t *)),
            }
    };

    char* input = "/*multi line\ncomment*/\nint main() {\n    return 0; // comment\n}";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    token_t token;
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_COMMENT);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, "/*multi line\ncomment*/");
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
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_COMMENT);
    CU_ASSERT_STRING_EQUAL_FATAL(token.value, "// comment");
    //CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_NEWLINE);
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_RBRACE);
    CU_ASSERT_EQUAL_FATAL((token = lscan(&lexer)).kind, TK_EOF);
}

int lexer_tests_init_suite() {
    CU_pSuite pSuite = CU_add_suite("lexer", NULL, NULL);
    if (NULL == CU_add_test(pSuite, "lex simple test program", test_simple_program)) {
        CU_cleanup_registry();
        return CU_get_error();
    }
}
