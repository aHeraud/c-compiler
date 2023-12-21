#ifndef C_COMPILER_PARSER_H
#define C_COMPILER_PARSER_H

#include <stdbool.h>
#include "ast.h"
#include "lexer.h"

typedef struct ParseError {
    const token_t *token;
    const token_t *previous_token;
    const char* production_name;
    const char* previous_production_name;
    enum ParseErrorType {
        PARSE_ERROR_EXPECTED_TOKEN,
        PARSE_ERROR_UNEXPECTED_END_OF_INPUT,
    } type;
    union {
        struct {
            size_t expected_count;
            token_kind_t expected[10];
        } expected_token;
        struct {
            token_kind_t expected;
        } unexpected_end_of_input;
    };
} parse_error_t;

void print_parse_error(FILE *__restrict stream, parse_error_t *error);
VEC_DEFINE(ParseErrorVector, parse_error_vector_t, parse_error_t)

/**
 * Contains the parser state.
 * The parser owns the lexer, and stores all of the tokens that have been scanned.
 * References to the tokens in the token vector are valid for the lifetime of the compilation process
 * (e.g. they have a static lifetime).
 *
 * Backtracking is implemented by storing the current token index, and restoring it when backtracking.
 */
typedef struct Parser {
    lexer_t lexer;
    token_ptr_vector_t tokens;
    /**
     * The index of the _next_ token (e.g. the last token consumed by the parser is at token_index - 1).
     */
    size_t next_token_index;
    parse_error_vector_t errors;
} parser_t;

parser_t pinit(lexer_t lexer);
bool parse(parser_t* parser, statement_t *node);

// Expressions
bool parse_primary_expression(parser_t *parser, expression_t *expr);
bool parse_postfix_expression(parser_t *parser, expression_t *expr);
bool parse_unary_expression(parser_t *parser, expression_t  *expr);
bool parse_cast_expression(parser_t *parser, expression_t *expr);
bool parse_multiplicative_expression(parser_t *parser, expression_t* expr);
bool parse_additive_expression(parser_t *parser, expression_t *expr);
bool parse_shift_expression(parser_t *parser, expression_t *expr);
bool parse_relational_expression(parser_t *parser, expression_t *expr);
bool parse_equality_expression(parser_t *parser, expression_t *expr);
bool parse_and_expression(parser_t *parser, expression_t *expr);
bool parse_exclusive_or_expression(parser_t *parser, expression_t *expr);
bool parse_inclusive_or_expression(parser_t *parser, expression_t *expr);
bool parse_logical_and_expression(parser_t *parser, expression_t *expr);
bool parse_logical_or_expression(parser_t *parser, expression_t *expr);
bool parse_conditional_expression(parser_t *parser, expression_t *expr);
bool parse_assignment_expression(parser_t *parser, expression_t *expr);
bool parse_expression(parser_t *parser, expression_t *expr);

//bool declaration(parser_t* parser, ast_node_t* node);
//bool declaration_specifiers(parser_t* parser, ast_node_t* node);
//bool init_declarator_list(parser_t* parser, ast_node_t* node);
//bool init_declarator(parser_t* parser, ast_node_t* node);
//bool storage_class_specifier(parser_t* parser, ast_node_t* node);
//bool type_specifier(parser_t* parser, ast_node_t* node);
//bool struct_or_union_specifier(parser_t* parser, ast_node_t* node);
//bool struct_or_union(parser_t* parser, ast_node_t* node);
//bool struct_declaration_list(parser_t* parser, ast_node_t* node);
//bool struct_declaration(parser_t* parser, ast_node_t* node);
//bool specifier_qualifier_list(parser_t* parser, ast_node_t* node);
//bool struct_declarator_list(parser_t* parser, ast_node_t* node);
//bool struct_declarator(parser_t* parser, ast_node_t* node);
//bool enum_specifier(parser_t* parser, ast_node_t* node);
//bool enumerator_list(parser_t* parser, ast_node_t* node);
//bool enumerator(parser_t* parser, ast_node_t* node);
//bool type_qualifier(parser_t* parser, ast_node_t* node);
//bool function_specifier(parser_t* parser, ast_node_t* node);
//bool declarator(parser_t* parser, ast_node_t* node);
//bool direct_declarator(parser_t* parser, ast_node_t* node);
//bool pointer(parser_t* parser, ast_node_t* node);
//bool type_qualifier_list(parser_t* parser, ast_node_t* node);
//bool parameter_type_list(parser_t* parser, ast_node_t* node);
//bool parameter_list(parser_t* parser, ast_node_t* node);
//bool parameter_declaration(parser_t* parser, ast_node_t* node);
//bool identifier_list(parser_t* parser, ast_node_t* node);
//bool type_name(parser_t* parser, ast_node_t* node);
//bool abstract_declarator(parser_t* parser, ast_node_t* node);
//bool direct_abstract_declarator(parser_t* parser, ast_node_t* node);
//bool initializer(parser_t* parser, ast_node_t* node);
//bool initializer_list(parser_t* parser, ast_node_t* node);
//bool designation(parser_t* parser, ast_node_t* node);
//bool designator_list(parser_t* parser, ast_node_t* node);
//bool designator(parser_t* parser, ast_node_t* node);

bool parse_statement(parser_t *parser, statement_t *statement);
//bool labeled_statement(parser_t* parser, ast_node_t* node);
bool parse_compound_statement(parser_t* parser, statement_t *node);
//bool block_item_list(parser_t* parser, ast_node_t* node);
//bool block_item(parser_t* parser, ast_node_t* node);
bool parse_expression_statement(parser_t *parser, statement_t *statement);
//bool selection_statement(parser_t* parser, ast_node_t* node);
//bool iteration_statement(parser_t* parser, ast_node_t* node);
//bool jump_statement(parser_t* parser, ast_node_t* node);

//bool translation_unit(parser_t* parser, ast_node_t* node);
//bool external_declaration(parser_t* parser, ast_node_t* node);
//bool function_definition(parser_t* parser, ast_node_t* node);
//bool declaration_list(parser_t* parser, ast_node_t* node);

#endif //C_COMPILER_PARSER_H
