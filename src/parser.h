#ifndef C_COMPILER_PARSER_H
#define C_COMPILER_PARSER_H

#include <stdbool.h>
#include "ast.h"
#include "lexer.h"

typedef struct ParseError {
    token_t* token;
    const char* production_name;
    enum ParseErrorType {
        UNEXPECTED_TOKEN,
        UNEXPECTED_END_OF_INPUT,
    } type;
    union {
        struct {
            token_kind_t expected;
            token_kind_t actual;
        } unexpected_token;
        struct {
            token_kind_t expected;
        } unexpected_end_of_input;
    };
} parse_error_t;

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
    token_vector_t tokens;
    /**
     * The index of the _next_ token (e.g. the last token consumed by the parser is at token_index - 1).
     */
    size_t next_token_index;
    parse_error_vector_t errors;
} parser_t;

parser_t pinit(lexer_t lexer);
bool parse(parser_t* parser, ast_node_t* ast);

bool primary_expression(parser_t* parser, ast_node_t* node);
bool postfix_expression(parser_t* parser, ast_node_t* node);
bool argument_expression_list(parser_t* parser, ast_node_t* node);
bool unary_expression(parser_t* parser, ast_node_t* node);
bool unary_operator(parser_t* parser, ast_node_t* node);
bool cast_expression(parser_t* parser, ast_node_t* node);
bool multiplicative_expression(parser_t* parser, ast_node_t* node);
bool additive_expression(parser_t* parser, ast_node_t* node);
bool shift_expression(parser_t* parser, ast_node_t* node);
bool relational_expression(parser_t* parser, ast_node_t* node);
bool equality_expression(parser_t* parser, ast_node_t* node);
bool and_expression(parser_t* parser, ast_node_t* node);
bool exclusive_or_expression(parser_t* parser, ast_node_t* node);
bool inclusive_or_expression(parser_t* parser, ast_node_t* node);
bool logical_and_expression(parser_t* parser, ast_node_t* node);
bool logical_or_expression(parser_t* parser, ast_node_t* node);
bool conditional_expression(parser_t* parser, ast_node_t* node);
bool assignment_expression(parser_t* parser, ast_node_t* node);
bool assignment_operator(parser_t* parser, ast_node_t* node);
bool expression(parser_t* parser, ast_node_t* node);

bool declaration(parser_t* parser, ast_node_t* node);
bool declaration_specifiers(parser_t* parser, ast_node_t* node);
bool init_declarator_list(parser_t* parser, ast_node_t* node);
bool init_declarator(parser_t* parser, ast_node_t* node);
bool storage_class_specifier(parser_t* parser, ast_node_t* node);
bool type_specifier(parser_t* parser, ast_node_t* node);
bool struct_or_union_specifier(parser_t* parser, ast_node_t* node);
bool struct_or_union(parser_t* parser, ast_node_t* node);
bool struct_declaration_list(parser_t* parser, ast_node_t* node);
bool struct_declaration(parser_t* parser, ast_node_t* node);
bool specifier_qualifier_list(parser_t* parser, ast_node_t* node);
bool struct_declarator_list(parser_t* parser, ast_node_t* node);
bool struct_declarator(parser_t* parser, ast_node_t* node);
bool enum_specifier(parser_t* parser, ast_node_t* node);
bool enumerator_list(parser_t* parser, ast_node_t* node);
bool enumerator(parser_t* parser, ast_node_t* node);
bool type_qualifier(parser_t* parser, ast_node_t* node);
bool function_specifier(parser_t* parser, ast_node_t* node);
bool declarator(parser_t* parser, ast_node_t* node);
bool direct_declarator(parser_t* parser, ast_node_t* node);
bool pointer(parser_t* parser, ast_node_t* node);
bool type_qualifier_list(parser_t* parser, ast_node_t* node);
bool parameter_type_list(parser_t* parser, ast_node_t* node);
bool parameter_list(parser_t* parser, ast_node_t* node);
bool parameter_declaration(parser_t* parser, ast_node_t* node);
bool identifier_list(parser_t* parser, ast_node_t* node);
bool type_name(parser_t* parser, ast_node_t* node);
bool abstract_declarator(parser_t* parser, ast_node_t* node);
bool direct_abstract_declarator(parser_t* parser, ast_node_t* node);
bool initializer(parser_t* parser, ast_node_t* node);
bool initializer_list(parser_t* parser, ast_node_t* node);
bool designation(parser_t* parser, ast_node_t* node);
bool designator_list(parser_t* parser, ast_node_t* node);
bool designator(parser_t* parser, ast_node_t* node);

bool statement(parser_t* parser, ast_node_t* node);
bool labeled_statement(parser_t* parser, ast_node_t* node);
bool compound_statement(parser_t* parser, ast_node_t* node);
bool block_item_list(parser_t* parser, ast_node_t* node);
bool block_item(parser_t* parser, ast_node_t* node);
bool expression_statement(parser_t* parser, ast_node_t* node);
bool selection_statement(parser_t* parser, ast_node_t* node);
bool iteration_statement(parser_t* parser, ast_node_t* node);
bool jump_statement(parser_t* parser, ast_node_t* node);

bool translation_unit(parser_t* parser, ast_node_t* node);
bool external_declaration(parser_t* parser, ast_node_t* node);
bool function_definition(parser_t* parser, ast_node_t* node);
bool declaration_list(parser_t* parser, ast_node_t* node);

#endif //C_COMPILER_PARSER_H
