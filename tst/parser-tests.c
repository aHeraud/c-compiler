#include <malloc.h>
#include "CUnit/Basic.h"
#include "tests.h"
#include "parser.h"

static lexer_global_context_t create_context() {
    return (lexer_global_context_t) {
            .user_include_paths = NULL,
            .system_include_paths = NULL,
            .macro_definitions = {
                    .size = 0,
                    .num_buckets = 10,
                    .buckets = calloc(10, sizeof(hashtable_entry_t *)),
            }
    };
}

void test_primary_expression_ident() {
    lexer_global_context_t context = create_context();
    char* input = "bar";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = {
            .token = lscan(&lexer),
            .lexer = lexer,
    };
    ast_node_t node;
    bool matches = primary_expression(&parser, &node);
    CU_ASSERT_TRUE(matches);
    CU_ASSERT_EQUAL(node.type, AST_PRIMARY_EXPRESSION);
    CU_ASSERT_EQUAL(node.primary_expression.type, PE_IDENTIFIER);
    CU_ASSERT_STRING_EQUAL(node.primary_expression.identifier.name, "bar");
}

void test_declaration_simple() {
    lexer_global_context_t context = create_context();
    char* input = "int foo = 5;";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    token_t token = lscan(&lexer);
    parser_t parser = {lexer,token};
    ast_node_t node;
    bool matches = declaration(&parser, &node);
    CU_ASSERT_TRUE_FATAL(matches);
    CU_ASSERT_EQUAL_FATAL(parser.token.kind, TK_EOF);

    ast_node_t* decl_specifiers_node = node.declaration.declaration_specifiers;
    CU_ASSERT_EQUAL_FATAL(decl_specifiers_node->declaration_specifiers.size, 1);
    CU_ASSERT_EQUAL_FATAL(decl_specifiers_node->declaration_specifiers.buffer[0]->type, AST_TYPE_SPECIFIER);
    CU_ASSERT_EQUAL_FATAL(decl_specifiers_node->declaration_specifiers.buffer[0]->type_specifier, TYPE_SPECIFIER_INT);

    ast_node_t* init_declarators_node = node.declaration.init_declarators;
    CU_ASSERT_NOT_EQUAL_FATAL(init_declarators_node, NULL);
    CU_ASSERT_EQUAL_FATAL(init_declarators_node->init_declarator_list.size, 1);
    CU_ASSERT_EQUAL_FATAL(init_declarators_node->init_declarator_list.buffer[0]->type, AST_INIT_DECLARATOR);

    ast_node_t* declarator = init_declarators_node->init_declarator_list.buffer[0]->init_declarator.declarator;
    CU_ASSERT_EQUAL_FATAL(declarator->type, AST_DECLARATOR);
    CU_ASSERT_EQUAL_FATAL(declarator->declarator.pointer, NULL);

    ast_node_t* direct_declarator_node = declarator->declarator.direct_declarator;
    CU_ASSERT_EQUAL_FATAL(direct_declarator_node->type, AST_DIRECT_DECLARATOR);
    CU_ASSERT_EQUAL_FATAL(direct_declarator_node->direct_declarator.type, DECL_IDENTIFIER);
    CU_ASSERT_STRING_EQUAL_FATAL(direct_declarator_node->direct_declarator.identifier.name, "foo");

    ast_node_t* initializer_node = init_declarators_node->init_declarator_list.buffer[0]->init_declarator.initializer;
    CU_ASSERT_EQUAL_FATAL(initializer_node->type, AST_INITIALIZER);
    CU_ASSERT_EQUAL_FATAL(initializer_node->initializer.type, INITIALIZER_EXPRESSION);
    CU_ASSERT_EQUAL_FATAL(initializer_node->initializer.expression->type, AST_PRIMARY_EXPRESSION);
    CU_ASSERT_EQUAL_FATAL(initializer_node->initializer.expression->primary_expression.type, PE_CONSTANT);
    CU_ASSERT_EQUAL_FATAL(initializer_node->initializer.expression->primary_expression.constant.type, CONSTANT_INTEGER);
    CU_ASSERT_EQUAL_FATAL(initializer_node->initializer.expression->primary_expression.constant.integer, 5);
}

void test_declaration_function_proto_no_params() {
    lexer_global_context_t context = create_context();
    char* input = "inline float foo();";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    token_t token = lscan(&lexer);
    parser_t parser = {lexer,token};

    ast_node_t node;
    bool matches = declaration(&parser, &node);
    CU_ASSERT_TRUE_FATAL(matches);
    CU_ASSERT_EQUAL_FATAL(lscan(&parser.lexer).kind, TK_EOF); // verify that we've consumed all tokens
    CU_ASSERT_EQUAL_FATAL(node.type, AST_DECLARATION);

    ast_node_t* decl_specifiers = node.declaration.declaration_specifiers;
    CU_ASSERT_EQUAL_FATAL(decl_specifiers->type, AST_DECLARATION_SPECIFIERS);
    CU_ASSERT_EQUAL_FATAL(decl_specifiers->declaration_specifiers.size, 2);
    CU_ASSERT_EQUAL_FATAL(decl_specifiers->declaration_specifiers.buffer[0]->type, AST_FUNCTION_SPECIFIER);
    CU_ASSERT_EQUAL_FATAL(decl_specifiers->declaration_specifiers.buffer[0]->function_specifier, FUNCTION_SPECIFIER_INLINE);
    CU_ASSERT_EQUAL_FATAL(decl_specifiers->declaration_specifiers.buffer[1]->type, AST_TYPE_SPECIFIER);
    CU_ASSERT_EQUAL_FATAL(decl_specifiers->declaration_specifiers.buffer[1]->type_specifier, TYPE_SPECIFIER_FLOAT);
}

void test_function_definition() {
    lexer_global_context_t context = create_context();
    char* input = "int foo() { return 5; }";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    token_t token = lscan(&lexer);
    parser_t parser = {lexer,token};

    ast_node_t node;
    bool matches = external_declaration(&parser, &node);
    CU_ASSERT_TRUE_FATAL(matches);
    CU_ASSERT_EQUAL_FATAL(lscan(&parser.lexer).kind, TK_EOF); // verify that we've consumed all tokens
    CU_ASSERT_EQUAL_FATAL(node.type, AST_FUNCTION_DEFINITION);
}

int parser_tests_init_suite() {
    CU_pSuite pSuite = CU_add_suite("parser", NULL, NULL);
    if (NULL == CU_add_test(pSuite, "primary expression - identifier", test_primary_expression_ident)) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (NULL == CU_add_test(pSuite, "declaration - simple", test_declaration_simple)) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (NULL == CU_add_test(pSuite, "declaration - function prototype (no parameter list)", test_declaration_function_proto_no_params)) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (NULL == CU_add_test(pSuite, "function definition", test_function_definition)) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    return 0;
}
