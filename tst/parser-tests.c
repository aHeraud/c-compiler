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
    parser_t parser = pinit(lexer);
    ast_node_t node;
    bool matches = primary_expression(&parser, &node);
    CU_ASSERT_TRUE(matches);
    CU_ASSERT_EQUAL(node.type, AST_PRIMARY_EXPRESSION);
    CU_ASSERT_EQUAL(node.primary_expression.type, PE_IDENTIFIER);
    CU_ASSERT_STRING_EQUAL(node.primary_expression.identifier.name, "bar");
}

void test_parameter_type_list() {
    char* input = "int a, char* b, ...";
    lexer_global_context_t context = create_context();
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);

    ast_node_t node;
    CU_ASSERT_TRUE_FATAL(parameter_type_list(&parser, &node))
    CU_ASSERT_EQUAL_FATAL(node.type, AST_PARAMETER_TYPE_LIST)
    parameter_type_list_t list = node.parameter_type_list;
    CU_ASSERT_EQUAL_FATAL(list.parameter_list.size, 2)
    CU_ASSERT_TRUE_FATAL(list.variadic)
    parameter_declaration_t parameters[2] = {
            list.parameter_list.buffer[0]->parameter_declaration,
            list.parameter_list.buffer[1]->parameter_declaration
    };

    CU_ASSERT_EQUAL_FATAL(parameters[0].declaration_specifiers->declaration_specifiers.buffer[0]->type_specifier, TYPE_SPECIFIER_INT)
    CU_ASSERT_EQUAL_FATAL(parameters[0].declarator->declarator.direct_declarator->direct_declarator.type, DECL_IDENTIFIER)
    CU_ASSERT_STRING_EQUAL_FATAL(parameters[0].declarator->declarator.direct_declarator->direct_declarator.identifier.name, "a")

    CU_ASSERT_EQUAL_FATAL(parameters[1].declaration_specifiers->declaration_specifiers.buffer[0]->type_specifier, TYPE_SPECIFIER_CHAR)
    CU_ASSERT_PTR_NOT_NULL(parameters[1].declarator->declarator.pointer)
    CU_ASSERT_STRING_EQUAL_FATAL(parameters[1].declarator->declarator.direct_declarator->direct_declarator.identifier.name, "b")
}

void test_declaration_simple() {
    lexer_global_context_t context = create_context();
    char* input = "int foo = 5;";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);
    ast_node_t node;
    bool matches = declaration(&parser, &node);
    CU_ASSERT_TRUE_FATAL(matches);

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
    parser_t parser = pinit(lexer);

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

void test_declaration_function_proto_with_params() {
    lexer_global_context_t context = create_context();
    char* input = "inline float foo(int a, int b);";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);

    ast_node_t node;
    bool matches = declaration(&parser, &node);
    CU_ASSERT_TRUE_FATAL(matches);
}

void test_declaration_function_proto_with_abstract_params() {
    lexer_global_context_t context = create_context();
    char* input = "inline float foo(int[], int);";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);

    ast_node_t node;
    bool matches = declaration(&parser, &node);
    CU_ASSERT_TRUE_FATAL(matches);
}

void test_function_definition() {
    lexer_global_context_t context = create_context();
    char* input = "int foo() { return 5; }";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);

    ast_node_t node;
    bool matches = function_definition(&parser, &node);
    CU_ASSERT_TRUE_FATAL(matches)
    CU_ASSERT_EQUAL_FATAL(lscan(&parser.lexer).kind, TK_EOF) // verify that we've consumed all tokens
    CU_ASSERT_EQUAL_FATAL(node.type, AST_FUNCTION_DEFINITION)
}

void test_function_with_parameter_list() {
    lexer_global_context_t context = create_context();
    char* input = "int main(int argc, char** argv) {\n\treturn 0;\n}";
    lexer_t lexer = linit("path/to/file", input, strlen(input), &context);
    parser_t parser = pinit(lexer);

    ast_node_t node;
    bool matches = external_declaration(&parser, &node);

    CU_ASSERT_TRUE_FATAL(matches)
    CU_ASSERT_EQUAL_FATAL(node.type, AST_FUNCTION_DEFINITION)

    // return type = int
    CU_ASSERT_EQUAL_FATAL(node.function_definition.declaration_specifiers->declaration_specifiers.size, 1)
    CU_ASSERT_EQUAL_FATAL(node.function_definition.declaration_specifiers->declaration_specifiers.buffer[0]->type, AST_TYPE_SPECIFIER)
    CU_ASSERT_EQUAL_FATAL(node.function_definition.declaration_specifiers->declaration_specifiers.buffer[0]->type_specifier, TYPE_SPECIFIER_INT)

    ast_node_t* parameter_type_list_node = node.function_definition.declarator->declarator.direct_declarator->direct_declarator.function.param_type_or_ident_list;
    CU_ASSERT_PTR_NOT_NULL(parameter_type_list)
    CU_ASSERT_EQUAL_FATAL(parameter_type_list_node->type, AST_PARAMETER_TYPE_LIST)
    CU_ASSERT_EQUAL_FATAL(parameter_type_list_node->parameter_type_list.parameter_list.size, 2)
    CU_ASSERT_FALSE_FATAL(parameter_type_list_node->parameter_type_list.variadic)

    // first parameter, int argc
    CU_ASSERT_EQUAL_FATAL(parameter_type_list_node->parameter_type_list.parameter_list.buffer[0]->type, AST_PARAMETER_DECLARATION)
    parameter_declaration_t argc = parameter_type_list_node->parameter_type_list.parameter_list.buffer[0]->parameter_declaration;
    CU_ASSERT_EQUAL_FATAL(argc.declaration_specifiers->declaration_specifiers.size, 1)
    CU_ASSERT_EQUAL_FATAL(argc.declaration_specifiers->declaration_specifiers.buffer[0]->type, AST_TYPE_SPECIFIER)
    CU_ASSERT_EQUAL_FATAL(argc.declaration_specifiers->declaration_specifiers.buffer[0]->type_specifier, TYPE_SPECIFIER_INT)
    CU_ASSERT_EQUAL_FATAL(argc.declarator->declarator.direct_declarator->direct_declarator.type, DECL_IDENTIFIER)
    CU_ASSERT_EQUAL_FATAL(argc.declarator->declarator.pointer, NULL) // not a pointer
    CU_ASSERT_STRING_EQUAL_FATAL(argc.declarator->declarator.direct_declarator->direct_declarator.identifier.name, "argc")

    // second parameter, char** argv
    CU_ASSERT_EQUAL_FATAL(parameter_type_list_node->parameter_type_list.parameter_list.buffer[1]->type, AST_PARAMETER_DECLARATION)
    parameter_declaration_t argv = parameter_type_list_node->parameter_type_list.parameter_list.buffer[1]->parameter_declaration;
    CU_ASSERT_EQUAL_FATAL(argv.declaration_specifiers->declaration_specifiers.size, 1)
    CU_ASSERT_EQUAL_FATAL(argv.declaration_specifiers->declaration_specifiers.buffer[0]->type, AST_TYPE_SPECIFIER)
    CU_ASSERT_EQUAL_FATAL(argv.declaration_specifiers->declaration_specifiers.buffer[0]->type_specifier, TYPE_SPECIFIER_CHAR)
    CU_ASSERT_EQUAL_FATAL(argv.declarator->declarator.direct_declarator->direct_declarator.type, DECL_IDENTIFIER)
    CU_ASSERT_PTR_NOT_NULL_FATAL(argv.declarator->declarator.pointer)
    CU_ASSERT_EQUAL_FATAL(argv.declarator->declarator.pointer->type, AST_POINTER)
    CU_ASSERT_PTR_NOT_NULL_FATAL(argv.declarator->declarator.pointer->pointer.next_pointer)
    CU_ASSERT_EQUAL_FATAL(argv.declarator->declarator.pointer->pointer.next_pointer->type, AST_POINTER)
    CU_ASSERT_PTR_NULL_FATAL(argv.declarator->declarator.pointer->pointer.next_pointer->pointer.next_pointer) // only 2 levels of indirection

}

int parser_tests_init_suite() {
    CU_pSuite pSuite = CU_add_suite("parser", NULL, NULL);
    if (NULL == CU_add_test(pSuite, "primary expression - identifier", test_primary_expression_ident) ||
        NULL == CU_add_test(pSuite, "parameter type list", test_parameter_type_list) ||
        NULL == CU_add_test(pSuite, "declaration - simple", test_declaration_simple) ||
        NULL == CU_add_test(pSuite, "declaration - function prototype (no parameter list)", test_declaration_function_proto_no_params) ||
        NULL == CU_add_test(pSuite, "declaration - function prototype (with parameter list)", test_declaration_function_proto_with_params) ||
        NULL == CU_add_test(pSuite, "declaration - function prototype (with abstract parameter list)", test_declaration_function_proto_with_abstract_params) ||
        NULL == CU_add_test(pSuite, "function definition", test_function_definition) ||
        NULL == CU_add_test(pSuite, "function with parameter list", test_function_with_parameter_list)
    ) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    return 0;
}
