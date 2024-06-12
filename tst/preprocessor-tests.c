#include <stdlib.h>
#include <unistd.h>
#include "CUnit/Basic.h"
#include "test-common.h"
#include "tests.h"
#include "parser/lexer.h"

static lexer_global_context_t create_context() {
    return (lexer_global_context_t) {
            .user_include_paths = NULL,
            .system_include_paths = NULL,
            .macro_definitions = hash_table_create_string_keys(128)
    };
}

void test_includes_file(
        char* input_path,
        string_vector_t* user_includes_paths,
        string_vector_t* system_includes_path
) {
    lexer_global_context_t context = {
            .user_include_paths = user_includes_paths,
            .system_include_paths = system_includes_path,
            .macro_definitions = hash_table_create_string_keys(16)
    };

    FILE* file = fopen(input_path, "r");
    CU_ASSERT_PTR_NOT_NULL_FATAL(file);
    char* source_buffer = NULL;
    size_t len = 0;
    ssize_t bytes_read = getdelim( &source_buffer, &len, '\0', file);
    CU_ASSERT_FATAL(bytes_read > 0);
    fclose(file);

    lexer_t lexer = linit(input_path, source_buffer, len, &context);
    token_t token;
    token_vector_t tokens = {NULL, 0, 0};
    while ((token = lscan(&lexer)).kind != TK_EOF) {
        append_token(&tokens.buffer, &tokens.size, &tokens.capacity, token);
    }

    token_kind_t expected_tokens[] = {
            TK_STATIC,
            TK_CONST,
            TK_INT,
            TK_IDENTIFIER,
            TK_ASSIGN,
            TK_INTEGER_CONSTANT,
            TK_SEMICOLON,
            TK_CONST,
            TK_INT,
            TK_IDENTIFIER,
            TK_ASSIGN,
            TK_IDENTIFIER,
            TK_STAR,
            TK_INTEGER_CONSTANT,
            TK_SEMICOLON
    };
    TEST_ASSERT_ARRAYS_EQUAL(expected_tokens, 15, token_kind_array(tokens.buffer, tokens.size), tokens.size, format_token_kind_array)

    CU_ASSERT_STRING_EQUAL_FATAL(tokens.buffer[3].value, "b");
    CU_ASSERT_STRING_EQUAL_FATAL(tokens.buffer[5].value, "4");
    CU_ASSERT_STRING_EQUAL_FATAL(tokens.buffer[9].value, "a");
    CU_ASSERT_STRING_EQUAL_FATAL(tokens.buffer[11].value, "b");
    CU_ASSERT_STRING_EQUAL_FATAL(tokens.buffer[13].value, "2");
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

void test_macro_define_and_replace_macro_no_body() {
    char*  input_path = "define-no-body.c";
    char* source_buffer = "#define FOO\nFOO\n";
    lexer_global_context_t context = create_context();
    lexer_t lexer = linit(input_path, source_buffer, strlen(source_buffer), &context);

    token_vector_t tokens = {NULL, 0, 0};
    token_t token;
    while ((token = lscan(&lexer)).kind != TK_EOF) {
        append_token(&tokens.buffer, &tokens.size, &tokens.capacity, token);
    }

    // verify that the macro was parsed correctly
    CU_ASSERT_EQUAL_FATAL(context.macro_definitions.size, 1)
    macro_definition_t* definition = malloc(sizeof(macro_definition_t));
    CU_ASSERT_TRUE_FATAL(hash_table_lookup(&context.macro_definitions, "FOO", (void**) &definition))
    CU_ASSERT_EQUAL_FATAL(definition->tokens.size, 0)
    CU_ASSERT_EQUAL_FATAL(definition->parameters.size, 0)
    CU_ASSERT_EQUAL_FATAL(definition->variadic, false)

    // verify that the macro was expanded correctly
    CU_ASSERT_EQUAL_FATAL(tokens.size, 0)
}

void test_macro_define_and_replace_macro_with_body() {
    char*  input_path = "define-with-body.c";
    char* source_buffer = "#define HELLO_WORLD printf(\"hello world!\");\nHELLO_WORLD\n";
    lexer_global_context_t context = create_context();
    lexer_t lexer = linit(input_path, source_buffer, strlen(source_buffer), &context);

    token_vector_t tokens = {.buffer = NULL, .size = 0, .capacity = 0};
    token_t token;
    while ((token = lscan(&lexer)).kind != TK_EOF) {
        append_token(&tokens.buffer, &tokens.size, &tokens.capacity, token);
    }

    CU_ASSERT_EQUAL_FATAL(context.macro_definitions.size, 1)
    macro_definition_t* definition = malloc(sizeof(macro_definition_t));
    CU_ASSERT_TRUE_FATAL(hash_table_lookup(&context.macro_definitions, "HELLO_WORLD", (void**) &definition))
    CU_ASSERT_EQUAL_FATAL(definition->tokens.size, 5)
    CU_ASSERT_EQUAL_FATAL(definition->parameters.size, 0)
    CU_ASSERT_EQUAL_FATAL(definition->variadic, false)
    token_kind_t expected_tokens[] = {TK_IDENTIFIER, TK_LPAREN, TK_STRING_LITERAL, TK_RPAREN, TK_SEMICOLON};
    for (int i = 0; i < 5; i++) {
        CU_ASSERT_EQUAL_FATAL(definition->tokens.buffer[i].kind, expected_tokens[i])
    }
    CU_ASSERT_STRING_EQUAL_FATAL(definition->tokens.buffer[0].value, "printf")
    CU_ASSERT_STRING_EQUAL_FATAL(definition->tokens.buffer[2].value, "hello world!")

    // verify that the macro was expanded correctly
    CU_ASSERT_EQUAL_FATAL(tokens.size, 5)
    for (int i = 0; i < 5; i++) {
        CU_ASSERT_EQUAL_FATAL(tokens.buffer[i].kind, expected_tokens[i])
    }
}

void test_macro_define_and_replace_parameterized_macro() {
    char*  input_path = "define-with-parameters.c";
    char* source_buffer = "#define SUM(a, b) a + b\nSUM((3 * 3),2)\n";
    lexer_global_context_t context = create_context();
    lexer_t lexer = linit(input_path, source_buffer, strlen(source_buffer), &context);

    token_vector_t tokens = {.buffer = NULL, .size = 0, .capacity = 0};
    token_t token;
    while ((token = lscan(&lexer)).kind != TK_EOF) {
        append_token(&tokens.buffer, &tokens.size, &tokens.capacity, token);
    }

    CU_ASSERT_EQUAL_FATAL(context.macro_definitions.size, 1)
    macro_definition_t* definition = malloc(sizeof(macro_definition_t));
    CU_ASSERT_TRUE_FATAL(hash_table_lookup(&context.macro_definitions, "SUM", (void**) &definition))
    CU_ASSERT_EQUAL_FATAL(definition->tokens.size, 3)
    CU_ASSERT_EQUAL_FATAL(definition->parameters.size, 2)
    CU_ASSERT_EQUAL_FATAL(definition->variadic, false)

    token_kind_t definition_expected_tokens[] = {TK_IDENTIFIER, TK_PLUS, TK_IDENTIFIER};
    for (int i = 0; i < 3; i++) {
        CU_ASSERT_EQUAL_FATAL(definition->tokens.buffer[i].kind, definition_expected_tokens[i])
    }
    CU_ASSERT_STRING_EQUAL_FATAL(definition->tokens.buffer[0].value, "a")
    CU_ASSERT_STRING_EQUAL_FATAL(definition->tokens.buffer[2].value, "b")

    // verify that the macro was expanded correctly
    token_kind_t expansion_expected_tokens[] = {
            TK_LPAREN,
            TK_INTEGER_CONSTANT,
            TK_STAR,
            TK_INTEGER_CONSTANT,
            TK_RPAREN,
            TK_PLUS,
            TK_INTEGER_CONSTANT
    };
    CU_ASSERT_EQUAL_FATAL(tokens.size, (sizeof(expansion_expected_tokens) / sizeof(token_kind_t)))
    for (int i = 0; i < tokens.size; i++) {
        CU_ASSERT_EQUAL_FATAL(tokens.buffer[i].kind, expansion_expected_tokens[i])
    }
}

void test_macro_define_and_replace_stringification() {
    char*  input_path = "define-with-parameters.c";
    char* source_buffer = "#define STRINGIFY(a) #a\nSTRINGIFY(foo)\n";
    lexer_global_context_t context = create_context();
    lexer_t lexer = linit(input_path, source_buffer, strlen(source_buffer), &context);

    token_vector_t tokens = {.buffer = NULL, .size = 0, .capacity = 0};
    token_t token;
    while ((token = lscan(&lexer)).kind != TK_EOF) {
        append_token(&tokens.buffer, &tokens.size, &tokens.capacity, token);
    }

    CU_ASSERT_EQUAL_FATAL(context.macro_definitions.size, 1)
    macro_definition_t* macro_definition = NULL;
    CU_ASSERT_TRUE_FATAL(hash_table_lookup(&context.macro_definitions, "STRINGIFY", (void**) &macro_definition))
    CU_ASSERT_EQUAL_FATAL(macro_definition->tokens.size, 2)
    token_kind_t expected_macro_tokens[] = {TK_HASH, TK_IDENTIFIER};
    for (int i = 0; i < 2; i++) {
        CU_ASSERT_EQUAL_FATAL(macro_definition->tokens.buffer[i].kind, expected_macro_tokens[i])
    }

    CU_ASSERT_EQUAL_FATAL(tokens.size, 1)
    CU_ASSERT_EQUAL_FATAL(tokens.buffer[0].kind, TK_STRING_LITERAL)
    CU_ASSERT_STRING_EQUAL_FATAL(tokens.buffer[0].value, "foo")
}

void test_macro_define_and_replace_token_pasting() {
    char*  input_path = "define-with-parameters.c";
    char* source_buffer = "#define PASTE(a, b) a ## b\nPASTE(foo, bar)\n";
    lexer_global_context_t context = create_context();
    lexer_t lexer = linit(input_path, source_buffer, strlen(source_buffer), &context);

    token_vector_t tokens = {.buffer = NULL, .size = 0, .capacity = 0};
    token_t token;
    while ((token = lscan(&lexer)).kind != TK_EOF) {
        append_token(&tokens.buffer, &tokens.size, &tokens.capacity, token);
    }

    CU_ASSERT_EQUAL_FATAL(context.macro_definitions.size, 1)
    macro_definition_t* macro_definition = NULL;
    CU_ASSERT_TRUE_FATAL(hash_table_lookup(&context.macro_definitions, "PASTE", (void**) &macro_definition))
    token_kind_t expected_macro_tokens[] = {TK_IDENTIFIER, TK_DOUBLE_HASH, TK_IDENTIFIER};
    TEST_ASSERT_ARRAYS_EQUAL(expected_macro_tokens, 3,
                             token_kind_array(macro_definition->tokens.buffer, macro_definition->tokens.size), macro_definition->tokens.size,
                             format_token_kind_array)

    CU_ASSERT_EQUAL_FATAL(tokens.size, 1)
    CU_ASSERT_EQUAL_FATAL(tokens.buffer[0].kind, TK_IDENTIFIER)
    CU_ASSERT_STRING_EQUAL_FATAL(tokens.buffer[0].value, "foobar")
}

void test_macro_define_and_replace_varargs() {
    char* input_path = "define-with-varargs.c";
    char* source_buffer = "#define PRINT(stream, ...) fprintf(stream, __VA_ARGS__)\nPRINT(stdout, \"hello %s!\", \"world\");\n";
    lexer_global_context_t context = create_context();
    lexer_t lexer = linit(input_path, source_buffer, strlen(source_buffer), &context);

    token_vector_t tokens = {.buffer = NULL, .size = 0, .capacity = 0};
    token_t token;
    while ((token = lscan(&lexer)).kind != TK_EOF) {
        append_token(&tokens.buffer, &tokens.size, &tokens.capacity, token);
    }

    CU_ASSERT_EQUAL_FATAL(context.macro_definitions.size, 1)
    macro_definition_t* macro_definition = NULL;
    CU_ASSERT_TRUE_FATAL(hash_table_lookup(&context.macro_definitions, "PRINT", (void**) &macro_definition))

    token_kind_t expected_macro_tokens[] = {
            TK_IDENTIFIER,
            TK_LPAREN,
            TK_IDENTIFIER,
            TK_COMMA,
            TK_IDENTIFIER,
            TK_RPAREN
    };
    TEST_ASSERT_ARRAYS_EQUAL(expected_macro_tokens, 6,
                             token_kind_array(macro_definition->tokens.buffer, macro_definition->tokens.size), macro_definition->tokens.size,
                             format_token_kind_array)

    token_kind_t expected_expansion_tokens[] = {
            TK_IDENTIFIER,
            TK_LPAREN,
            TK_IDENTIFIER,
            TK_COMMA,
            TK_STRING_LITERAL,
            TK_COMMA,
            TK_STRING_LITERAL,
            TK_RPAREN,
            TK_SEMICOLON
    };
    TEST_ASSERT_ARRAYS_EQUAL(expected_expansion_tokens, 9, token_kind_array(tokens.buffer, tokens.size), tokens.size, format_token_kind_array)
}

void test_macro_define_and_replace_parameter_expansion() {
    char* input_path = "define-with-parameter-expansion.c";
    char* source_buffer = "#define FOO(a) a\n#define BAR b\nFOO(BAR)\n";
    lexer_global_context_t context = create_context();
    lexer_t lexer = linit(input_path, source_buffer, strlen(source_buffer), &context);

    token_vector_t tokens = {.buffer = NULL, .size = 0, .capacity = 0};
    token_t token;
    while ((token = lscan(&lexer)).kind != TK_EOF) {
        append_token(&tokens.buffer, &tokens.size, &tokens.capacity, token);
    }

    token_kind_t expected_expansion_tokens[] = {TK_IDENTIFIER};
    TEST_ASSERT_ARRAYS_EQUAL(expected_expansion_tokens, 1, token_kind_array(tokens.buffer, tokens.size), tokens.size, format_token_kind_array)
    const char* expected_expansion_value[] = {"b"};
    TEST_ASSERT_STRING_ARRAYS_EQUAL(expected_expansion_value, 1, token_value_array(tokens.buffer, tokens.size), tokens.size)
}

void test_macro_define_and_replace_parameter_name_is_defined_macro() {
    char* input_path = "define-with-parameter-name-is-defined-macro.c";
    char* source_buffer = "#define BAR 42\n#define FOO(BAR) BAR\nFOO(baz)\n";
    lexer_global_context_t context = create_context();
    lexer_t lexer = linit(input_path, source_buffer, strlen(source_buffer), &context);

    token_vector_t tokens = {.buffer = NULL, .size = 0, .capacity = 0};
    token_t token;
    while ((token = lscan(&lexer)).kind != TK_EOF) {
        append_token(&tokens.buffer, &tokens.size, &tokens.capacity, token);
    }

    token_kind_t expected_expansion_tokens[] = {TK_IDENTIFIER};
    TEST_ASSERT_ARRAYS_EQUAL(expected_expansion_tokens, 1, token_kind_array(tokens.buffer, tokens.size), tokens.size, format_token_kind_array)
    const char* expected_expansion_value[] = {"baz"};
    TEST_ASSERT_STRING_ARRAYS_EQUAL(expected_expansion_value, 1, token_value_array(tokens.buffer, tokens.size), tokens.size)
}

void test_macro_define_and_undefine() {
    char* input_path = "define-and-undefine-macro.c";
    char* source_buffer = "#define FOO 42\n#undef FOO\nFOO\n";
    lexer_global_context_t context = create_context();
    lexer_t lexer = linit(input_path, source_buffer, strlen(source_buffer), &context);

    token_vector_t tokens = {.buffer = NULL, .size = 0, .capacity = 0};
    token_t token;
    while ((token = lscan(&lexer)).kind != TK_EOF) {
        append_token(&tokens.buffer, &tokens.size, &tokens.capacity, token);
    }

    CU_ASSERT_EQUAL_FATAL(context.macro_definitions.size, 0)
    CU_ASSERT_EQUAL_FATAL(tokens.size, 1)
    CU_ASSERT_EQUAL_FATAL(tokens.buffer[0].kind, TK_IDENTIFIER)
    CU_ASSERT_STRING_EQUAL_FATAL(tokens.buffer[0].value, "FOO")
}

void test__FILE__substitution() {
    char* input_path = "file-substitution.c";
    char* source_buffer = "__FILE__\n";
    lexer_global_context_t context = create_context();
    lexer_t lexer = linit(input_path, source_buffer, strlen(source_buffer), &context);

    token_vector_t tokens = {.buffer = NULL, .size = 0, .capacity = 0};
    token_t token;
    while ((token = lscan(&lexer)).kind != TK_EOF) {
        append_token(&tokens.buffer, &tokens.size, &tokens.capacity, token);
    }

    CU_ASSERT_EQUAL_FATAL(tokens.size, 1)
    CU_ASSERT_EQUAL_FATAL(tokens.buffer[0].kind, TK_STRING_LITERAL)
    CU_ASSERT_STRING_EQUAL_FATAL(tokens.buffer[0].value, "file-substitution.c")
}

void test__LINE__substitution() {
    char* input_path = "line-substitution.c";
    char* source_buffer = "__LINE__\n__LINE__\n__LINE__\n";
    lexer_global_context_t context = create_context();
    lexer_t lexer = linit(input_path, source_buffer, strlen(source_buffer), &context);

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
}

int preprocessor_tests_init_suite() {
    CU_pSuite pSuite = CU_add_suite("preprocessor", NULL, NULL);
    if (NULL == CU_add_test(pSuite, "#include - relative path", test_includes_header_relative_path) ||
        NULL == CU_add_test(pSuite, "#include - additional include directory", test_includes_header_additional_directory) ||
        NULL == CU_add_test(pSuite, "#define - no body", test_macro_define_and_replace_macro_no_body) ||
        NULL == CU_add_test(pSuite, "#define - with multi-token body", test_macro_define_and_replace_macro_with_body) ||
        NULL == CU_add_test(pSuite, "#define - with parameters", test_macro_define_and_replace_parameterized_macro) ||
        NULL == CU_add_test(pSuite, "#define - parameter stringification", test_macro_define_and_replace_stringification) ||
        NULL == CU_add_test(pSuite, "#define - token pasting", test_macro_define_and_replace_token_pasting) ||
        NULL == CU_add_test(pSuite, "#define - variadic macro", test_macro_define_and_replace_varargs) ||
        NULL == CU_add_test(pSuite, "#define - parameter expansion", test_macro_define_and_replace_parameter_expansion) ||
        NULL == CU_add_test(pSuite, "#define - parameter name is defined macro", test_macro_define_and_replace_parameter_name_is_defined_macro) ||
        NULL == CU_add_test(pSuite, "#define - and #undef", test_macro_define_and_undefine) ||
        NULL == CU_add_test(pSuite, "__FILE__", test__FILE__substitution) ||
        NULL == CU_add_test(pSuite, "__LINE__", test__LINE__substitution)
    ) {
        CU_cleanup_registry();
        exit(CU_get_error());
    }

    return 0;
}
