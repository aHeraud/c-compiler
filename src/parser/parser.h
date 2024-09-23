#ifndef C_COMPILER_PARSER_H
#define C_COMPILER_PARSER_H

#include <stdbool.h>
#include "ast.h"
#include "parser/lexer.h"

typedef struct ParseError {
    const token_t *token;
    // Generally, the token previously consumer by the parser.
    // In the case of PARSE_ERROR_ILLEGAL_DECLARATION_SPECIFIERS, this is the previous conflicting declaration specifier.
    const token_t *previous_token;
    const char* production_name;
    const char* previous_production_name;
    enum ParseErrorType {
        PARSE_ERROR_EXPECTED_TOKEN,
        PARSE_ERROR_UNEXPECTED_END_OF_INPUT,
        PARSE_ERROR_ILLEGAL_DECLARATION_SPECIFIERS,
        PARSE_ERROR_TYPE_SPECIFIER_MISSING,
        PARSE_ERROR_ILLEGAL_USE_OF_RESTRICT,
        PARSE_ERROR_EXPECTED_EXPRESSION_OR_TYPE_NAME_AFTER_SIZEOF,
        PARSE_ERROR_PARAMETER_TYPE_MALFORMED,
        PARSE_ERROR_EXPECTED_EXPRESSION,
        PARSE_ERROR_REDECLARATION_OF_SYMBOL_AS_DIFFERENT_TYPE,
    } kind;
    union {
        struct {
            size_t expected_count;
            token_kind_t expected[10];
        } expected_token;
        struct {
            token_kind_t expected;
        } unexpected_end_of_input;
        struct {
            const token_t *prev;
            const token_t *redec;
        } redeclaration_of_symbol;
    } value;
} parse_error_t;

void print_parse_error(FILE *__restrict stream, parse_error_t *error);
VEC_DEFINE(ParseErrorVector, parse_error_vector_t, parse_error_t)

typedef struct ParserSymbolTable* parser_symbol_table_t;

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
    parser_symbol_table_t symbol_table;
} parser_t;

parser_t pinit(lexer_t lexer);
bool parse(parser_t* parser, translation_unit_t *translation_unit);

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

bool parse_declaration(parser_t *parser, ptr_vector_t *declarations);
bool parse_declaration_specifiers(parser_t *parser, type_t *type);
bool parse_specifier_qualifier_list(parser_t *parser, type_t *type);
bool parse_struct_or_union_specifier(parser_t *parser, token_t **keyword, struct_t *struct_type);
bool parse_init_declarator(parser_t *parser, type_t base_type, declaration_t *decl);
bool parse_initializer(parser_t *parser, initializer_t *initializer);
bool parse_initializer_list(parser_t *parser, initializer_list_t *list);
bool parse_designation(parser_t *parser, designator_list_t *list);
bool parse_declarator(parser_t *parser, type_t base_type, declaration_t *declaration);
bool parse_direct_declarator(parser_t *parser, token_t **identifier_out, ptr_vector_t *left, ptr_vector_t *right);
bool parse_pointer(parser_t *parser, const type_t *base_type, type_t **pointer_type);
bool parse_parameter_type_list(parser_t *parser, parameter_type_list_t *parameters);
bool parse_type_name(parser_t *parser, type_t **type_out);
bool parse_abstract_declarator(parser_t *parser, type_t base_type, type_t **type_out);

bool parse_statement(parser_t *parser, statement_t *statement);
bool parse_compound_statement(parser_t* parser, statement_t *statement, const token_t *open);
bool parse_expression_statement(parser_t *parser, statement_t *statement);
bool parse_while_statement(parser_t *parser, statement_t *statement, token_t *keyword);
bool parse_do_while_statement(parser_t *parser, statement_t *statement);
bool parse_for_statement(parser_t* parser, statement_t *statement, token_t *keyword);
bool parse_break_statement(parser_t *parser, statement_t *statement);
bool parse_continue_statement(parser_t *parser, statement_t *statement);
bool parse_goto_statement(parser_t *parser, statement_t *statement);
bool parse_labeled_statement(parser_t *parser, statement_t *statement);
bool parse_case_statement(parser_t *parser, statement_t *statement);
bool parse_default_case_statement(parser_t *parser, statement_t *statement);
bool parse_switch_statement(parser_t *parser, statement_t *statement);
bool parse_if_statement(parser_t* parser, statement_t *statement, token_t *keyword);
bool parse_return_statement(parser_t* parser, statement_t *statement, token_t *keyword);

bool parse_external_declaration(parser_t *parser, external_declaration_t *external_declaration);
bool parse_function_definition(parser_t *parser, const declaration_t *declarator, const token_t* body_start, function_definition_t *fn);

#endif //C_COMPILER_PARSER_H
