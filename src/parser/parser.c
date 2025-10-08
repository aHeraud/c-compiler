// Recursive descent parser for the C language, based on the reference c99 grammar: see docs/c99.bnf
// Currently intentionally leaks memory for the purpose of simplicity, will likely be fixed later (memory arena?).
//
// The grammar has been modified to remove left-recursion to enable parsing via recursive descent (see docs/c99.bnf) for
// the reference grammar. However, the modified grammar is not LL(k), as there are some ambiguities that require an
// infinite lookahead to resolve.
// These are:
// 1. When parsing a <unary-expression> starting with 'sizeof', it is not possible predict whether to parse
//    'sizeof' <unary-expression> or 'sizeof' '(' <type-name> ')', as both a <unary-expression> and a <type-name> can
//    be prefixed with an arbitrary number of '(' tokens.
// 2. When parsing a <parameter-declarator> it is not possible to determine whether to parse a <declarator> or an
//    abstract declarator without an infinite lookahead, as both can be prefixed with an arbitrary number of '*' and
//    '(' tokens.
// To resolve these ambiguities, we use a simple backtracking mechanism to try all possible parses and backtrack if
// a parse fails.
//
// The C grammar is not a context free grammar, as the parser needs to be able to differentiate identifiers versus
// typedef names.
// Consider the following example expression: `(a)*b`. What the parser should generate for this statement is dependent
// on what `a` is. If `a` is a typedef name, then this is a dereference and cast, otherwise it is multiplication.
// Another example is the statement `a * b`. If a is an identifier, than this is an expression statement, but if a is
// a typedef name, then it's a declaration.
// To keep track, the parser will have a simplified symbol table, which keeps track of lexical scopes, identifiers and
// typedefs. Why do we need to keep track of both identifiers and typedefs? Because a typedef or identifier in an inner
// scope can hide a typedef or identifier declared in an enclosing scope, so when looking up a symbol to check if it is
// a typedef, we need to know if there's a different definition of the symbol in a closer scope.
// For example:
// ```
// typedef int value;
// int square(int value) {
//     return value * value; // <-- This is ok, in this context value refers to an identifier
//                           //     If we didn't keep track of identifiers in the parser's symbol table, we would just
//                           //     see that the enclosing scope had a typedef named "value", and would not be able to
//                           //     correctly parse this.
// }
// ```
//
// Restoring the parser's symbol table when backtracking:
// Each symbol and scope in the symbol table store the value of the index of the next token at the time they were
// created. The parser checkpoints also include this value. When restoring from a checkpoint, any scope or symbol which
// have a `next_token_index` value larger than the value in the checkpoint should be removed from the symbol table.

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "errors.h"
#include "parser.h"

#include <string.h>

#include "parser/lexer.h"

void append_parse_error(parse_error_vector_t* vec, parse_error_t error) {
    VEC_APPEND(vec, error);
}

typedef struct ParserSymbol
{
    enum {
        SYMBOL_IDENTIFIER,
        SYMBOL_TYPEDEF,
    } kind;
    token_t *token;
    // if this is a typedef, the type
    type_t *type;
    // Index of the next token when this symbol was created
    // For restoring the state of the parse table when backtracking
    int next_token_index;
} parser_symbol_t;

typedef struct ParserScope parser_scope_t;
struct ParserScope {
    parser_scope_t *parent;
    // Map of symbol name -> symbol
    hash_table_t symbols_map;
    // A list of symbols in the symbol table.
    // Used for rolling back the symbol table state when backtracking.
    ptr_vector_t symbols_vec;
    // Index of the next token when this symbol was created
    // For restoring the state of the parse table when backtracking
    int next_token_index;
};

struct ParserSymbolTable {
    parser_scope_t *root_scope;
    parser_scope_t *current_scope;
};

void parser_enter_scope(parser_t *parser) {
    parser_scope_t *scope = malloc(sizeof (parser_scope_t));
    *scope = (parser_scope_t) {
        .parent = parser->symbol_table->current_scope,
        .symbols_map = hash_table_create_string_keys(64),
    };
    parser->symbol_table->current_scope = scope;
}

void parser_leave_scope(parser_t *parser) {
    assert(parser->symbol_table->current_scope != parser->symbol_table->root_scope);
    assert(parser->symbol_table->current_scope != NULL);
    parser_scope_t *scope = parser->symbol_table->current_scope;
    parser->symbol_table->current_scope = scope->parent;
    free(scope);
}

void parser_insert_symbol(parser_t *parser, const parser_symbol_t *symbol) {
    parser_symbol_t *prev = NULL;
    if (hash_table_lookup(&parser->symbol_table->current_scope->symbols_map, symbol->token->value, (void**) &prev) &&
        prev->kind != symbol->kind) {
        append_parse_error(&parser->errors, (parse_error_t) {
            .previous_production_name = NULL,
            .previous_token = NULL,
            .production_name = NULL,
            .token = symbol->token,
            .kind = PARSE_ERROR_REDECLARATION_OF_SYMBOL_AS_DIFFERENT_TYPE,
            .value.redeclaration_of_symbol = {
                .prev = prev->token,
                .redec = symbol->token,
            }
        });
        // don't replace the existing value
        return;
    }

    hash_table_insert(&parser->symbol_table->current_scope->symbols_map, symbol->token->value, (void*) symbol);
    VEC_APPEND(&parser->symbol_table->current_scope->symbols_vec, symbol);
}

void parser_insert_symbol_for_declaration(parser_t *parser, const declaration_t *decl) {
    bool is_typedef = decl->type->storage_class == STORAGE_CLASS_TYPEDEF;
    if (is_typedef && decl->initializer != NULL) {
        // TODO: error for illegal initializer
    }

    // If this doesn't declare anything, don't create a symbol
    if (decl->identifier == NULL) return;

    parser_symbol_t *symbol = malloc(sizeof(parser_symbol_t));
    *symbol = (parser_symbol_t) {
        .kind = is_typedef ? SYMBOL_TYPEDEF : SYMBOL_IDENTIFIER,
        .next_token_index = parser->next_token_index,
        .token = decl->identifier,
        .type = is_typedef ? decl->type : NULL,
    };
    parser_insert_symbol(parser, symbol);
}

parser_symbol_t *parser_lookup_symbol_in_current_scope(const parser_t *parser, const char *name) {
    const parser_scope_t *scope = parser->symbol_table->current_scope;
    parser_symbol_t *symbol;
    hash_table_lookup(&scope->symbols_map, name, (void**) &symbol);
    return symbol;
}

parser_symbol_t *parser_lookup_symbol(parser_t *parser, const char *name) {
    const parser_scope_t *scope = parser->symbol_table->current_scope;
    while (scope != NULL) {
        parser_symbol_t *symbol = NULL;
        hash_table_lookup(&scope->symbols_map, name, (void**) &symbol);
        if (symbol != NULL) return symbol;
        scope = scope->parent;
    }
    return NULL;
}

bool any_non_null(size_t count, ...) {
    va_list args;
    va_start(args, count);
    for (size_t i = 0; i < count; i++) {
        if (va_arg(args, void *) != NULL) {
            va_end(args);
            return true;
        }
    }
    va_end(args);
    return false;
}

void* first_non_null(size_t count, ...) {
    va_list args;
    va_start(args, count);
    for (size_t i = 0; i < count; i++) {
        void *arg = va_arg(args, void *);
        if (arg != NULL) {
            va_end(args);
            return arg;
        }
    }
    va_end(args);
    return NULL;
}

bool any_token_kind_equals(token_kind_t kind, size_t count, ...) {
    va_list args;
    va_start(args, count);
    for (size_t i = 0; i < count; i++) {
        token_kind_t arg = va_arg(args, token_kind_t);
        if (arg == kind) {
            va_end(args);
            return true;
        }
    }
    va_end(args);
    return false;
}

#define FIRST_NON_NULL(...) first_non_null(sizeof((void*[]){__VA_ARGS__}) / sizeof(void*), __VA_ARGS__)
#define ANY_NON_NULL(...) any_non_null(sizeof((void*[]){__VA_ARGS__}) / sizeof(void*), __VA_ARGS__)
#define TOKEN_KIND_ONE_OF(value, ...) any_token_kind_equals(value, sizeof((token_kind_t[]){__VA_ARGS__}) / sizeof(token_kind_t), __VA_ARGS__)

const token_kind_t DECLARATION_SPECIFIER_TOKENS[] = {
    TK_TYPEDEF, TK_EXTERN, TK_STATIC, TK_AUTO, TK_REGISTER, // storage-class-specifier
    TK_VOID, TK_CHAR, TK_SHORT, TK_INT, TK_LONG, TK_FLOAT, TK_DOUBLE, TK_SIGNED, TK_UNSIGNED, TK_BOOL, TK_COMPLEX, TK_STRUCT, TK_UNION, TK_ENUM, // type-specifiers
    TK_CONST, TK_RESTRICT, TK_VOLATILE, // type-qualifiers
    TK_INLINE, // function-specifier
};

void print_parse_error(FILE *__restrict stream, parse_error_t *error) {
    source_position_t position = error->token->position;
    fprintf(stream, "%s:%d:%d: error: ", position.path, position.line, position.column);
    switch (error->kind) {
        case PARSE_ERROR_EXPECTED_TOKEN:
            fprintf(stream, error->value.expected_token.expected_count > 1 ? "expected one of " : "expected ");
            for (size_t i = 0; i < error->value.expected_token.expected_count; i++) {
                fprintf(stream, "%s", token_kind_display_names[error->value.expected_token.expected[i]]);
                if (i < error->value.expected_token.expected_count - 1) {
                    fprintf(stream, ", ");
                } else if (i == error->value.expected_token.expected_count - 2) {
                    fprintf(stream, " or ");
                }
            }

            if (error->previous_production_name != NULL ) {
                fprintf(stream, " after %s\n", error->previous_production_name);
            }
            break;
        case PARSE_ERROR_UNEXPECTED_END_OF_INPUT:
            fprintf(stream, "Unexpected end of input\n");
            fprintf(stream, "Expected token: %s\n", token_kind_display_names[error->value.unexpected_end_of_input.expected]);
            break;
        case PARSE_ERROR_ILLEGAL_USE_OF_RESTRICT:
            fprintf(stream, "Illegal use of restrict (requires pointer or reference)\n");
            break;
        case PARSE_ERROR_ILLEGAL_DECLARATION_SPECIFIERS:
            if (error->previous_token != NULL) {
                fprintf(stream, "Cannot combine %s with previous specifier %s\n",
                        error->token->value, error->previous_token->value);
            } else {
                fprintf(stream, "Illegal declaration specifiers\n");
            }
            break;
        case PARSE_ERROR_TYPE_SPECIFIER_MISSING:
            fprintf(stream, "Type specifier missing\n");
            break;
        case PARSE_ERROR_EXPECTED_EXPRESSION_OR_TYPE_NAME_AFTER_SIZEOF:
            fprintf(stream, "Expected expression or `(` type-name `)` after 'sizeof'\n");
            break;
        case PARSE_ERROR_PARAMETER_TYPE_MALFORMED:
            fprintf(stream, "Expected a declarator, comma, closing parenthesis, or ellipsis after type\n");
            break;
        case PARSE_ERROR_EXPECTED_EXPRESSION:
            fprintf(stream, "Expected an expression\n");
            break;
        case PARSE_ERROR_REDECLARATION_OF_SYMBOL_AS_DIFFERENT_TYPE:
            fprintf(stream, "redeclaration of symbol %s as different type", error->value.redeclaration_of_symbol.redec->value);
            break;
        case PARSE_ERROR_ENUM_SPECIFIER_WITHOUT_IDENTIFIER_OR_ENUMERATOR_LIST:
            fprintf(stream, "%s:%d:%d error: enum specifier must be followed by an identifier or an enumerator list\n",
                    error->token->position.path, error->token->position.line, error->token->position.column);
            break;
    }
}

source_span_t spanning_tokens(token_t *start, token_t *end) {
    return (source_span_t) {
            .start = start->position,
            .end = end->position,
    };
}

source_span_t span_starting(source_position_t start, token_t *end) {
    return (source_span_t) {
            .start = start,
            .end = end->position,
    };
}

source_span_t spanning(source_position_t start, source_position_t end) {
    return (source_span_t) {
            .start = start,
            .end = end,
    };
}

#define SPANNING_NEXT(start) spanning_tokens(start, next_token(parser))
#define SPAN_STARTING(start) span_starting(start, next_token(parser))

typedef struct ParseCheckpoint {
    size_t token_index;
    size_t error_index;
} parse_checkpoint_t;

parser_t pinit(lexer_t lexer) {
    parser_symbol_table_t symbol_table = malloc(sizeof(struct ParserSymbolTable));
    symbol_table->root_scope = malloc(sizeof(parser_scope_t));
    *symbol_table->root_scope = (parser_scope_t) {
        .parent = NULL,
        .symbols_map = hash_table_create_string_keys(64),
    };
    symbol_table->current_scope = symbol_table->root_scope;
    return (parser_t) {
            .lexer = lexer,
            .tokens = {.size = 0, .capacity = 0, .buffer = NULL},
            .errors = {.size = 0, .capacity = 0, .buffer = NULL},
            .next_token_index = 0,
            .symbol_table = symbol_table,
            .id_counter = 1,
    };
}

void recover(parser_t *parser);

/**
 * Creates a checkpoint at the current parser state. Later, the parser can be restored to this state using
 * the backtrack function.
 * @param parser Parser instance
 * @return Checkpoint
 */
parse_checkpoint_t create_checkpoint(const parser_t* parser) {
    return (parse_checkpoint_t) {
        .token_index = parser->next_token_index,
        .error_index = parser->errors.size,
    };
}

/**
 * Restores the parser to a previously saved state.
 * @param parser Parser instance
 * @param checkpoint Checkpoint to restore to
 */
void backtrack(parser_t* parser, parse_checkpoint_t checkpoint) {
    parser->next_token_index = checkpoint.token_index;
    parser->errors.size = checkpoint.error_index;

    // Restore the symbol table state
    // First, leave any scopes that were entered after the checkpoint was created.
    while(parser->symbol_table->current_scope != parser->symbol_table->root_scope &&
        parser->symbol_table->current_scope->next_token_index > checkpoint.token_index) {
        parser_leave_scope(parser);
    }
    // Next, remove any symbols that were added to the now current scope after the checkpoint was created.
    parser_scope_t *scope = parser->symbol_table->current_scope;
    for (int i = scope->symbols_vec.size; i > 0; i -= 1) {
        parser_symbol_t *symbol = scope->symbols_vec.buffer[i-1];
        if (symbol->next_token_index <= checkpoint.token_index) break;
        hash_table_remove(&scope->symbols_map, symbol->token->value, NULL);
        scope->symbols_vec.size -= 1;
    }
}

token_t *next_token(parser_t* parser) {
    token_t* token;
    if (parser->next_token_index < parser->tokens.size) {
        token = parser->tokens.buffer[parser->next_token_index];
    } else {
        token_t *next = malloc(sizeof(token_t));
        *next = lscan(&parser->lexer);
        append_token_ptr(&parser->tokens.buffer, &parser->tokens.size, &parser->tokens.capacity, next);
        token = parser->tokens.buffer[parser->tokens.size - 1];
    }
    return token;
}

source_position_t* current_position(parser_t* parser) {
    return &next_token(parser)->position;
}

bool peek(parser_t* parser, token_kind_t kind) {
    return next_token(parser)->kind == kind;
}

bool peek2(parser_t *parser, token_kind_t kind) {
    parse_checkpoint_t checkpoint  = create_checkpoint(parser);
    parser->next_token_index += 1;
    bool result = peek(parser, kind);
    backtrack(parser, checkpoint);
    return result;
}

bool accept(parser_t* parser, token_kind_t kind, token_t** token_out) {
    token_t* token = next_token(parser);
    bool eof = token->kind == TK_EOF;
    if (token->kind == kind) {
        if (!eof) parser->next_token_index++;
        if (token_out != NULL) *token_out = token;
        return true;
    } else {
        return false;
    }
}

bool require(parser_t* parser, token_kind_t kind, token_t** token_out, const char* production_name, const char* previous_production_name) {
    if (accept(parser, kind, token_out)) {
        return true;
    } else {
        token_t* previous_token = parser->next_token_index > 0 ? parser->tokens.buffer[parser->next_token_index - 1] : NULL;
        token_t* token = next_token(parser);
        parse_error_t error;
        if (token->kind == TK_EOF) {
            error = (parse_error_t) {
                .token = token,
                .previous_token = previous_token,
                .production_name = production_name,
                .previous_production_name = previous_production_name,
                .kind = PARSE_ERROR_UNEXPECTED_END_OF_INPUT,
                .value.unexpected_end_of_input = {
                    .expected = kind,
                },
            };
        } else {
            error = (parse_error_t) {
                .token = token,
                .previous_token = previous_token,
                .production_name = production_name,
                .previous_production_name = previous_production_name,
                .kind = PARSE_ERROR_EXPECTED_TOKEN,
                .value.expected_token = {
                    .expected_count = 1,
                    .expected = {kind},
                },
            };
        }
        append_parse_error(&parser->errors, error);
        return false;
    }
}

/**
 * Recovers from a parse error by skipping tokens until a semicolon is found.
 * @param parser Parser instance
 */
void recover(parser_t *parser) {
    token_t *token = next_token(parser);
    while (token->kind != TK_EOF) {
        parser->next_token_index++;
        if (token->kind == TK_SEMICOLON) {
            break;
        }
        token = next_token(parser);
    }
}

bool parse(parser_t* parser, translation_unit_t *translation_unit) {
    ptr_vector_t external_declarations = {.buffer = NULL, .size = 0, .capacity = 0};

    while (next_token(parser)->kind != TK_EOF) {
        external_declaration_t *external_declaration = malloc(sizeof(external_declaration_t));
        if (!parse_external_declaration(parser, external_declaration)) {
            free(external_declaration);
            // discard tokens until we get to the next semicolon
            recover(parser);
        } else {
            append_ptr(&external_declarations.buffer, &external_declarations.size, &external_declarations.capacity, external_declaration);
        }
    }

    *translation_unit = (translation_unit_t) {
        .external_declarations = (external_declaration_t**) external_declarations.buffer,
        .length = external_declarations.size,
    };

    return parser->errors.size == 0;
}

// Declarations

bool typedef_name(parser_t *parser, bool _peek, token_t **token_out, type_t **type_out) {
    token_t *identifier = NULL;
    if (!peek(parser, TK_IDENTIFIER)) return false;

    identifier = next_token(parser);

    parser_symbol_t *symbol = parser_lookup_symbol(parser, identifier->value);
    if (symbol == NULL) return false;

    if (symbol->kind == SYMBOL_TYPEDEF) {
        if (token_out != NULL) *token_out = identifier;
        if (type_out != NULL) *type_out = symbol->type;
        if (!_peek) accept(parser, TK_IDENTIFIER, NULL); // consume the token
        return true;
    }

    return false;
}

/**
 * Parses a declaration.
 *
 * <declaration> ::= <declaration-specifiers> <init-declarator-list>? ';'
 *
 * <init-declarator-list> ::= <init-declarator> | <init-declarator-list> ',' <init-declarator>
 *
 * If this is called while parsing an external declaration, we have already parsed the declaration specifiers and the
 * first declarator of the init-declarator-list, so we pass them in as parameters.
 *
 * @param parser Parser instance
 * @param first_declarator First declarator of the init-declarator-list which has already been parsed,
 *                         or NULL if this is not an external declaration.
 * @param declarations Vector to append the parsed declarations to
 * @return true if the declaration was parsed successfully, false otherwise
 */
bool _parse_declaration(parser_t *parser, declaration_t *first_declarator, ptr_vector_t *declarations) {
    type_t type;
    if (first_declarator == NULL) {
        parse_declaration_specifiers(parser, &type); // always succeeds
    } else {
        type = *first_declarator->type;
    }

    if (first_declarator == NULL && accept(parser, TK_SEMICOLON, NULL)) {
        // This is a declaration without an identifier, e.g. "int;", or "typedef float;".
        // This is legal, but useless (unless it declares a struct/union or enum type).
        // TODO: warning for empty declaration
        if (type.kind == TYPE_STRUCT_OR_UNION || type.kind == TYPE_ENUM) {
            type_t *type_heap = malloc(sizeof(type_t));
            *type_heap = type;
            declaration_t *declaration = malloc(sizeof(declaration_t));
            *declaration = (declaration_t) {
                .type = type_heap,
                .identifier = NULL,
                .initializer = NULL
            };
            VEC_APPEND(declarations, declaration);
        }
        return true;
    }

    // If we didn't find a semicolon, then we need to attempt to parse an <init-declarator-list>.
    if (first_declarator != NULL) {
        // We've already parsed a declarator.
        // Check if we still need to parse an initializer, then parse the rest of the init-declarator-list.
        if (accept(parser, TK_ASSIGN, NULL)) {
            initializer_t *initializer = malloc(sizeof(initializer_t));
            if (!parse_initializer(parser, initializer)) {
                free(initializer);
                return false;
            }
            first_declarator->initializer = initializer;
        }
        append_ptr(&declarations->buffer, &declarations->size, &declarations->capacity, first_declarator);

        if (!accept(parser, TK_COMMA, NULL)) {
            parser_insert_symbol_for_declaration(parser, first_declarator);
            return require(parser, TK_SEMICOLON, NULL, "declaration", NULL);
        }
    }

    // Parse the init-declarator-list (or the remaining part of it).
    do {
        declaration_t *decl = malloc(sizeof(declaration_t));
        if (!parse_init_declarator(parser, type, decl)) {
            free(decl);
            return false;
        }
        append_ptr(&declarations->buffer, &declarations->size, &declarations->capacity, decl);
    } while (accept(parser, TK_COMMA, NULL));

    // Update the symbol table
    for (int i = 0; i < declarations->size; i += 1) {
        declaration_t *decl = declarations->buffer[i];
        parser_insert_symbol_for_declaration(parser, decl);
    }

    return require(parser, TK_SEMICOLON, NULL, "declaration", NULL);
}

bool parse_declaration(parser_t *parser, ptr_vector_t *declarations) {
    return _parse_declaration(parser, NULL, declarations);
}

parse_error_t illegal_declaration_specifiers(token_t *token, token_t *prev) {
    return (parse_error_t) {
            .token = token,
            .previous_token = prev,
            .production_name = "declaration-specifiers",
            .previous_production_name = NULL,
            .kind = PARSE_ERROR_ILLEGAL_DECLARATION_SPECIFIERS,
    };
}

/**
 * Parses either a specifier-qualifier-list or declaration-specifiers.
 *
 * * <declaration-specifiers> ::= <storage-class-specifier> <declaration-specifiers>
 *                              | <type-specifier> <declaration-specifiers>
 *                              | <type-qualifier> <declaration-specifiers>
 *                              | <function-specifier> <declaration-specifiers>
 *
 *  <specifier-qualifier-list> ::= <type-specifier> <specifier-qualifier-list>?
 *                               | <type-qualifier> <specifier-qualifier-list>?
 *
 * <storage-class-specifier> ::= 'typedef' | 'extern' | 'static' | 'auto' | 'register'
 *
 * <type-specifier> ::= 'void' | 'char' | 'short' | 'int' | 'long' | 'float' | 'double' | 'signed' | 'unsigned' | '_Bool' | '_Complex'
 *                    | <struct-or-union-specifier>
 *                    | <enum-specifier>
 *                    | <typedef-name>
 *
 * <type-qualifier> ::= 'const' | 'restrict' | 'volatile'
 *
 * <function-specifier> ::= 'inline'
 *
 * TODO: The parsing of enums is not yet implemented.
 * TODO: Inling is not yet implemented, and the keyword will be silently ignored.
 *
 * Only one storage-class-specifier may be present in a declaration specifiers list.
 *
 * Valid combinations of type specifiers:
 * - void
 * - char
 * - signed char
 * - unsigned char
 * - short, signed short, short int, signed short int
 * - unsigned short, unsigned short int
 * - int, signed, signed int
 * - unsigned, unsigned int
 * - long, signed long, long int, signed long int
 * - unsigned long, unsigned long int
 * - long long, signed long long, long long int, signed long long int
 * - unsigned long long, unsigned long long int
 * - float
 * - double
 * - long double
 * - _Bool
 * - float _Complex
 * - double _Complex
 * - long double _Complex
 * - struct-or-union-specifier
 * - enum-specifier
 * - typedef-name
 *
 * @param parser
 * @param is_declaration true if this is a declaration, false if this is a type specifier
 *                       if false, only type-specifiers and type-qualifiers are allowed
 * @param type Out parameter for the type that the specifiers represent
 * @return
 */
bool parse_specifiers(parser_t *parser, bool is_declaration, type_t *type) {
    const token_t *storage_class =NULL;
    bool is_const = false;
    bool is_volatile = false;
    bool is_inline = false;
    token_t *void_ = NULL;
    token_t *bool_ = NULL;
    token_t *char_ = NULL;
    token_t *short_ = NULL;
    token_t *int_ = NULL;
    token_t *long_ = NULL;
    token_t *long_long = NULL;
    token_t *float_ = NULL;
    token_t *double_ = NULL;
    token_t *signed_ = NULL;
    token_t *unsigned_ = NULL;
    token_t *complex_ = NULL;
    token_t *struct_or_union = NULL;
    token_t *enum_ = NULL;

    token_t *typedef_name_token = NULL;
    type_t *typedef_type = NULL;

    struct_t *struct_type = NULL;
    enum_specifier_t *enum_specifier = NULL;

    while (true) {
        token_t *token;
        if (is_declaration && (accept(parser, TK_TYPEDEF, &token) ||
            accept(parser, TK_EXTERN, &token) ||
            accept(parser, TK_STATIC, &token) ||
            accept(parser, TK_AUTO, &token) ||
            accept(parser, TK_REGISTER, &token))) {
            // storage-class-specifier

            if (storage_class != NULL) {
                // TODO: warn about duplicate specifiers
                if (token->kind != storage_class->kind) {
                    append_parse_error(&parser->errors, (parse_error_t) {
                            .token = token,
                            .previous_token = storage_class,
                            .production_name = "storage-class-specifier",
                            .previous_production_name = "storage-class-specifier",
                            .kind = PARSE_ERROR_ILLEGAL_DECLARATION_SPECIFIERS,
                    });
                }
            } else {
                storage_class = token;
            }
        } else if (is_declaration && accept(parser, TK_INLINE, &token)) {
            // TODO: inline
        } else if (accept(parser, TK_CONST, &token)) {
            is_const = true;
        } else if (accept(parser, TK_RESTRICT, &token)) {
            // It is illegal to use restrict in this context.
            append_parse_error(&parser->errors, (parse_error_t) {
                    .token = token,
                    .previous_token = NULL,
                    .production_name = "declaration-specifiers",
                    .previous_production_name = NULL,
                    .kind = PARSE_ERROR_ILLEGAL_USE_OF_RESTRICT,
            });
        } else if (accept(parser, TK_VOLATILE, &token)) {
            is_volatile = true;
        } else if (typedef_name(parser, false, &token, &typedef_type)) {
            token_t *conflict = (token_t *) FIRST_NON_NULL(void_, bool_, char_, short_, int_, long_, long_long, float_,
                                                           double_, signed_, unsigned_, complex_, struct_or_union, enum_,
                                                           typedef_name_token);
            if (conflict != NULL) {
                append_parse_error(&parser->errors, illegal_declaration_specifiers(token, conflict));
            } else {
                typedef_name_token = token;
            }
        } else if (accept(parser, TK_VOID, &token)) {
            token_t *conflict = (token_t *) FIRST_NON_NULL(void_, bool_, char_, short_, int_, long_, long_long, float_,
                                                           double_, signed_, unsigned_, complex_, struct_or_union, enum_,
                                                           typedef_name_token);
            if (conflict != NULL) {
                append_parse_error(&parser->errors, illegal_declaration_specifiers(token, conflict));
            } else {
                void_ = token;
            }
        } else if (accept(parser, TK_CHAR, &token)) {
            token_t *conflict = (token_t *) FIRST_NON_NULL(void_, bool_, char_, short_, int_, long_, long_long, float_,
                                                           double_, complex_, struct_or_union, enum_, typedef_name_token);
            if (conflict != NULL) {
                append_parse_error(&parser->errors, illegal_declaration_specifiers(token, conflict));
            } else {
                char_ = token;
            }
        } else if (accept(parser, TK_SHORT, &token)) {
            token_t *conflict = (token_t *) FIRST_NON_NULL(void_, char_, short_, long_, long_long, float_, double_,
                                                           bool_, complex_, struct_or_union, enum_, typedef_name_token);
            if (conflict != NULL) {
                append_parse_error(&parser->errors, illegal_declaration_specifiers(token, conflict));
            } else {
                short_ = token;
            }
        } else if (accept(parser, TK_INT, &token)) {
            token_t *conflict = (token_t *) FIRST_NON_NULL(void_, bool_, int_, float_, double_, complex_, struct_or_union,
                                                           enum_, typedef_name_token);
            if (conflict != NULL) {
                append_parse_error(&parser->errors, illegal_declaration_specifiers(token, conflict));
            } else {
                int_ = token;
            }
        } else if (accept(parser, TK_LONG, &token)) {
            token_t *conflict = (token_t *) FIRST_NON_NULL(void_, bool_, long_long, float_, double_, struct_or_union,
                                                           enum_, typedef_name_token);
            if (conflict != NULL) {
                append_parse_error(&parser->errors, illegal_declaration_specifiers(token, conflict));
            } else if (long_ != NULL) {
                if (complex_ != NULL) {
                    append_parse_error(&parser->errors, illegal_declaration_specifiers(token, complex_));
                } else {
                    long_long = token;
                }
            } else {
                long_ = token;
            }
        } else if (accept(parser, TK_FLOAT, &token)) {
            token_t *conflict = (token_t *) FIRST_NON_NULL(void_, bool_, char_, short_, int_, long_, long_long, float_,
                                                           signed_, unsigned_, struct_or_union, enum_,
                                                           typedef_name_token);
            if (conflict != NULL) {
                append_parse_error(&parser->errors, illegal_declaration_specifiers(token, conflict));
            } else {
                float_ = token;
            }
        } else if (accept(parser, TK_DOUBLE, &token)) {
            token_t *conflict = (token_t *) FIRST_NON_NULL(void_, bool_, char_, short_, int_, long_long, float_,
                                                           double_, signed_, unsigned_, struct_or_union, enum_,
                                                           typedef_name_token);
            if (conflict != NULL) {
                append_parse_error(&parser->errors, illegal_declaration_specifiers(token, conflict));
            } else {
                double_ = token;
            }
        } else if (accept(parser, TK_SIGNED, &token)) {
            token_t *conflict = (token_t *) FIRST_NON_NULL(void_, bool_, float_, double_, signed_, unsigned_,
                                                           struct_or_union, enum_, typedef_name_token);
            if (conflict != NULL) {
                append_parse_error(&parser->errors, illegal_declaration_specifiers(token, conflict));
            } else {
                signed_ = token;
            }
        } else if (accept(parser, TK_UNSIGNED, &token)) {
            token_t *conflict = (token_t *) FIRST_NON_NULL(void_, bool_, float_, double_, signed_, unsigned_,
                                                           struct_or_union, enum_, typedef_name_token);
            if (conflict != NULL) {
                append_parse_error(&parser->errors, illegal_declaration_specifiers(token, conflict));
            } else {
                unsigned_ = token;
            }
        } else if (accept(parser, TK_BOOL, &token)) {
            token_t *conflict = (token_t *) FIRST_NON_NULL(void_, bool_, char_, short_, int_, long_, long_long, float_,
                                                           double_, signed_, unsigned_, complex_, struct_or_union,
                                                           enum_, typedef_name_token);
            if (conflict != NULL) {
                append_parse_error(&parser->errors, illegal_declaration_specifiers(token, conflict));
            } else {
                bool_ = token;
            }
        } else if (accept(parser, TK_COMPLEX, &token)) {
            token_t *conflict = (token_t *) FIRST_NON_NULL(void_, bool_, char_, short_, int_, long_long, signed_,
                                                           unsigned_, struct_or_union, enum_, typedef_name_token);
            if (conflict != NULL) {
                append_parse_error(&parser->errors, illegal_declaration_specifiers(token, conflict));
            } else {
                complex_ = token;
            }
        } else if (peek(parser, TK_STRUCT) || peek(parser, TK_UNION)) {
            token_t *conflict = (token_t *) FIRST_NON_NULL(void_, bool_, char_, short_, int_, long_, long_long, float_,
                                                           double_, signed_, unsigned_, complex_, struct_or_union,
                                                           typedef_name_token, enum_);
            if (conflict != NULL) append_parse_error(&parser->errors, illegal_declaration_specifiers(next_token(parser), conflict));
            struct_type = malloc(sizeof(struct_t));
            if (!parse_struct_or_union_specifier(parser, &struct_or_union, struct_type)) return false;
        } else if (peek(parser, TK_ENUM)) {
            token_t *conflict = (token_t *) FIRST_NON_NULL(void_, bool_, char_, short_, int_, long_, long_long, float_,
                                                           double_, signed_, unsigned_, complex_, struct_or_union,
                                                           typedef_name_token, enum_);
            if (conflict != NULL) append_parse_error(&parser->errors, illegal_declaration_specifiers(next_token(parser), conflict));
            enum_specifier = malloc(sizeof(enum_specifier_t));
            if (!parse_enum_specifier(parser, enum_specifier)) return false;
        } else {
            break;
        }
    }

    if (complex_ != NULL && (float_ == NULL && double_ == NULL)) {
        append_parse_error(&parser->errors, illegal_declaration_specifiers(complex_, NULL));
        complex_ = NULL;
    }

    // Build the type out of the specifiers

    *type = (type_t) {
        .kind = TYPE_VOID,
        .is_const = is_const,
        .is_volatile = is_volatile,
        .storage_class = STORAGE_CLASS_AUTO,
    };

    if (storage_class != NULL) {
        if (storage_class->kind == TK_EXTERN) {
            type->storage_class = STORAGE_CLASS_EXTERN;
        } else if (storage_class->kind == TK_REGISTER) {
            type->storage_class = STORAGE_CLASS_REGISTER;
        } else if (storage_class->kind == TK_STATIC) {
            type->storage_class = STORAGE_CLASS_STATIC;
        } else if (storage_class->kind == TK_TYPEDEF) {
            type->storage_class = STORAGE_CLASS_TYPEDEF;
        }
    }

    if (typedef_name_token != NULL && typedef_type != NULL) {
        // If the type was a typedef, then we use the existing type information as a base type and just update the
        // const/volatile/storage class fields.
        is_const |= typedef_type->is_const;
        is_volatile |= typedef_type->is_volatile;
        const storage_class_t storage_class_value = type->storage_class;
        *type = *typedef_type;
        type->is_const = is_const;
        type->is_volatile = is_volatile;
        type->storage_class = storage_class_value;
    } else if (struct_or_union != NULL) {
        type->kind = TYPE_STRUCT_OR_UNION;
        type->value.struct_or_union = *struct_type;
    } else if (enum_specifier != NULL) {
        type->kind = TYPE_ENUM;
        type->value.enum_specifier = *enum_specifier;
        free(enum_specifier);
        enum_specifier = NULL;
    } else if (ANY_NON_NULL(bool_, char_, short_, int_, long_long, signed_, unsigned_)) {
        type->kind = TYPE_INTEGER;
        type->value.integer.is_signed = unsigned_ == NULL;
        if (bool_ != NULL) {
            type->value.integer.is_signed = false;
            type->value.integer.size = INTEGER_TYPE_BOOL;
        } else if (char_ != NULL) {
            type->value.integer.size = INTEGER_TYPE_CHAR;
        } else if (short_ != NULL) {
            type->value.integer.size = INTEGER_TYPE_SHORT;
        } else if (long_long != NULL) {
            type->value.integer.size = INTEGER_TYPE_LONG_LONG;
        } else if (long_ != NULL) {
            type->value.integer.size = INTEGER_TYPE_LONG;
        } else {
            type->value.integer.size = INTEGER_TYPE_INT;
        }
    } else if (float_ != NULL || double_ != NULL) {
        type->kind = TYPE_FLOATING;
        if (double_ != NULL) {
            if (long_ != NULL) {
                type->value.floating = FLOAT_TYPE_LONG_DOUBLE;
            } else {
                type->value.floating = FLOAT_TYPE_DOUBLE;
            }
        } else {
            type->value.floating = FLOAT_TYPE_FLOAT;
        }
    } else if (long_ != NULL) {
        type->kind = TYPE_INTEGER;
        type->value.integer.is_signed = true;
        type->value.integer.size = INTEGER_TYPE_LONG;
    } else if (void_ != NULL) {
        type->kind = TYPE_VOID;
    } else {
        // Implicit int. This is an error, but we can recover from it.
        append_parse_error(&parser->errors, (parse_error_t) {
                .token = next_token(parser),
                .previous_token = NULL,
                .production_name = "declaration-specifiers",
                .previous_production_name = NULL,
                .kind = PARSE_ERROR_TYPE_SPECIFIER_MISSING,
        });
        type->kind = TYPE_INTEGER;
        type->value.integer.size = INTEGER_TYPE_INT;
        type->value.integer.is_signed = true;
    }

    return true;
}

bool parse_struct_declarator(parser_t *parser, type_t base_type, struct_field_t *field) {
    declaration_t declarator;
    if (!peek(parser, TK_COLON)) {
        if (!parse_declarator(parser, base_type, &declarator)) return false;
        field->identifier = declarator.identifier;
        field->type = declarator.type;
        field->bitfield_width = NULL;
    } else {
        // anonymous bitfield
        field->identifier = NULL;
        type_t *type = malloc(sizeof(type_t));
        *type = base_type;
        field->type = type;
    }

    if (accept(parser, TK_COLON, NULL)) {
        expression_t *expr = malloc(sizeof(expression_t));
        if (!parse_expression(parser, expr)) {
            free(expr);
            return false;
        }
        field->bitfield_width = expr;
    }

    return true;
}

bool parse_struct_declaration(parser_t *parser, struct_t *struct_type) {
    type_t base_type;
    if (!parse_specifier_qualifier_list(parser, &base_type)) {
        return false;
    }

    do {
        struct_field_t *field = malloc(sizeof(struct_field_t));
        if (!parse_struct_declarator(parser, base_type, field)) {
            free(field);
            return false;
        }
        field->index = struct_type->fields.size;
        VEC_APPEND(&struct_type->fields, field);
        if (field->identifier != NULL) {
            hash_table_insert(&struct_type->field_map, field->identifier->value, field);
        }
    } while (accept(parser, TK_COMMA, NULL));

    return require(parser, TK_SEMICOLON, NULL, "struct-declaration", NULL);
}

bool parse_struct_or_union_specifier(parser_t *parser, token_t **keyword, struct_t *struct_type) {
    bool is_union = false;
    if (accept(parser, TK_UNION, keyword)) {
        is_union = true;
    } else if (!require(parser, TK_STRUCT, keyword, "struct-or-union-specifier", NULL)) {
        return false;
    }

    token_t *identifier = NULL;
    accept(parser, TK_IDENTIFIER, &identifier);
    *struct_type = (struct_t) {
        .fields = VEC_INIT,
        .field_map = hash_table_create_string_keys(64),
        .is_union = is_union,
        .identifier = identifier,
        .has_body = false,
        .packed = false,
    };

    if (accept(parser, TK_LBRACE, NULL)) {
        struct_type->has_body = true;
        while (!accept(parser, TK_RBRACE, NULL)) {
            if (!parse_struct_declaration(parser, struct_type)) {
                return false;
            }
        }
    } else if (identifier == NULL) {
        // This is an incomplete struct/union type, which is not allowed for anonymous structs/unions
        // (i.e. a struct/union without a tag).
        append_parse_error(&parser->errors, (parse_error_t) {
            .token = next_token(parser),
            .previous_token = *keyword,
            .production_name = "struct-or-union-specifier",
            .previous_production_name = NULL,
            .kind = PARSE_ERROR_EXPECTED_TOKEN,
            .value.expected_token = {
                .expected_count = 1,
                .expected = TK_IDENTIFIER,
            },
        });
        return true; // can recover
    }

    // give the struct a generated identifier if it doesn't have one
    if (struct_type->identifier == NULL) {
        const char *ident_pfx = "__anon_struct__";
        char index[64];
        snprintf(index, 63, "%u", parser->id_counter++);
        char *ident_value = malloc(strlen(ident_pfx) + strlen(index) + 1);
        snprintf(ident_value, strlen(ident_pfx) + strlen(index) + 1, "%s%s", ident_pfx, index);

        token_t *token = malloc(sizeof(token_t));
        *token = (token_t) {
            .kind = TK_IDENTIFIER,
            .position = (*keyword)->position,
            .value = ident_value,
        };

        struct_type->identifier = token;
    }

    return true;
}

/**
 * <enumerator> ::= <enumeration-constant>
 *                | <enumeration-constant> = <constant-expression>
 * @param parser
 * @return success
 */
bool parse_enumerator(parser_t *parser, enumerator_t *enumerator) {
    // parse the required identifier
    token_t *identifier = NULL;
    if (!require(parser, TK_IDENTIFIER, &identifier, "enumerator", "enumerator-list")) return false;

    // optional initializer
    expression_t *expr = NULL;
    if (accept(parser, TK_ASSIGN, NULL)) {
        expr = malloc(sizeof(expression_t));
        if (!parse_conditional_expression(parser, expr)) {
            free(expr);
            return false;
        }
    }

    *enumerator = (enumerator_t ) {
        .identifier = identifier,
        .value = expr,
    };

    return true;
}

bool parse_enumerator_list(parser_t *parser, enumerator_vector_t *list) {
    // TODO: can leak inner expression on error
    enumerator_t enumerator;

    if (!parse_enumerator(parser, &enumerator)) return false;
    VEC_APPEND(list, enumerator);

    while (accept(parser, TK_COMMA, NULL) && !peek(parser, TK_RBRACE)) {
        // TODO: can leak inner expression on error
        if (!parse_enumerator(parser, &enumerator)) return false;
        VEC_APPEND(list, enumerator);
    }

    return true;
}

bool parse_enum_specifier(parser_t *parser, enum_specifier_t *enum_specifier) {
    // parse the enum keyword, required
    token_t *keyword;
    if (!require(parser, TK_ENUM, &keyword, "enum-specifier", NULL))
        return false;

    // parse the tag identifier, optional if an enumerator list is specified
    token_t *identifier = NULL;
    accept(parser, TK_IDENTIFIER, &identifier);

    // check for the presence of the enumerator-list
    token_t *enumerator_list_start = NULL;
    enumerator_vector_t list = VEC_INIT;
    if (accept(parser, TK_LBRACE, &enumerator_list_start)) {
        if (!parse_enumerator_list(parser, &list)) return false;
        if (!require(parser, TK_RBRACE, NULL, "enum-specifier", NULL))
            return false;
    } else if (identifier == NULL) {
        // no enumerator list, so the identifier is required
        append_parse_error(&parser->errors, (parse_error_t) {
            .token = keyword,
            .previous_token = NULL,
            .previous_production_name = NULL,
            .kind = PARSE_ERROR_ENUM_SPECIFIER_WITHOUT_IDENTIFIER_OR_ENUMERATOR_LIST,
        });
        return false;
    }

    *enum_specifier = (enum_specifier_t) {
        .identifier = identifier,
        .enumerators = list,
    };
    return true;
}


bool parse_declaration_specifiers(parser_t *parser, type_t *type) {
    return parse_specifiers(parser, true, type);
}

bool parse_specifier_qualifier_list(parser_t *parser, type_t *type) {
    return parse_specifiers(parser, false, type);
}

bool parse_init_declarator(parser_t *parser, type_t base_type, declaration_t *decl) {
    if (!parse_declarator(parser, base_type, decl)) {
        return false;
    }

    token_t *token;
    if (accept(parser, TK_ASSIGN, &token)) {
        initializer_t *initializer = malloc(sizeof(initializer_t));
        if (!parse_initializer(parser, initializer)) {
            free(initializer);
            return false;
        }

        decl->initializer = initializer;
    }

    return true;
}

bool parse_initializer(parser_t *parser, initializer_t *initializer) {
    if (accept(parser, TK_LBRACE, NULL)) {
        initializer_list_t *list = malloc(sizeof(initializer_list_t));
        if (!parse_initializer_list(parser, list)) {
            free(list);
            return false;
        }
        *initializer = (initializer_t) {
            .kind = INITIALIZER_LIST,
            .value.list = list,
        };
        return require(parser, TK_RBRACE, NULL, "initializer", NULL);
    } else {
        expression_t *expr = malloc(sizeof(expression_t));
        if (!parse_assignment_expression(parser, expr)) {
            free(expr);
            return false;
        }
        *initializer = (initializer_t) {
            .kind = INITIALIZER_EXPRESSION,
            .value.expression = expr,
        };
        return true;
    }
}

bool parse_initializer_list(parser_t *parser, initializer_list_t *list) {
    *list = (initializer_list_t) VEC_INIT;
    do {
        initializer_list_element_t element = {
            .designation = NULL,
            .initializer = NULL,
        };

        // optional designation
        if (peek(parser, TK_LBRACKET) || peek(parser, TK_DOT)) {
            designator_list_t *designator_list = malloc(sizeof(designator_list_t));
            if (!parse_designation(parser, designator_list)) {
                free(designator_list);
                return false;
            }
            element.designation = designator_list;
        }

        initializer_t *initializer = malloc(sizeof(initializer_t));
        if (!parse_initializer(parser, initializer)) {
            free(initializer);
            return false;
        }

        element.initializer = initializer;
        VEC_APPEND(list, element);
    } while (accept(parser, TK_COMMA, NULL) && !peek(parser, TK_RBRACE));

    return true;
}

bool parse_designation(parser_t *parser, designator_list_t *list) {
    *list = (designator_list_t) VEC_INIT;
    do {
        if (accept(parser, TK_LBRACKET, NULL)) {
            expression_t *index = malloc(sizeof(expression_t));
            if (!parse_conditional_expression(parser, index)) {
                free(index);
                return false;
            }
            designator_t designator = {
                .kind = DESIGNATOR_INDEX,
                .value.index = index,
            };
            if (!require(parser, TK_RBRACKET, NULL, "designation", NULL)) return false;
            VEC_APPEND(list, designator);
        } else if (accept(parser, TK_DOT, NULL)) {
            token_t *identifier;
            if (!require(parser, TK_IDENTIFIER, &identifier, "designator", NULL)) return false;
            designator_t designator = {
                .kind = DESIGNATOR_FIELD,
                .value.field = identifier,
            };
            VEC_APPEND(list, designator);
        } else {
            // Error
            // This should be unreachable
            assert(false);
            return false;
        }
    } while (peek(parser, TK_LBRACKET) || peek(parser, TK_DOT));

    return require(parser, TK_ASSIGN, NULL, "designation", NULL);
}

const type_t* get_innermost_incomplete_type(const type_t *type) {
    const type_t *current = type;
    while (current->kind == TYPE_POINTER || current->kind == TYPE_ARRAY || current->kind == TYPE_FUNCTION) {
        if (current->kind == TYPE_POINTER && current->value.pointer.base != NULL) {
            current = current->value.pointer.base;
        } else if (current->kind == TYPE_ARRAY && current->value.array.element_type != NULL) {
            current = current->value.array.element_type;
        } else if (current->kind == TYPE_FUNCTION && current->value.function.return_type != NULL) {
            current = current->value.function.return_type;
        } else {
            break;
        }
    }
    return current;
}

type_t *build_incomplete_type(ptr_vector_t *left, ptr_vector_t *right) {
    assert(left != NULL && right != NULL);

    if (left->size == 0 && right->size == 0) {
        return NULL;
    }

    reverse_ptr_vector(right->buffer, right->size);

    type_t *current;
    if (right->size > 0) {
        current = pop_ptr(right->buffer, &right->size);
    } else {
        current = pop_ptr(left->buffer, &left->size);
    }
    type_t *outer = current;
    current = get_innermost_incomplete_type(current);

    while (left->size > 0 || right->size > 0) {
        type_t *next;
        if (right->size > 0) {
            next = pop_ptr(right->buffer, &right->size);
        } else {
            next = pop_ptr(left->buffer, &left->size);
        }

        if (current->kind == TYPE_POINTER) {
            current->value.pointer.base = next;
        } else if (current->kind == TYPE_ARRAY) {
            current->value.array.element_type = next;
        } else if (current->kind == TYPE_FUNCTION) {
            current->value.function.return_type = next;
        } else {
            assert(false); // Invalid type stack
        }
        current = get_innermost_incomplete_type(next);
    }

    return outer;
}

/**
 * Inner function for parsing a declarator.
 * Outputs the raw identifier token and the incomplete type stack.
 *
 * @param parser Parser instance
 * @param identifier_out Output parameter for the pointer to the identifier token.
 * @param left Reference to the declarator segments to the left of the identifier
 * @param right Reference to the declarator segments to the right of the identifier
 * @return true if the declarator was parsed successfully, false otherwise
 */
bool _parse_declarator(parser_t *parser, token_t **identifier_out, type_t **type_out) {
    ptr_vector_t left = (ptr_vector_t) {.buffer = NULL, .size = 0, .capacity = 0};
    ptr_vector_t right = (ptr_vector_t) {.buffer = NULL, .size = 0, .capacity = 0};
    if (accept(parser, TK_STAR, NULL)) {
        type_t *type = NULL;
        if (!parse_pointer(parser, NULL, &type)) {
            return false;
        }
        append_ptr(&left.buffer, &left.size, &left.capacity, type);
    }

    if (!parse_direct_declarator(parser, identifier_out, &left, &right)) {
        return false;
    }

    *type_out = build_incomplete_type(&left, &right);

    return true;
}

/**
 * Parses a declarator.
 *
 * <declarator> ::= <pointer>? <direct-declarator>
 *
 * @param parser Parser instance
 * @param base_type Base type derived from the declaration specifiers (e.g. int, float, etc...)
 * @param declaration Out parameter for the parsed declaration
 * @return true if the declarator was parsed successfully, false otherwise
 */
bool parse_declarator(parser_t *parser, type_t base_type, declaration_t *declaration) {
    token_t *identifier = NULL;
    type_t *type = NULL;
    if (!_parse_declarator(parser, &identifier, &type)) {
        return false;
    }

    // If this is a typedef, move the typedef storage class specifier to the outer type
    bool is_typedef = base_type.storage_class == STORAGE_CLASS_TYPEDEF;
    if (is_typedef) base_type.storage_class = STORAGE_CLASS_AUTO;

    // Move the base type to the heap
    type_t *base = malloc(sizeof(type_t));
    *base = base_type;

    if (type == NULL) {
        type = base;
    } else {
        type_t *inner = get_innermost_incomplete_type(type);
        if (inner->kind == TYPE_POINTER) {
            inner->value.pointer.base = base;
        } else if (inner->kind == TYPE_ARRAY) {
            inner->value.array.element_type = base;
        } else if (inner->kind == TYPE_FUNCTION) {
            inner->value.function.return_type = base;
        } else {
            assert(false); // Invalid type stack
        }
    }

    if (is_typedef) type->storage_class = STORAGE_CLASS_TYPEDEF;

    declaration->identifier = identifier;
    declaration->type = type;
    declaration->initializer = NULL;

    return true;
}

/**
 * Parses a pointer.
 *
 * <pointer> ::= '*' <type-qualifier-list>? <pointer>?
 *
 * @param parser Parser instance.
 * @param base_type May be NULL, will result in an incomplete type.
 * @return True if the pointer was parsed successfully, false otherwise
 */
bool parse_pointer(parser_t *parser, const type_t *base_type, type_t **pointer_type) {
    assert(parser != NULL && pointer_type != NULL);

    bool is_const = false;
    bool is_volatile = false;
    bool is_restrict = false;

    // Parse type-qualifiers
    while (true) {
        token_t *token;
        if (accept(parser, TK_CONST, &token)) {
            is_const = true;
        } else if (accept(parser, TK_RESTRICT, &token)) {
            is_restrict = true;
        } else if (accept(parser, TK_VOLATILE, &token)) {
            is_volatile = true;
        } else {
            break;
        }
    }

    *pointer_type = malloc(sizeof(type_t));
    **pointer_type = (type_t) {
        .kind = TYPE_POINTER,
        .value.pointer = {
            .base = base_type,
            .is_const = is_const,
            .is_volatile = is_volatile,
            .is_restrict = is_restrict,
        },
    };

    if (accept(parser, TK_STAR, NULL)) {
        parse_pointer(parser, *pointer_type, pointer_type);
    }

    return true;
}

bool parse_direct_declarator_prime(parser_t *parser, type_t **type);

/**
 * Parses a direct declarator.
 * More specifically, it converts the direct-declarator into a type + identifier.
 *
 * After eliminating left recursion, the grammar for direct-declarator is:
 *
 * <direct-declarator> ::= <identifier> <direct-declarator-prime>?
 *                       | '(' <declarator> ')' <direct-declarator-prime>?
 *
 * <direct-declarator-prime> ::= '[' <type-qualifier-list>? <assignment-expression>? ']'          // Array
 *                             | '[' 'static' <type-qualifier-list>? <assignment-expression> ']'  // Array
 *                             | '[' <type-qualifier-list> 'static' <assignment-expression> ']'   // Array
 *                             | '[' <type-qualifier-list>? '*' ']'                               // Variable Length Array (VLA) - Not Implemented
 *                             | '(' <parameter-type-list> ')'                                    // Parameter type list for a function pointer
 *                             | '(' <identifier-list>? ')'                                       // K&R style function declaration - Not Implemented
 *
 * @param parser Parser instance
 * @param identifier_out Output parameter for the pointer to the identifier token.
 * @param left Reference to the declarator segments to the left of the identifier
 * @param right Reference to the declarator segments to the right of the identifier
 * @return true if the direct declarator was parsed successfully, false otherwise
 */
bool parse_direct_declarator(parser_t *parser, token_t **identifier_out, ptr_vector_t *left, ptr_vector_t *right) {
    // Declarations are parsed from left to right, but decoded from the inside out starting at the identifier.
    // `()` (function) and `[]` (array) have a higher precedence than `*` (pointer).

    if (accept(parser, TK_IDENTIFIER, identifier_out)) {
        // Parse the components to the right of the identifier
        while (next_token(parser)->kind == TK_LPAREN || next_token(parser)->kind == TK_LBRACKET) {
            type_t *inner;
            if (!parse_direct_declarator_prime(parser, &inner)) {
                return false;
            }
            append_ptr(&right->buffer, &right->size, &right->capacity, inner);
        }

        return true;
    } else if (accept(parser, TK_LPAREN, NULL)) {
        // Parenthesized declarator (e.g. int (foo), int (*foo), int (*foo)(), etc...).
        // Can be arbitrarily nested.
        // Example: `int (*foo[2])(void);` -> type of `foo` = array 2 of pointer to function (void) returning int

        // Parse the nested declarator.
        {
            type_t *inner;
            if (!_parse_declarator(parser, identifier_out, &inner)) {
                return false;
            }
            if (inner != NULL) {
                append_ptr(&right->buffer, &right->size, &right->capacity, inner);
            }
        }

        if (!require(parser, TK_RPAREN, NULL, "direct-declarator", NULL)) {
            return false;
        }

        while (next_token(parser)->kind == TK_LPAREN || next_token(parser)->kind == TK_LBRACKET) {
            type_t *inner;
            if (!parse_direct_declarator_prime(parser, &inner)) {
                return false;
            }
            append_ptr(&right->buffer, &right->size, &right->capacity, inner); // push onto the stack
        }

        return true;
    } else {
        append_parse_error(&parser->errors, (parse_error_t) {
            .token = next_token(parser),
            .previous_token = NULL,
            .production_name = "direct-declarator",
            .previous_production_name = NULL,
            .kind = PARSE_ERROR_EXPECTED_TOKEN,
            .value.expected_token = {
                .expected_count = 2,
                .expected = {TK_IDENTIFIER, TK_LPAREN},
            },
        });
        return false;
    }
}

/**
 * Parses the right hand side of a direct (possibly abstract) declarator.
 * @param parser Parser instance
 * @param type Partial type derived from the declaration specifiers
 * @return true if the direct declarator prime was parsed successfully, false otherwise
 */
bool parse_direct_declarator_prime(parser_t *parser, type_t **type) {
    if (accept(parser, TK_LBRACKET, NULL)) {
        // This is an array declaration.
        bool is_static = false;
        bool is_const = false;
        bool is_volatile = false;
        bool is_restrict = false;

        // Parse type-qualifiers (and 'static') for the size expression.
        // TODO: what are these type-qualifiers for?
        // Ignores duplicates and illegal combinations of specifiers.
        // TODO: report errors for illegal combinations of specifiers
        // TODO: report warnings for duplicate specifiers
        while (true) {
            token_t *token;
            if (accept(parser, TK_CONST, &token)) {
                is_const = true;
            } else if (accept(parser, TK_RESTRICT, &token)) {
                is_restrict = true;
            } else if (accept(parser, TK_VOLATILE, &token)) {
                is_volatile = true;
            } else {
                break;
            }
        }

        expression_t *size;
        if (accept(parser, TK_RBRACKET, NULL))  {
            // No size specified (e.g. `int a[]`)
            size = NULL;
        } else {
            size = malloc(sizeof(expression_t));
            if (!parse_assignment_expression(parser, size)) {
                free(size);
                return false;
            }
            if (!require(parser, TK_RBRACKET, NULL, "direct-declarator-prime", NULL)) {
                free(size);
                return false;
            }
        }

        // Build the new partial array type.
        // At this time we don't know what the element type is, so we just store the size and the qualifiers.

        (*type) = malloc(sizeof(type_t));
        **type = (type_t) {
            .kind = TYPE_ARRAY,
            .value.array = {
                .element_type = NULL, // This will be filled in later
                .size = size,
            },
        };

        return true;
    } else if (accept(parser, TK_LPAREN, NULL)) {
        // This is a function declaration.
        parameter_type_list_t *parameters = malloc(sizeof(parameter_type_list_t));
        if (!parse_parameter_type_list(parser, parameters)) {
            return false;
        }

        (*type) = malloc(sizeof(type_t));
        **type = (type_t) {
            .kind = TYPE_FUNCTION,
            .value.function = {
                .return_type = NULL, // This will be filled in later
                .parameter_list = parameters,
            },
        };

        return true; // already consumed closing paren in parse_parameter_type_list
    } else {
        return false;
    }
}

/**
 * Parses a parameter type list.
 *
 * <parameter-type-list> ::= <parameter-list>
 *                         | <parameter-list> ',' '...'
 *
 * <parameter-list> ::= <parameter-declaration>
 *                    | <parameter-list> ',' <parameter-declaration>
 *
 * <parameter-declaration> ::= <declaration-specifiers> <declarator>
 *                           | <declaration-specifiers> <abstract-declarator>?
 */
bool parse_parameter_type_list(parser_t *parser, parameter_type_list_t *parameters) {
    // At this point we've already parsed the opening parenthesis.
    // We need to parse the parameter list (which may be empty) and then the closing parenthesis.
    ptr_vector_t vec = {.buffer = NULL, .size = 0, .capacity = 0};
    *parameters = (parameter_type_list_t) {
            .parameters = NULL,
            .length = 0,
            .variadic = false,
    };

    if (accept(parser, TK_RPAREN, NULL)) {
        // This is an empty parameter list.
        return true;
    }

    do {
        if (accept(parser, TK_ELLIPSIS, NULL)) {
            // This is a variadic function declaration.
            parameters->variadic = true;
            break;
        } else if (next_token(parser)->kind == TK_EOF) {
            // This is the end of the parameter list.
            break;
        }

        type_t *base = malloc(sizeof(type_t));
        if (!parse_declaration_specifiers(parser, base)) {
            free(base);
            return false;
        }

        parameter_declaration_t *param = malloc(sizeof(parameter_declaration_t));
        *param = (parameter_declaration_t) {
                .type = base,
                .identifier = NULL,
        };

        if (next_token(parser)->kind == TK_COMMA) {
            // This is a parameter declaration without a declarator/abstract-declarator.
            append_ptr(&vec.buffer, &vec.size, &vec.capacity, param);
        } else if (next_token(parser)->kind == TK_RPAREN) {
            // This is a parameter declaration with an identifier, and it's the last parameter.
            append_ptr(&vec.buffer, &vec.size, &vec.capacity, param);
            break;
        } else {
            // The declaration specifiers can either be followed by a declarator, or an abstract-declarator.
            // It's not possible to predict which is next with a fixed lookahead, so we will try to parse both and backtrack
            // after any failures.
            parse_checkpoint_t checkpoint = create_checkpoint(parser);

            declaration_t decl;
            if (parse_declarator(parser, *base, &decl)) {
                param->type = decl.type;
                param->identifier = decl.identifier;
                append_ptr(&vec.buffer, &vec.size, &vec.capacity, param);
            } else {
                backtrack(parser, checkpoint); // parse_declarator failed, so reset the parser state
                type_t *type = NULL;
                if (parse_abstract_declarator(parser, *base, &type)) {
                    param->identifier = NULL;
                    param->type = type;
                    append_ptr(&vec.buffer, &vec.size, &vec.capacity, param);
                } else {
                    append_parse_error(&parser->errors, (parse_error_t) {
                            .kind = PARSE_ERROR_PARAMETER_TYPE_MALFORMED,
                            .token = next_token(parser),
                            .production_name = "parameter-declaration",
                    });
                    return false;
                }
            }
        }
    } while (accept(parser, TK_COMMA, NULL));

    if (!require(parser, TK_RPAREN, NULL, "parameter-type-list", NULL)) {
        return false;
    }

    shrink_ptr_vector((void ***) &vec.buffer, &vec.size, &vec.capacity);
    parameters->parameters = (parameter_declaration_t**) vec.buffer;
    parameters->length = vec.size;

    // Special case: If the parameter type list is `(void)`, then it's an empty parameter list.
    if (parameters->length == 1 && parameters->parameters[0]->type->kind == TYPE_VOID && parameters->parameters[0]->identifier == NULL) {
        parameters->parameters = NULL;
        parameters->length = 0;
    }

    return true;
}

/**
 * Parses a type name (e.g. for a cast expression).
 *
 * @param parser Parser instance
 * @param type_out Output parameter for the derived type
 * @return true if the type name was parsed successfully, false otherwise
 */
bool parse_type_name(parser_t *parser, type_t **type_out) {
    if (typedef_name(parser, false, NULL, type_out)) return true;

    type_t *base_type = malloc(sizeof(type_t));
    parse_specifier_qualifier_list(parser, base_type);

    // abstract-declarator is optional
    if (peek(parser, TK_STAR) || peek(parser, TK_LPAREN) || peek(parser, TK_LBRACKET)) {
        if (!parse_abstract_declarator(parser, *base_type, type_out)) {
            free(base_type);
            return false;
        }
    } else {
        *type_out = base_type;
    }

    return true;
}

bool parse_direct_abstract_declarator(parser_t *parser, ptr_vector_t *right);

/**
 * Inner function for parsing an abstract declarator.
 * Outputs the incomplete type derived from the declarator.
 *
 * @param parser Parser instance
 * @param identifier_out Output parameter for the pointer to the identifier token.
 * @param left Reference to the declarator segments to the left of the identifier
 * @param right Reference to the declarator segments to the right of the identifier
 * @return true if the declarator was parsed successfully, false otherwise
 */
bool _parse_abstract_declarator(parser_t *parser, type_t **type_out) {
    ptr_vector_t left = (ptr_vector_t) {.buffer = NULL, .size = 0, .capacity = 0};
    ptr_vector_t right = (ptr_vector_t) {.buffer = NULL, .size = 0, .capacity = 0};

    bool matched_ptr = false;
    if (accept(parser, TK_STAR, NULL)) {
        matched_ptr = true;
        type_t *type = NULL;
        if (!parse_pointer(parser, NULL, &type)) {
            return false;
        }
        append_ptr(&left.buffer, &left.size, &left.capacity, type);
    }

    // The <direct-abstract-declarator> is optional if we've already matched a pointer
    if (peek(parser, TK_LPAREN) || peek(parser, TK_LBRACKET) || !matched_ptr) {
        if (!parse_direct_abstract_declarator(parser, &right)) {
            return false;
        }
    }

    *type_out = build_incomplete_type(&left, &right);

    return true;
}

/**
 * Parses an abstract declarator.
 * TODO: shares a lot of code with parse_declarator, consider refactoring
 *
 * <abstract-declarator> ::= <pointer>
 *                         | <pointer>? <direct-abstract-declarator>
 *
 * @param parser Parser instance
 * @param base_type Base type derived from the declaration specifiers (e.g. int, float, etc...)
 * @param type_out Out parameter for the derived type
 * @return true if the abstract declarator was parsed successfully, false otherwise
 */
bool parse_abstract_declarator(parser_t *parser, type_t base_type, type_t **type_out) {
    type_t *type = NULL;
    if (!_parse_abstract_declarator(parser, &type)) {
        return false;
    }

    // Move the base type to the heap
    type_t *base = malloc(sizeof(type_t));
    *base = base_type;

    if (type == NULL) {
        type = base;
    } else {
        type_t *inner = get_innermost_incomplete_type(type);
        if (inner->kind == TYPE_POINTER) {
            inner->value.pointer.base = base;
        } else if (inner->kind == TYPE_ARRAY) {
            inner->value.array.element_type = base;
        } else if (inner->kind == TYPE_FUNCTION) {
            inner->value.function.return_type = base;
        } else {
            assert(false); // Invalid type stack
        }
    }

    *type_out = type;

    return true;
}

/**
 * Parses a direct abstract declarator.
 *
 * After eliminating left recursion, the grammar for direct-abstract-declarator is:
 *
 * <direct-abstract-declarator> ::= '(' <abstract-declarator> ')' <direct-abstract-declarator-prime>*
 *                                | <direct-abstract-declarator-prime>+
 *
 * <direct-abstract-declarator-prime> ::= '[' <type-qualifier-list>? <assignment-expression>? ']'          // Array
 *                                      | '[' 'static' <type-qualifier-list>? <assignment-expression> ']'  // Array
 *                                      | '[' <type-qualifier-list> 'static' <assignment-expression> ']'   // Array
 *                                      | '[' '*' ']'                                                      // Variable Length Array (VLA) - Not Implemented
 *                                      | '(' <parameter-type-list> ')'                                    // Parameter type list for a function pointer
 *
 * @param parser Parser instance
 * @param right Reference to the declarator segments to the right of the identifier (or rather, where the imaginary identifier would be)
 * @return true if the direct declarator was parsed successfully, false otherwise
 */
bool parse_direct_abstract_declarator(parser_t *parser, ptr_vector_t *right) {
    if (accept(parser, TK_LPAREN, NULL)) {
        {
            type_t *inner;
            if (!_parse_abstract_declarator(parser, &inner)) {
                return false;
            }
            if (inner != NULL) {
                append_ptr(&right->buffer, &right->size, &right->capacity, inner);
            }
        }

        if (!require(parser, TK_RPAREN, NULL, "direct-abstract-declarator", NULL)) {
            return false;
        }

        while (next_token(parser)->kind == TK_LPAREN || next_token(parser)->kind == TK_LBRACKET) {
            type_t *inner;
            if (!parse_direct_declarator_prime(parser, &inner)) {
                return false;
            }
            append_ptr(&right->buffer, &right->size, &right->capacity, inner); // push onto the stack
        }

        return true;
    } else {
        bool matched_at_least_one = false;
        while (peek(parser, TK_LPAREN) || peek(parser, TK_LBRACKET)) {
            type_t *inner;
            if (!parse_direct_declarator_prime(parser, &inner)) {
                return false;
            }
            append_ptr(&right->buffer, &right->size, &right->capacity, inner); // push onto the stack
            matched_at_least_one = true;
        }

        if (matched_at_least_one) {
            return true;
        } else {
            append_parse_error(&parser->errors, (parse_error_t) {
                .token = next_token(parser),
                .previous_token = NULL,
                .production_name = "direct-abstract-declarator",
                .previous_production_name = NULL,
                .kind = PARSE_ERROR_EXPECTED_TOKEN,
                .value.expected_token = {
                    .expected_count = 2,
                    .expected = {TK_LPAREN, TK_LBRACKET},
                },
            });
            return false;
        }
    }
}

// Statements

bool parse_statement(parser_t *parser, statement_t *stmt) {
    token_t *terminator;
    if (accept(parser, TK_SEMICOLON, &terminator)) {
        *stmt = (statement_t) {
                .kind = STATEMENT_EMPTY,
                .terminator = terminator,
        };
        return true;
    }

    token_t *begin = NULL;
    if (accept(parser, TK_LBRACE, &begin)) {
        parser_enter_scope(parser);
        bool success = parse_compound_statement(parser, stmt, begin);
        parser_leave_scope(parser);
        return success;
    } else if (accept(parser, TK_IF, &begin)) {
        return parse_if_statement(parser, stmt, begin);
    } else if (accept(parser, TK_RETURN, &begin)) {
        return parse_return_statement(parser, stmt, begin);
    } else if (accept(parser, TK_WHILE, &begin)) {
        return parse_while_statement(parser, stmt, begin);
    } else if (peek(parser, TK_DO)) {
        return parse_do_while_statement(parser, stmt);
    } else if (accept(parser, TK_FOR, &begin)) {
        return parse_for_statement(parser, stmt, begin);
    } else if (peek(parser, TK_BREAK)) {
        return parse_break_statement(parser, stmt);
    } else if (peek(parser, TK_CONTINUE)) {
        return parse_continue_statement(parser, stmt);
    } else if (peek(parser, TK_GOTO)) {
        return parse_goto_statement(parser, stmt);
    } else if (peek(parser, TK_IDENTIFIER) && peek2(parser, TK_COLON)) {
        return parse_labeled_statement(parser, stmt);
    } else if (peek(parser, TK_SWITCH)) {
        return parse_switch_statement(parser, stmt);
    } else if (peek(parser, TK_CASE)) {
        return parse_case_statement(parser, stmt);
    } else if (peek(parser, TK_DEFAULT)) {
        return parse_default_case_statement(parser, stmt);
    } else {
        return parse_expression_statement(parser, stmt);
    }
}

bool parse_compound_statement(parser_t *parser, statement_t *stmt, const token_t* open_brace) {
    ptr_vector_t block_items = {.buffer = NULL, .size = 0, .capacity = 0};

    token_t *last_token;
    while (!accept(parser, TK_RBRACE, &last_token) && !accept(parser, TK_EOF, &last_token)) {
        // Peek at the next token.
        const token_t *next = next_token(parser);
        if (TOKEN_KIND_ONE_OF(next->kind,  TK_TYPEDEF, TK_EXTERN, TK_STATIC, TK_AUTO, TK_REGISTER,
                              TK_CONST, TK_RESTRICT, TK_VOLATILE, TK_INLINE, TK_VOID, TK_CHAR, TK_SHORT, TK_INT,
                              TK_LONG, TK_FLOAT, TK_DOUBLE, TK_SIGNED, TK_UNSIGNED, TK_BOOL, TK_COMPLEX, TK_STRUCT,
                              TK_UNION, TK_ENUM) || typedef_name(parser, true, NULL, NULL)) {
            // This is a declaration
            ptr_vector_t declarations = {.buffer = NULL, .size = 0, .capacity = 0};
            if (!parse_declaration(parser, &declarations)) {
                // We can recover from a malformed declaration by skipping tokens until we consume the next semicolon.
                recover(parser);
                continue;
            }
            for (int i = 0; i < declarations.size; i += 1) {
                block_item_t  *block_item = malloc(sizeof(block_item_t));
                *block_item = (block_item_t) {
                        .kind = BLOCK_ITEM_DECLARATION,
                        .value.declaration =  declarations.buffer[i],
                };
                append_ptr((void ***) &block_items.buffer, &block_items.size, &block_items.capacity, block_item);
            }
        } else {
            // We can recover from parse errors following a parse error in a statement by skipping tokens until
            // we find a semicolon.
            // An error has already been appended to the parser's error vector at this point.
            statement_t *statement = malloc(sizeof(statement_t));
            if (!parse_statement(parser, statement)) {
                free(statement);
                recover(parser);
                continue;
            }
            block_item_t *block_item = malloc(sizeof(block_item_t));
            *block_item = (block_item_t) {
                    .kind = BLOCK_ITEM_STATEMENT,
                    .value.statement = statement,
            };
            append_ptr((void ***) &block_items.buffer, &block_items.size, &block_items.capacity, block_item);
        }
    }
    shrink_ptr_vector((void ***) &block_items.buffer, &block_items.size, &block_items.capacity);

    if (last_token->kind == TK_RBRACE) {
        *stmt = (statement_t) {
            .kind = STATEMENT_COMPOUND,
            .value.compound = {
                .open_brace = open_brace,
                .block_items = block_items,
            },
            .terminator = last_token,
        };
        return true;
    } else {
        // TODO: free allocated statements?
        append_parse_error(&parser->errors, (parse_error_t) {
            .token = last_token,
            .previous_token = parser->next_token_index > 0 ? parser->tokens.buffer[parser->next_token_index - 1] : NULL,
            .production_name = "compound-statement",
            .previous_production_name = NULL,
            .kind = PARSE_ERROR_UNEXPECTED_END_OF_INPUT,
            .value.unexpected_end_of_input = {
                .expected = TK_RBRACE,
            },
        });
        return false;
    }
}

bool parse_if_statement(parser_t* parser, statement_t *statement, token_t *keyword) {
    require(parser, TK_LPAREN, NULL, "if-statement", NULL);
    expression_t *condition = malloc(sizeof(expression_t));
    if (!parse_expression(parser, condition)) {
        free(condition);
        return false;
    }
    require(parser, TK_RPAREN, NULL, "if-statement", NULL);

    statement_t *then_statement = malloc(sizeof(statement_t));
    if (!parse_statement(parser, then_statement)) {
        free(then_statement);
        return false;
    }

    statement_t *else_statement = NULL;
    if (accept(parser, TK_ELSE, NULL)) {
        else_statement = malloc(sizeof(statement_t));
        if (!parse_statement(parser, else_statement)) {
            free(else_statement);
            return false;
        }
    }

    *statement = (statement_t) {
        .kind = STATEMENT_IF,
        .value.if_ = {
            .keyword = keyword,
            .condition = condition,
            .true_branch = then_statement,
            .false_branch = else_statement,
        },
    };

    return true;
}

bool parse_return_statement(parser_t *parser, statement_t *stmt, token_t *keyword) {
    token_t *terminator;
    expression_t *expr = NULL;

    if (!accept(parser, TK_SEMICOLON, &terminator)) {
        expr = malloc(sizeof(expression_t));
        if (!parse_expression(parser, expr)) {
            free(expr);
            return false;
        }

        // Should we insert a semicolon here for error recovery?
        if (!require(parser, TK_SEMICOLON, &terminator, "return-statement", "expression")) {
            free(expr);
            return false;
        }
    }

    *stmt = (statement_t) {
        .kind = STATEMENT_RETURN,
        .value.return_ = {
            .keyword = keyword,
            .expression = expr,
        },
        .terminator = terminator,
    };

    return true;
}

bool parse_while_statement(parser_t* parser, statement_t *statement, token_t *keyword) {
    if (!require(parser, TK_LPAREN, NULL, "while-statement", NULL)) return false;

    expression_t *condition = malloc(sizeof(expression_t));
    if (!parse_expression(parser, condition)) {
        free(condition);
        return false;
    }

    token_t *terminator = NULL;
    if (!require(parser, TK_RPAREN, &terminator, "while-statement", NULL)) return false;

    statement_t *body = malloc(sizeof(statement_t));
    if (!parse_statement(parser, body)) {
        free(body);
        return false;
    }

    *statement = (statement_t) {
        .kind = STATEMENT_WHILE,
        .value.while_ = {
            .keyword = keyword,
            .condition = condition,
            .body = body,
        },
        .terminator = terminator,
    };

    return true;
}

bool parse_do_while_statement(parser_t *parser, statement_t *statement) {
    token_t *do_token, *while_token, *terminator;
    if (!require(parser, TK_DO, &do_token, "do-while-statement", NULL))
        return false;

    statement_t *body = malloc(sizeof(statement_t));
    if (!parse_statement(parser, body)) {
        free(body);
        return false;
    }

    if (!require(parser, TK_WHILE, &while_token, "do-while-statement", NULL))
        return false;

    if (!require(parser, TK_LPAREN, NULL, "do-while-statement", NULL))
        return false;

    expression_t *condition = malloc(sizeof(expression_t));
    if (!parse_expression(parser, condition)) {
        free(body);
        free(condition);
        return false;
    }

    if (!require(parser, TK_RPAREN, NULL, "do-while-statement", NULL))
        return false;

    if (!require(parser, TK_SEMICOLON, &terminator, "do-while-statement", NULL))
        return false;

    *statement = (statement_t) {
        .kind = STATEMENT_DO_WHILE,
        .terminator = terminator,
        .value.do_while = {
            .body = body,
            .condition = condition,
            .do_keyword = do_token,
            .while_keyword = while_token,
        },
    };
    return true;
}

bool parse_for_statement(parser_t* parser, statement_t *statement, token_t *keyword) {
    if (!require(parser, TK_LPAREN, NULL, "for-statement", NULL)) return false;

    // The for statement initializer begins a new scope
    // Without this, the following would be rejected by the parser:
    // ```
    // int i = 42;
    // for (int i = 0; i < 10; i += 1) {}
    // ```
    const parser_scope_t *prev_scope = parser->symbol_table->current_scope;
    parser_enter_scope(parser);

    statement->kind = STATEMENT_FOR;
    statement->value.for_.keyword = keyword;

    // Parse the initializer
    // It can be:
    // 1. A declaration
    // 2. An expression statement
    // 3. Or just a semicolon (special case of the above)

    // If the next-token is a declaration specifier, then it must be a declaration
    bool is_declaration = typedef_name(parser, true, NULL, NULL);
    for (int i = 0; i < sizeof (DECLARATION_SPECIFIER_TOKENS) / sizeof (token_kind_t); i += 1) {
        if (peek(parser, DECLARATION_SPECIFIER_TOKENS[i])) {
            is_declaration = true;
            break;
        }
    }

    if (is_declaration) {
        // This is a declaration
        statement->value.for_.initializer.kind = FOR_INIT_DECLARATION;
        statement->value.for_.initializer.declarations = malloc(sizeof(ptr_vector_t));
        *statement->value.for_.initializer.declarations = (ptr_vector_t ) VEC_INIT;
        if (!parse_declaration(parser, statement->value.for_.initializer.declarations)) {
            free(statement->value.for_.initializer.declarations);
            statement->value.for_.initializer.declarations = NULL;
            goto error;
        }
    } else {
        // This should be an expression statement, or an empty statement
        const token_t *start = next_token(parser);
        statement->value.for_.initializer.kind = FOR_INIT_EXPRESSION;
        statement_t *initializer = malloc(sizeof(statement_t));
        if (!parse_statement(parser, initializer)) {
            free(initializer);
            goto error;
        } else {
            if (initializer->kind == STATEMENT_EMPTY) {
                statement->value.for_.initializer.kind = FOR_INIT_EMPTY;
            } else if (initializer->kind == STATEMENT_EXPRESSION) {
                statement->value.for_.initializer.expression = initializer->value.expression;
            } else {
                // Parsed some other statement, which is an error
                append_parse_error(&parser->errors, (parse_error_t) {
                    .token = start,
                    .previous_token = NULL,
                    .production_name = "for-statement",
                    .previous_production_name = NULL,
                    .kind = PARSE_ERROR_EXPECTED_EXPRESSION,
                });
                goto error;
            }
        }
    }

    // Parse the expression for the condition
    if (!accept(parser, TK_SEMICOLON, NULL)) {
        statement->value.for_.condition = malloc(sizeof(expression_t));
        if (!parse_expression(parser, statement->value.for_.condition)) {
            free(statement->value.for_.condition);
            goto error;
        }
        if (!require(parser, TK_SEMICOLON, NULL, "for-statement", "expression")) return false;
    } else {
        statement->value.for_.condition = NULL;
    }

    // Parse the post-expression
    if (!accept(parser, TK_RPAREN, NULL)) {
        statement->value.for_.post = malloc(sizeof(expression_t));
        if (!parse_expression(parser, statement->value.for_.post)) {
            free(statement->value.for_.post);
            goto error;
        }
        if (!require(parser, TK_RPAREN, NULL, "for-statement", "expression")) return false;
    } else {
        statement->value.for_.post = NULL;
    }

    statement_t *body = malloc(sizeof(statement_t));
    if (!parse_statement(parser, body)) {
        free(body);
        goto error;
    }

    statement->value.for_.body = body;
    return true;

error:
    // leave whatever scopes we've entered
    while (parser->symbol_table->current_scope != prev_scope) parser_leave_scope(parser);
    return false;
}

bool parse_break_statement(parser_t *parser, statement_t *statement) {
    token_t *keyword = NULL;
    if (!accept(parser, TK_BREAK, &keyword)) return false;

    token_t *semicolon = NULL;
    bool terminated = require(parser, TK_SEMICOLON, &semicolon, "break-statement", NULL);

    *statement = (statement_t) {
        .terminator = semicolon,
        .kind = STATEMENT_BREAK,
        .value.break_ = {
            .keyword = keyword,
        },
    };

    return terminated;
}

bool parse_continue_statement(parser_t *parser, statement_t *statement) {
    token_t *keyword = NULL;
    if (!accept(parser, TK_CONTINUE, &keyword)) return false;

    token_t *semicolon = NULL;
    bool terminated = require(parser, TK_SEMICOLON, &semicolon, "break-statement", NULL);

    *statement = (statement_t) {
        .terminator = semicolon,
        .kind = STATEMENT_CONTINUE,
        .value.continue_ = {
            .keyword = keyword,
        },
    };

    return terminated;
}

bool parse_goto_statement(parser_t *parser, statement_t *statement) {
    token_t *keyword = NULL;
    if (!accept(parser, TK_GOTO, &keyword)) return false;

    token_t *identifier = NULL;
    if (!require(parser, TK_IDENTIFIER, &identifier, "goto-statement", NULL))
        return false;

    token_t *semicolon = NULL;
    bool terminated = require(parser, TK_SEMICOLON, &semicolon, "goto-statement", NULL);

    *statement = (statement_t) {
        .terminator = semicolon,
        .kind = STATEMENT_GOTO,
        .value.goto_ = {
            .identifier = identifier,
        },
    };

    return terminated;
}

bool parse_labeled_statement(parser_t *parser, statement_t *statement) {
    token_t *identifier = NULL;
    if (!require(parser, TK_IDENTIFIER, &identifier, "labeled-statement", NULL))
        return false;
    if (!require(parser, TK_COLON, NULL, "labeled-statement", NULL))
        return false;
    statement_t *statement_inner = malloc(sizeof(statement_t));
    if (!parse_statement(parser, statement_inner))
        return false;
    *statement = (statement_t) {
        .kind = STATEMENT_LABEL,
        .value.label_ = {
            .identifier = identifier,
            .statement = statement_inner,
        },
    };
    return true;
}

bool parse_switch_statement(parser_t *parser, statement_t *statement) {
    token_t *keyword = NULL;
    if (!require(parser, TK_SWITCH, &keyword, "switch-statement", NULL))
        return false;

    if (!require(parser, TK_LPAREN, NULL, "switch-statement", NULL))
        return false;

    expression_t *expression = malloc(sizeof(expression_t));
    if (!parse_expression(parser, expression)) {
        free(expression);
        return false;
    }

    if (!require(parser, TK_RPAREN, NULL, "switch-statement", NULL)) {
        free(expression);
        return false;
    }

    statement_t *inner_statement = malloc(sizeof(statement_t));
    if (!parse_statement(parser, inner_statement)) {
        free(expression);
        free(inner_statement);
        return false;
    }

    *statement = (statement_t) {
        .kind = STATEMENT_SWITCH,
        .terminator = inner_statement->terminator,
        .value = {
            .switch_ = {
                .keyword = keyword,
                .expression = expression,
                .statement = inner_statement,
            }
        }
    };
    return true;
}

bool parse_case_statement(parser_t *parser, statement_t *stmt) {
    token_t *keyword = NULL;
    if (!require(parser, TK_CASE, &keyword, "case-statement", NULL))
        return false;

    expression_t expr;
    if (!parse_expression(parser, &expr)) return false;

    if (!require(parser, TK_COLON, &keyword, "case-statement", "expression"))
        return false;

    statement_t inner;
    if (!parse_statement(parser, &inner)) return false;

    *stmt = (statement_t) {
        .kind = STATEMENT_CASE,
        .terminator = inner.terminator,
        .value.case_ = {
            .expression = malloc(sizeof(expression_t)),
            .statement = malloc(sizeof(statement_t)),
        },
    };
    *stmt->value.case_.expression = expr;
    *stmt->value.case_.statement = inner;
    return true;
}

bool parse_default_case_statement(parser_t *parser, statement_t *stmt) {
    token_t *keyword;
    if (!require(parser, TK_DEFAULT, &keyword, "default-case-statement", NULL))
        return false;

    if (!require(parser, TK_COLON, &keyword, "default-case-statement", NULL))
        return false;

    statement_t inner;
    if (!parse_statement(parser, &inner)) return false;

    *stmt = (statement_t) {
        .kind = STATEMENT_CASE,
        .terminator = inner.terminator,
        .value.case_ = {
            .keyword = keyword,
            .expression = NULL,
            .statement = malloc(sizeof(statement_t)),
        },
    };
    *stmt->value.case_.statement = inner;
    return true;
}

bool parse_expression_statement(parser_t *parser, statement_t *stmt) {
    token_t *terminator; // hasta la vista baby
    expression_t *expr = malloc(sizeof(expression_t));
    if (!parse_expression(parser, expr)) {
        free(expr);
        return false;
    }

    if (!require(parser, TK_SEMICOLON, &terminator, "statement", "expression")) {
        free(expr);
        return false;
    }

    *stmt = (statement_t) {
        .kind = STATEMENT_EXPRESSION,
        .value.expression = expr,
        .terminator = terminator,
    };

    return true;
}


// Expressions

bool parse_expression(parser_t *parser, expression_t *node) {
    if (!parse_assignment_expression(parser, node)) {
        return false;
    }

    token_t *token = NULL;
    while (accept(parser, TK_COMMA, &token)) {
        expression_t *right = malloc(sizeof(expression_t));
        if (!parse_assignment_expression(parser, right)) {
            free(right);
            return false;
        }

        expression_t *left = malloc(sizeof(expression_t));
        *left = *node;
        *node = (expression_t) {
            .span = spanning(left->span.start, right->span.end),
            .kind = EXPRESSION_BINARY,
            .value.binary = {
                .kind = BINARY_COMMA,
                .left = left,
                .right = right,
                .operator_token = token,
            }
        };
    }

    return true;
}

/**
 * Parses an assignment expression.
 *
 * <assignment-expression> ::= <conditional-expression>
 *                           | <unary-expression> <assignment-operator> <assignment-expression>
 *
 * @param parser Parser instance
 * @param expr Expression node to store the result in
 * @return false if an error occurred, true otherwise
 */
bool parse_assignment_expression(parser_t *parser, expression_t *expr) {
    if (!parse_conditional_expression(parser, expr)) {
        return false;
    }
 
    token_t *token;
    if (accept(parser, TK_ASSIGN, &token) ||
        accept(parser, TK_BITWISE_XOR_ASSIGN, &token) ||
        accept(parser, TK_MULTIPLY_ASSIGN, &token) ||
        accept(parser, TK_MULTIPLY_ASSIGN, &token) ||
        accept(parser, TK_DIVIDE_ASSIGN, &token) ||
        accept(parser, TK_MOD_ASSIGN, &token) ||
        accept(parser, TK_PLUS_ASSIGN, &token) ||
        accept(parser, TK_MINUS_ASSIGN, &token) ||
        accept(parser, TK_LSHIFT_ASSIGN, &token) ||
        accept(parser, TK_RSHIFT_ASSIGN, &token) ||
        accept(parser, TK_BITWISE_AND_ASSIGN, &token) ||
        accept(parser, TK_BITWISE_OR_ASSIGN, &token) ||
        accept(parser, TK_BITWISE_XOR_ASSIGN, &token)) {

        expression_t *right = malloc(sizeof(expression_t));
        if (!parse_assignment_expression(parser, right)) {
            free(right);
            return false;
        }
        
        binary_assignment_operator_t assignment_operator;
        if (token->kind == TK_ASSIGN) {
            assignment_operator = BINARY_ASSIGN;
        } else if (token->kind == TK_BITWISE_AND_ASSIGN) {
            assignment_operator = BINARY_BITWISE_AND_ASSIGN;
        } else if (token->kind == TK_BITWISE_OR_ASSIGN) {
            assignment_operator = BINARY_BITWISE_OR_ASSIGN;
        } else if (token->kind == TK_BITWISE_XOR_ASSIGN) {
            assignment_operator = BINARY_BITWISE_XOR_ASSIGN;
        } else if (token->kind == TK_MULTIPLY_ASSIGN) {
            assignment_operator = BINARY_MULTIPLY_ASSIGN;
        } else if (token->kind == TK_DIVIDE_ASSIGN) {
            assignment_operator = BINARY_DIVIDE_ASSIGN;
        } else if (token->kind == TK_MOD_ASSIGN) {
            assignment_operator = BINARY_MODULO_ASSIGN;
        } else if (token->kind == TK_PLUS_ASSIGN) {
            assignment_operator = BINARY_ADD_ASSIGN;
        } else if (token->kind == TK_MINUS_ASSIGN) {
            assignment_operator = BINARY_SUBTRACT_ASSIGN;
        } else if (token->kind == TK_LSHIFT_ASSIGN) {
            assignment_operator = BINARY_SHIFT_LEFT_ASSIGN;
        } else if (token->kind == TK_RSHIFT_ASSIGN) {
            assignment_operator = BINARY_SHIFT_RIGHT_ASSIGN;
        } else {
            // This should never be reached
            fprintf(stderr, "%s:%d: Invalid assignment operator %s", __FILE__, __LINE__, token->value);
            exit(1);
        }

        expression_t *left = malloc(sizeof(expression_t));
        *left = *expr;
        *expr = (expression_t) {
            .span = spanning(left->span.start, right->span.end),
            .kind = EXPRESSION_BINARY,
            .value.binary = {
                .kind = BINARY_ASSIGNMENT,
                .left = left,
                .right = right,
                .operator_token = token,
                .operator.assignment = assignment_operator,
            }
        };
    }

    return true;
}

/**
 * Parses a conditional (ternary) expression.
 *
 * <conditional-expression> ::= <logical-or-expression>
 *                            | <logical-or-expression> '?' <expression> ':' <conditional-expression>
 *
 * @param parser Parser instance
 * @param expr Expression node to store the result in
 * @return false if an error occurred, true otherwise
 */
bool parse_conditional_expression(parser_t *parser, expression_t *expr) {
    if (!parse_logical_or_expression(parser, expr)) {
        return false;
    }

    token_t *token;
    if (accept(parser, TK_TERNARY, &token)) {
        expression_t *condition = malloc(sizeof(expression_t));
        *condition = *expr;

        expression_t *true_expression = malloc(sizeof(expression_t));
        if (!parse_expression(parser, true_expression)) {
            free(condition);
            free(true_expression);
            return false;
        }

        if (!require(parser, TK_COLON, NULL, "conditional-expression", "expression")) {
            free(condition);
            free(true_expression);
            return false;
        }

        expression_t *false_expression = malloc(sizeof(expression_t));
        if (!parse_conditional_expression(parser, false_expression)) {
            free(condition);
            free(true_expression);
            free(false_expression);
            return false;
        }

        *expr = (expression_t) {
            .span = spanning(expr->span.start, false_expression->span.end),
            .kind = EXPRESSION_TERNARY,
            .value.ternary = {
                .condition = condition,
                .true_expression = true_expression,
                .false_expression = false_expression,
            }
        };
    }

    return true;
}

/**
 * Parses a logical or expression.
 *
 * <logical-or-expression> ::= <logical-and-expression>
 *                           | <logical-or-expression> '||' <logical-and-expression>
 *
 * After rewriting to eliminate left-recursion:
 * <logical-or-expression> ::= <logical-and-expression> <logical-or-expression'>*
 * <logical-or-expression'> ::= '||' <logical-and-expression>
 *
 * @param parser Parser instance
 * @param expr Expression node to store the result in
 * @return false if an error occurred, true otherwise
 */
bool parse_logical_or_expression(parser_t *parser, expression_t *expr) {
    if (!parse_logical_and_expression(parser, expr)) {
        return false;
    }

    token_t *token;
    while (accept(parser, TK_LOGICAL_OR, &token)) {
        expression_t *right = malloc(sizeof(expression_t));
        if (!parse_logical_and_expression(parser, right)) {
            free(right);
            return false;
        }

        expression_t *left = malloc(sizeof(expression_t));
        *left = *expr;
        *expr = (expression_t) {
            .span = spanning(left->span.start, right->span.end),
            .kind = EXPRESSION_BINARY,
            .value.binary = {
                .kind = BINARY_LOGICAL,
                .left = left,
                .right = right,
                .operator_token = token,
                .operator.logical = BINARY_LOGICAL_OR,
            }
        };
    }

    return true;
}

/**
 * Parses a logical and expression.
 *
 * <logical-and-expression> ::= <inclusive-or-expression>
                              | <logical-and-expression> '&&' <inclusive-or-expression>
 *
 * After rewriting to eliminate left-recursion:
 *
 * <logical-and-expression> ::= <inclusive-or-expression> <logical-and-expression'>*
 * <logical-and-expression'> ::= '&&' <inclusive-or-expression>
 *
 * @param parser Parser instance
 * @param expr Expression node to store the result in
 * @return false if an error occurred, true otherwise
 */
bool parse_logical_and_expression(parser_t *parser, expression_t *expr) {
    if (!parse_inclusive_or_expression(parser, expr)) {
        return false;
    }

    token_t *token;
    while (accept(parser, TK_LOGICAL_AND, &token)) {
        expression_t *right = malloc(sizeof(expression_t));
        if (!parse_inclusive_or_expression(parser, right)) {
            free(right);
            return false;
        }

        expression_t *left = malloc(sizeof(expression_t));
        *left = *expr;
        *expr = (expression_t) {
            .span = spanning(left->span.start, right->span.end),
            .kind = EXPRESSION_BINARY,
            .value.binary = {
                .kind = BINARY_LOGICAL,
                .left = left,
                .right = right,
                .operator_token = token,
                .operator.logical = BINARY_LOGICAL_AND,
            }
        };
    }

    return true;
}

/**
 * Parses an inclusive or expression.
 *
 * <inclusive-or-expression> ::= <exclusive-or-expression>
 *                             | <inclusive-or-expression> '|' <exclusive-or-expression>
 *
 * After rewriting to eliminate left-recursion:
 *
 * <inclusive-or-expression> ::= <exclusive-or-expression> <inclusive-or-expression'>*
 * <inclusive-or-expression'> ::= '|' <exclusive-or-expression> <inclusive-or-expression'>
 *
 * @param parser Parser instance
 * @param expr Expression node to store the result in
 * @return false if an error occurred, true otherwise
 */
bool parse_inclusive_or_expression(parser_t *parser, expression_t *expr) {
    if (!parse_exclusive_or_expression(parser, expr)) {
        return false;
    }

    token_t *token;
    while (accept(parser, TK_BITWISE_OR, &token)) {
        expression_t *right = malloc(sizeof(expression_t));
        if (!parse_exclusive_or_expression(parser, right)) {
            free(right);
            return false;
        }

        expression_t *left = malloc(sizeof(expression_t));
        *left = *expr;
        *expr = (expression_t) {
            .span = spanning(left->span.start, right->span.end),
            .kind = EXPRESSION_BINARY,
            .value.binary = {
                .kind = BINARY_BITWISE,
                .left = left,
                .right = right,
                .operator_token = token,
                .operator.bitwise = BINARY_BITWISE_OR,
            }
        };
    }

    return true;
}

/**
 * Parses an exclusive or expression.
 *
 * <exclusive-or-expression> ::= <and-expression>
 *                             | <exclusive-or-expression> '^' <and-expression>
 *
 * After rewriting to eliminate left-recursion:
 *
 * <exclusive-or-expression> ::= <and-expression> <exclusive-or-expression'>*
 * <exclusive-or-expression'> ::= '^' <and-expression>
 *
 * @param parser Parser instance
 * @param expr Expression node to store the result in
 * @return false if an error occurred, true otherwise
 */
bool parse_exclusive_or_expression(parser_t *parser, expression_t *expr) {
    if (!parse_and_expression(parser, expr)) {
        return false;
    }

    token_t *token;
    while (accept(parser, TK_BITWISE_XOR, &token)) {
        expression_t *right = malloc(sizeof(expression_t));
        if (!parse_equality_expression(parser, right)) {
            free(right);
            return false;
        }

        expression_t *left = malloc(sizeof(expression_t));
        *left = *expr;
        *expr = (expression_t) {
            .span = spanning(left->span.start, right->span.end),
            .kind = EXPRESSION_BINARY,
            .value.binary = {
                .kind = BINARY_BITWISE,
                .left = left,
                .right = right,
                .operator_token = token,
                .operator.bitwise = BINARY_BITWISE_XOR,
            }
        };
    }

    return true;
}

/**
 * Parses an and expression.
 *
 * <and-expression> ::= <equality-expression>
 *                    | <and-expression> '&' <equality-expression>
 *
 * After rewriting to eliminate left-recursion:
 *
 * <and-expression> ::= <equality-expression> <and-expression'>*
 * <and-expression'> ::= '&' <equality-expression>
 *
 * @param parser Parser instance
 * @param node Expression node to store the result in
 * @return false if an error occurred, true otherwise
 */
bool parse_and_expression(parser_t* parser, expression_t* expr) {
    if (!parse_equality_expression(parser, expr)) {
        return false;
    }

    token_t *token;
    while (accept(parser, TK_AMPERSAND, &token)) {
        expression_t *right = malloc(sizeof(expression_t));
        if (!parse_equality_expression(parser, right)) {
            free(right);
            return false;
        }

        expression_t *left = malloc(sizeof(expression_t));
        *left = *expr;
        *expr = (expression_t) {
            .span = spanning(left->span.start, right->span.end),
            .kind = EXPRESSION_BINARY,
            .value.binary = {
                .kind = BINARY_BITWISE,
                .left = left,
                .right = right,
                .operator_token = token,
                .operator.bitwise = BINARY_BITWISE_AND,
            }
        };
    }

    return true;
}

bool equality_expression_prime(parser_t *parser, expression_t *expr, const token_t *operator);

/**
 * Parses an equality expression.
 *
 * <equality-expression> ::= <relational-expression>
 *                        | <equality-expression> ('==' | '!=') <relational-expression>
 *
 * After rewriting to eliminate left-recursion:
 *
 * <equality-expression> ::= <relational-expression> <equality-expression'>*
 * <equality-expression'> ::= ('==' | '!=') <relational-expression>
 *
 * @param parser Parser instance
 * @param expr Expression node to store the result in
 * @return false if an error occurred, true otherwise
 */
bool parse_equality_expression(parser_t *parser, expression_t *expr) {
    if (!parse_relational_expression(parser, expr)) {
        return false;
    }

    token_t *token;
    if (accept(parser, TK_EQUALS, &token) || accept(parser, TK_NOT_EQUALS, &token)) {
        return equality_expression_prime(parser, expr, token);
    } else {
        return true;
    }
}

bool equality_expression_prime(parser_t *parser, expression_t *expr, const token_t *operator) {
    expression_t *right = malloc(sizeof(expression_t));
    if (!parse_relational_expression(parser, right)) {
        free(right);
        return false;
    }

    assert(operator->kind == TK_EQUALS || operator->kind == TK_NOT_EQUALS);
    binary_comparison_operator_t comparison_operator = operator->kind == TK_EQUALS ?
            BINARY_COMPARISON_EQUAL : BINARY_COMPARISON_NOT_EQUAL;

    expression_t *left = malloc(sizeof(expression_t));
    *left = *expr;
    *expr = (expression_t) {
        .span = spanning(left->span.start, right->span.end),
        .kind = EXPRESSION_BINARY,
        .value.binary = {
            .kind = BINARY_COMPARISON,
            .left = left,
            .right = right,
            .operator_token = operator,
            .operator.comparison = comparison_operator
        }
    };

    token_t *token;
    if (accept(parser, TK_EQUALS, &token) || accept(parser, TK_NOT_EQUALS, &token)) {
        return equality_expression_prime(parser, expr, token);
    } else {
        return true;
    }
}

bool relational_expression_prime(parser_t *parser, expression_t *expr, const token_t *operator);

/**
 * <relational-expression> ::= <shift-expression>
 *                          | <relational-expression> ('<' | '>' | '<=' | '>=') <shift-expression>
 *
 * After rewriting to eliminate left-recursion:
 *
 * <relational-expression> ::= <shift-expression> <relational-expression'>*
 * <relational-expression'> ::= ('<' | '>' | '<=' | '>=') <shift-expression>
 *
 * @param parser Parser instance
 * @param expr Expression node to store the result in
 * @return false if an error occurred, true otherwise
 */
bool parse_relational_expression(parser_t *parser, expression_t *expr) {
    if (!parse_shift_expression(parser, expr)) {
        return false;
    }

    token_t *token;
    if (accept(parser, TK_LESS_THAN, &token) ||
        accept(parser, TK_GREATER_THAN, &token) ||
        accept(parser, TK_LESS_THAN_EQUAL, &token) ||
        accept(parser, TK_GREATER_THAN_EQUAL, &token)) {
        return relational_expression_prime(parser, expr, token);
    } else {
        return true;
    }
}

bool relational_expression_prime(parser_t *parser, expression_t *expr, const token_t *operator) {
    expression_t *right = malloc(sizeof(expression_t));
    if (!parse_shift_expression(parser, right)) {
        free(right);
        return false;
    }

    assert(operator->kind == TK_LESS_THAN || operator->kind == TK_GREATER_THAN ||
           operator->kind == TK_LESS_THAN_EQUAL || operator->kind == TK_GREATER_THAN_EQUAL);
    binary_comparison_operator_t binary_operator;
    if (operator->kind == TK_LESS_THAN) {
        binary_operator = BINARY_COMPARISON_LESS_THAN;
    } else if (operator->kind == TK_LESS_THAN_EQUAL) {
        binary_operator = BINARY_COMPARISON_LESS_THAN_OR_EQUAL;
    } else if (operator->kind == TK_GREATER_THAN) {
        binary_operator = BINARY_COMPARISON_GREATER_THAN;
    } else {
        binary_operator = BINARY_COMPARISON_GREATER_THAN_OR_EQUAL;
    }

    expression_t *left = malloc(sizeof(expression_t));
    *left = *expr;
    *expr = (expression_t) {
        .span = spanning(left->span.start, right->span.end),
        .kind = EXPRESSION_BINARY,
        .value.binary = {
            .kind = BINARY_COMPARISON,
            .left = left,
            .right = right,
            .operator_token = operator,
            .operator.comparison = binary_operator,
        }
    };

    token_t *token;
    if (accept(parser, TK_LESS_THAN, &token) ||
        accept(parser, TK_LESS_THAN_EQUAL, &token) ||
        accept(parser, TK_GREATER_THAN, &token) ||
        accept(parser, TK_GREATER_THAN_EQUAL, &token)) {
        return relational_expression_prime(parser, expr, token);
    } else {
        return true;
    }
}

bool shift_expression_prime(parser_t *parser, expression_t *expr, const token_t *operator);

/**
 * <shift-expression> ::= <additive-expression>
 *                      | <shift-expression> '<<' <additive-expression>
 *                      | <shift-expression> '>>' <additive-expression>
 *
 * After rewriting to eliminate left-recursion:
 *
 * <shift-expression> ::= <additive-expression> <shift-expression'>*
 * <shift-expression'> ::= '<<' <additive-expression>
 *                      | '>>' <additive-expression>
 *
 * @param parser Parser instance
 * @param expr Expression node to store the resulting expression in
 * @return false if an error occurred, true otherwise
 */
bool parse_shift_expression(parser_t *parser, expression_t *expr) {
    if (!parse_additive_expression(parser, expr)) {
        return false;
    }

    token_t *token;
    if (accept(parser, TK_LSHIFT, &token) ||
        accept(parser, TK_RSHIFT, &token)) {
        return shift_expression_prime(parser, expr, token);
    } else {
        return true;
    }
}

bool shift_expression_prime(parser_t *parser, expression_t *expr, const token_t *operator) {
    expression_t *right = malloc(sizeof(expression_t));
    if (!parse_additive_expression(parser, right)) {
        free(right);
        return false;
    }

    assert(operator->kind == TK_LSHIFT || operator->kind == TK_RSHIFT);
    binary_bitwise_operator_t binary_operator = operator->kind == TK_LSHIFT ?
            BINARY_BITWISE_SHIFT_LEFT : BINARY_BITWISE_SHIFT_RIGHT;

    expression_t *left = malloc(sizeof(expression_t));
    *left = *expr;
    *expr = (expression_t) {
        .span = spanning(left->span.start, right->span.end),
        .kind = EXPRESSION_BINARY,
        .value.binary = {
            .kind = BINARY_BITWISE,
            .left = left,
            .right = right,
            .operator_token = operator,
            .operator.bitwise = binary_operator,
        }
    };

    token_t *token;
    if (accept(parser, TK_LSHIFT, &token) ||
        accept(parser, TK_RSHIFT, &token)) {
        return shift_expression_prime(parser, expr, token);
    } else {
        return true;
    }
}

bool additive_expression_prime(parser_t *parser, expression_t *expr, const token_t *operator);

/**
 * <additive-expression> ::= <multiplicative-expression>
 *                         | <additive-expression> '+' <multiplicative-expression>
 *                         | <additive-expression> '-' <multiplicative-expression>
 *
 * As this rule is left-recursive, it must be rewritten to eliminate the left-recursion while preserving the operator
 * associativity. The new rule becomes:
 *
 * <additive-expression> ::= <multiplicative-expression> <additive-expression'>*
 * <additive-expression'> ::= '+' <multiplicative-expression> <additive-expression'>
 *                         | '-' <multiplicative-expression> <additive-expression'>
 *
 * @param parser Parser instance
 * @param expr Expression node to store the resulting expression in
 * @return false if an error occurred, true otherwise
 */
bool parse_additive_expression(parser_t* parser, expression_t* expr) {
    if (!parse_multiplicative_expression(parser, expr)) {
        return false;
    }

    token_t *token;
    if (accept(parser, TK_PLUS, &token) || accept(parser, TK_MINUS, &token)) {
        return additive_expression_prime(parser, expr, token);
    } else {
        return true;
    }
}

bool additive_expression_prime(parser_t* parser, expression_t* expr, const token_t *operator) {
    expression_t *right = malloc(sizeof(expression_t));
    if (!parse_multiplicative_expression(parser, right)) {
        free(right);
        return false;
    }

    assert(operator->kind == TK_PLUS || operator->kind == TK_MINUS);
    binary_arithmetic_operator_t binary_operator =
            operator-> kind == TK_PLUS ? BINARY_ARITHMETIC_ADD : BINARY_ARITHMETIC_SUBTRACT;

    expression_t *left = malloc(sizeof(expression_t));
    *left = *expr;
    *expr = (expression_t) {
        .span = spanning(left->span.start, right->span.end),
        .kind = EXPRESSION_BINARY,
        .value.binary = {
            .kind = BINARY_ARITHMETIC,
            .left = left,
            .right = right,
            .operator_token = operator,
            .operator.arithmetic = binary_operator,
        }
    };

    token_t *token;
    if (accept(parser, TK_PLUS, &token) ||
        accept(parser, TK_MINUS, &token)) {
        return additive_expression_prime(parser, expr, token);
    } else {
        return true;
    }
}

bool multiplicative_expression_prime(parser_t* parser, expression_t* node, const token_t *operator);

/**
 * <multiplicative-expression> ::= <cast-expression>
 *                               | <multiplicative-expression> '*' <cast-expression>
 *                               | <multiplicative-expression> '/' <cast-expression>
 *                               | <multiplicative-expression> '%' <cast-expression>
 *
 * The rule is left-recursive and cannot be directly parsed using recursive descent.
 * We rewrite it to eliminate the left-recursion, while preserving the operator associativity:
 *
 * <multiplicative-expression> ::= <cast-expression> <multiplicative-expression'>*
 * <multiplicative-expression'> ::= '*' <cast-expression> <multiplicative-expression'>
 *                                | '/' <cast-expression> <multiplicative-expression'>
 *                                | '%' <cast-expression> <multiplicative-expression'>
 *
 * @param parser Parser instance
 * @param expr Expression node to store the resulting expression in
 * @return false if an error occurred, true otherwise
 */
bool parse_multiplicative_expression(parser_t* parser, expression_t* expr) {
    if (!parse_cast_expression(parser, expr)) {
        return false;
    }

    token_t *token;
    if (accept(parser, TK_STAR, &token) ||
        accept(parser, TK_SLASH, &token) ||
        accept(parser, TK_PERCENT, &token)) {
        return multiplicative_expression_prime(parser, expr, token);
    } else {
        return true;
    }
}

bool multiplicative_expression_prime(parser_t* parser, expression_t* expr, const token_t *operator) {
    expression_t *right = malloc(sizeof(expression_t));
    if (!parse_cast_expression(parser, right)) {
        free(right);
        return false;
    }

    assert(operator->kind == TK_STAR || operator->kind == TK_SLASH || operator->kind == TK_PERCENT);
    binary_arithmetic_operator_t binary_operator =
            operator-> kind == TK_STAR ? BINARY_ARITHMETIC_MULTIPLY :
            operator-> kind == TK_SLASH ? BINARY_ARITHMETIC_DIVIDE :
            BINARY_ARITHMETIC_MODULO;

    expression_t *left = malloc(sizeof(expression_t));
    *left = *expr;
    *expr = (expression_t) {
        .span = spanning(left->span.start, right->span.end),
        .kind = EXPRESSION_BINARY,
        .value.binary = {
            .left = left,
            .right = right,
            .operator_token = operator,
            .operator.arithmetic = binary_operator,
        }
    };

    token_t *token;
    if (accept(parser, TK_STAR, &token) ||
        accept(parser, TK_SLASH, &token) ||
        accept(parser, TK_PERCENT, &token)) {
        return multiplicative_expression_prime(parser, expr, token);
    } else {
        return true;
    }
}

bool parse_cast_expression(parser_t *parser, expression_t *expr) {
    token_t *token = NULL;

    // Look ahead to see if this could possibly be a cast expression
    // We expect to see '(' followed by a type name
    // A special case is '(' followed by an identifier, which could either be a primary expression, or could be a
    // cast expression if that identifier is a typedef-name (which we have to look up in the symbol table).
    //
    // Note: '(' <type-name> ')' could also be the start of a compound literal. We need to reach the following token to
    // decide (if it's a '{', it must be a compound literal or a syntax error).
    // This is very ugly
    parse_checkpoint_t checkpoint = create_checkpoint(parser);
    type_t *type = NULL;
    bool is_cast = accept(parser, TK_LPAREN, NULL) && parse_type_name(parser, &type) &&
                   accept(parser, TK_RPAREN, NULL) && !peek(parser, TK_LBRACE);
    backtrack(parser, checkpoint);

    if (is_cast) {
        accept(parser, TK_LPAREN, &token);
        if (!parse_type_name(parser, &type)) {
            if (type != NULL) {
                free(type);
            }
            return false;
        }

        if (!require(parser, TK_RPAREN, NULL, "cast-expression", "type-name")) {
            free(type);
            return false;
        }

        expression_t *operand = malloc(sizeof(expression_t));
        if (!parse_cast_expression(parser, operand)) {
            free(operand);
            free(type);
            return false;
        }

        *expr = (expression_t) {
            .span = SPANNING_NEXT(token),
            .kind = EXPRESSION_CAST,
            .value.cast = {
                .type = type,
                .expression = operand,
            },
        };

        return true;
    } else {
        return parse_unary_expression(parser, expr);
    }
}

// helper function to parse: <unary-operator> <cast-expression>
bool unary_op(parser_t *parser, expression_t* expr, token_t *token) {
    expression_t *operand = malloc(sizeof(expression_t));
    if (!parse_cast_expression(parser, operand)) {
        free(operand);
        return false;
    }

    int operator;
    switch (token->kind) {
        case TK_AMPERSAND:
            operator = UNARY_ADDRESS_OF;
            break;
        case TK_STAR:
            operator = UNARY_DEREFERENCE;
            break;
        case TK_PLUS:
            operator = UNARY_PLUS;
            break;
        case TK_MINUS:
            operator = UNARY_MINUS;
            break;
        case TK_BITWISE_NOT:
            operator = UNARY_BITWISE_NOT;
            break;
        case TK_EXCLAMATION:
            operator = UNARY_LOGICAL_NOT;
            break;
        default:
            assert(false);
    }

    *expr = (expression_t) {
        .span = SPANNING_NEXT(token),
        .kind = EXPRESSION_UNARY,
        .value.unary = {
            .operator = operator,
            .operand = operand,
            .token = token,
        },
    };
    return true;
}

bool parse_unary_expression(parser_t *parser, expression_t *expr) {
    token_t *token;

    if (accept(parser, TK_INCREMENT, &token)) {
        expression_t *operand = malloc(sizeof(expression_t));
        if (!parse_unary_expression(parser, operand)) {
            free(operand);
            return false;
        }

        *expr = (expression_t) {
            .span = SPANNING_NEXT(token),
            .kind = EXPRESSION_UNARY,
            .value.unary = {
                .operator = UNARY_PRE_INCREMENT,
                .operand = operand,
                .token = token,
            },
        };
        return true;
    } else if (accept(parser, TK_DECREMENT, &token)) {
        expression_t *operand = malloc(sizeof(expression_t));
        if (!parse_unary_expression(parser, operand)) {
            free(operand);
            return false;
        }

        *expr = (expression_t) {
            .span = SPANNING_NEXT(token),
            .kind = EXPRESSION_UNARY,
            .value.unary = {
                .operator = UNARY_PRE_DECREMENT,
                .operand = operand,
                .token = token,
            }
        };
        return true;
    } else if (accept(parser, TK_AMPERSAND, &token) ||
               accept(parser, TK_STAR, &token) ||
               accept(parser, TK_PLUS, &token) ||
               accept(parser, TK_MINUS, &token) ||
               accept(parser, TK_BITWISE_NOT, &token) ||
               accept(parser, TK_EXCLAMATION, &token)) {
        return unary_op(parser, expr, token);
    } else if (accept(parser, TK_SIZEOF, &token)) {
        if (peek(parser, TK_LPAREN)) {
            // Ambiguous, could be sizeof(type) or sizeof expression, as a primary expression also starts with a '('.
            // Try both and return the first that succeeds.
            parse_checkpoint_t checkpoint = create_checkpoint(parser);

            expression_t *inner = malloc(sizeof(expression_t));
            if (parse_unary_expression(parser, inner)) {
                *expr = (expression_t) {
                    .span = SPANNING_NEXT(token),
                    .kind = EXPRESSION_UNARY,
                    .value.unary = {
                        .operator = UNARY_SIZEOF,
                        .operand = inner,
                        .token = token,
                    },
                };
                return true;
            }

            free(inner); // Cleanup

            // Restore the parser state and try the other alternative: '(' <type-name> ')'
            backtrack(parser, checkpoint);

            // We know the next token is '(', since we rolled back the parser state.
            accept(parser, TK_LPAREN, NULL);

            type_t *type = NULL;
            if (!parse_type_name(parser, &type)) {
                free(type);
                append_parse_error(&parser->errors, (parse_error_t) {
                    .kind = PARSE_ERROR_EXPECTED_EXPRESSION_OR_TYPE_NAME_AFTER_SIZEOF,
                    .production_name = "unary-expression",
                    .previous_production_name = NULL,
                    .token = token,
                });
                return false;
            }

            if (!require(parser, TK_RPAREN, NULL, "unary-expression", "type-name")) {
                free(type);
                return false;
            }

            *expr = (expression_t) {
                .span = SPANNING_NEXT(token),
                .kind = EXPRESSION_SIZEOF,
                .value.type = type,
            };
            return true;
        } else {
            // Must be 'sizeof' <unary-expression>
            expression_t *inner = malloc(sizeof(expression_t));
            if (!parse_unary_expression(parser, inner)) {
                free(inner);
                return false;
            }

            *expr = (expression_t) {
                .span = SPANNING_NEXT(token),
                .kind = EXPRESSION_UNARY,
                .value.unary = {
                    .operator = UNARY_SIZEOF,
                    .operand = inner,
                    .token = token,
                },
            };
            return true;
        }
    } else {
        return parse_postfix_expression(parser, expr);
    }
}

bool parse_postfix_expression(parser_t *parser, expression_t *expr) {
    // Can either be a primary expression, followed by:
    // 1. array index         - '[' <expression> ']')
    // 2. Function call       - '(' <argument-expression-list>? ')'
    // 3. Member access       - '.' <identifier>
    // 4. Member access (ptr) - '->' <identifier>
    // 5. Increment           - '++'
    // 6. Decrement           - '--'
    // Alternatively, can be:
    // 1. Compound literal    - '(' <type-name> ')' '{' <initializer-list> ',' '}'

    // Try the compound literal first, to avoid excess lookahead/backtracking, as we only need a few tokens
    // TODO: don't use backtracking here, just use 2 tokens of lookahead, provide better syntax error
    parse_checkpoint_t checkpoint = create_checkpoint(parser);
    token_t *compound_lit_start = NULL;
    type_t *compound_lit_type = NULL;
    if (accept(parser, TK_LPAREN, &compound_lit_start) && parse_type_name(parser, &compound_lit_type) && accept(parser, TK_RPAREN, NULL) && accept(parser, TK_LBRACE, NULL)) {
        initializer_list_t initializer_list;
        if (!parse_initializer_list(parser, &initializer_list)) {
            return false;
        }
        require(parser, TK_RBRACE, NULL, "postfix-expression", "initializer-list");
        *expr = (expression_t) {
                .span = SPAN_STARTING(compound_lit_start->position),
                .kind = EXPRESSION_COMPOUND_LITERAL,
                .value.compound_literal = {
                        .type = compound_lit_type,
                        .initializer_list = initializer_list
                },
        };
        return true;
    } else {
        backtrack(parser, checkpoint);
    };

    expression_t *primary = malloc(sizeof(expression_t));
    if (!parse_primary_expression(parser, primary)) {
        free(primary);
        return false;
    }

    expression_t *current = primary;
    while (TOKEN_KIND_ONE_OF(next_token(parser)->kind, TK_LBRACKET, TK_LPAREN, TK_DOT, TK_ARROW, TK_INCREMENT, TK_DECREMENT)) {
        token_t *token = NULL;
        if (accept(parser, TK_LBRACKET, NULL)) {
            // array indexing
            expression_t *index = malloc(sizeof(expression_t));
            if (!parse_expression(parser, index)) {
                free(index);
                return false;
            }

            if (!require(parser, TK_RBRACKET, NULL, "postfix-expression", "expression")) {
                free(index);
                return false;
            }

            expression_t *array = current;
            current = malloc(sizeof(expression_t));
            *current = (expression_t) {
                .span = spanning(array->span.start, *current_position(parser)),
                .kind = EXPRESSION_ARRAY_SUBSCRIPT,
                .value.array_subscript = {
                    .array = array,
                    .index = index,
                },
            };
        } else if (accept(parser, TK_LPAREN, NULL)) {
            // function call
            // parse argument list
            ptr_vector_t arguments = {.size = 0, .capacity = 0, .buffer = NULL};
            while (next_token(parser) ->kind != TK_RPAREN && next_token(parser)->kind != TK_EOF) {
                expression_t *argument = malloc(sizeof(expression_t));

                // special handling for va_arg(va_list, type) (__builtin_va_arg)
                // second argument is a type, which is not allowed by the grammar
                if (arguments.size == 1 && primary->kind == EXPRESSION_PRIMARY &&
                    primary->value.primary.kind == PE_IDENTIFIER &&
                    strcmp(primary->value.primary.value.token.value, "__builtin_va_arg") == 0) {
                    type_t *type = NULL;
                    if (!parse_type_name(parser, &type)) {
                        free(argument);
                        // TODO: cleanup
                        return false;
                    }
                    *argument = (expression_t) {
                        .span = SPAN_STARTING(*current_position(parser)),
                        .kind = EXPRESSION_TYPE,
                        .value.type = type,
                    };
                } else {
                    if (!parse_assignment_expression(parser, argument)) {
                        free(argument);
                        // TODO: cleanup arguments
                        return false;
                    }
                }

                append_ptr(&arguments.buffer, &arguments.size, &arguments.capacity, argument);

                if (!accept(parser, TK_COMMA, NULL)) {
                    break;
                }
            }

            if (!require(parser, TK_RPAREN, NULL, "argument-expression-list", "")) {
                // TODO: cleanup
                return false;
            }

            expression_t *callee = current;
            current = malloc(sizeof(expression_t));

            *current = (expression_t) {
                .span = spanning(callee->span.start, *current_position(parser)),
                .kind = EXPRESSION_CALL,
                .value.call = {
                    .callee = callee,
                    .arguments = arguments,
                },
            };
        } else if (accept(parser, TK_DOT, &token) || accept(parser, TK_ARROW, &token)) {
            // struct member access
            token_t *identifier;
            if (!require(parser, TK_IDENTIFIER, &identifier, "postfix-expression", "expression")) {
                return false;
            }

            expression_t *struct_or_union = current;
            current = malloc(sizeof(expression_t));

            *current = (expression_t) {
                .span = spanning(struct_or_union->span.start, *current_position(parser)),
                .kind = EXPRESSION_MEMBER_ACCESS,
                .value.member_access = {
                    .struct_or_union = struct_or_union,
                    .operator = *token,
                    .member = *identifier,
                },
            };
        } else if (accept(parser, TK_INCREMENT, NULL)) {
            // post-increment
            expression_t *operand = current;
            current = malloc(sizeof(expression_t));
            *current = (expression_t) {
                .span = SPAN_STARTING(operand->span.start),
                .kind = EXPRESSION_UNARY,
                .value.unary = {
                    .operator = UNARY_POST_INCREMENT,
                    .operand = operand,
                    .token = token,
                },
            };
        } else if (accept(parser, TK_DECREMENT, NULL)) {
            expression_t *operand = current;
            current = malloc(sizeof(expression_t));
            *current = (expression_t) {
                .span = SPAN_STARTING(operand->span.start),
                .kind = EXPRESSION_UNARY,
                .value.unary = {
                    .operator = UNARY_POST_DECREMENT,
                    .operand = operand,
                    .token = token,
                },
            };
        }
    }

    *expr = *current;
    free(current);

    return true;
}

bool parse_primary_expression(parser_t* parser, expression_t* expr) {
    token_t *token;
    source_position_t start = *current_position(parser);

    if (!typedef_name(parser, true, NULL, NULL) && accept(parser, TK_IDENTIFIER, &token)) {
        *expr = (expression_t) {
            .span = SPAN_STARTING(start),
            .kind = EXPRESSION_PRIMARY,
            .value.primary = {
                .kind = PE_IDENTIFIER,
                .value.token = *token,
            }
        };
        return true;
    } else if (accept(parser, TK_INTEGER_CONSTANT, &token) ||
               accept(parser, TK_FLOATING_CONSTANT, &token) ||
               accept(parser, TK_CHAR_LITERAL, &token)) {
        *expr = (expression_t) {
            .span = SPAN_STARTING(start),
            .kind = EXPRESSION_PRIMARY,
            .value.primary = {
                .kind = PE_CONSTANT,
                .value.token = *token,
            },
        };
        return true;
    } else if (accept(parser, TK_STRING_LITERAL, &token)) {
        *expr = (expression_t) {
            .span = SPAN_STARTING(start),
            .kind = EXPRESSION_PRIMARY,
            .value.primary = {
                .kind = PE_STRING_LITERAL,
                .value.token = *token,
            },
        };
        return true;
    } else if (accept(parser, TK_LPAREN, &token)) {
        expression_t *expr2 = malloc(sizeof(expression_t));
        if (!parse_expression(parser, expr2)) {
            free(expr2);
            return false;
        }

        if (!require(parser, TK_RPAREN, NULL, "primary-expression", "expression")) {
            free(expr2);
            return false;
        }

        *expr = (expression_t) {
            .span = spanning(start, *current_position(parser)),
            .kind = EXPRESSION_PRIMARY,
            .value.primary = {
                .kind = PE_EXPRESSION,
                .value.expression = expr2,
            },
        };

        return true;
    } else {
        token_t *prev_token = parser->next_token_index > 0 ? parser->tokens.buffer[parser->next_token_index - 1] : NULL;
        parse_error_t error = (parse_error_t) {
            .token = next_token(parser),
            .previous_token = prev_token,
            .production_name = "primary-expression",
            .previous_production_name = NULL,
            .kind = PARSE_ERROR_EXPECTED_TOKEN,
            .value.expected_token = {
                .expected_count = 6,
                .expected = {TK_IDENTIFIER, TK_INTEGER_CONSTANT, TK_FLOATING_CONSTANT, TK_CHAR_LITERAL, TK_STRING_LITERAL, TK_LPAREN },
            },
        };
        append_parse_error(&parser->errors, error);
        return false;
    }
}

// External definitions

/**
 * Parses an external declaration (declaration or a function definition).
 *
 * <external-declaration> ::= <function-definition>
 *                          | <declaration>
 *
 * @param parser
 * @param external_declaration
 * @return
 */
bool parse_external_declaration(parser_t *parser, external_declaration_t *external_declaration) {
    type_t type;
    if (!parse_declaration_specifiers(parser, &type)) {
        return false;
    }

    if (accept(parser, TK_SEMICOLON, NULL)) {
        type_t *type_ptr = malloc(sizeof(type_t));
        *type_ptr = type;
        declaration_t *declaration = malloc(sizeof(declaration_t));
        *declaration = (declaration_t) {
            .type = type_ptr,
            .identifier = NULL,
            .initializer = NULL,
        };
        declaration_t **list = malloc(sizeof (declaration_t**));
        list[0] = declaration;
        *external_declaration = (external_declaration_t) {
            .kind = EXTERNAL_DECLARATION_DECLARATION,
            .value.declaration = {
                .declarations = list,
                .length = 1,
            }
        };
        // Empty declaration (e.g. `int;`)
        return true;
    }

    declaration_t *decl = malloc(sizeof(declaration_t));
    if (!parse_declarator(parser, type, decl)) {
        return false;
    }

    token_t *body_start = NULL;
    if (decl->type->kind == TYPE_FUNCTION && accept(parser, TK_LBRACE, &body_start)) {
        // This is a function definition
        // register symbol in the parser symbol table for the function
        parser_symbol_t *symbol = malloc(sizeof(parser_symbol_t));
        *symbol = (parser_symbol_t) {
            .kind = SYMBOL_IDENTIFIER,
            .token = decl->identifier,
            .next_token_index = parser->next_token_index,
            .type = NULL,
        };
        parser_insert_symbol(parser, symbol);

        function_definition_t *fn = malloc(sizeof(function_definition_t));
        if (!parse_function_definition(parser, decl, body_start, fn)) {
            free(fn);
            return false;
        }
        *external_declaration = (external_declaration_t) {
            .kind = EXTERNAL_DECLARATION_FUNCTION_DEFINITION,
            .value.function_definition = fn,
        };
    } else {
        // This is a declaration
        ptr_vector_t declarations = {.size = 0, .capacity = 0, .buffer = NULL};
        if (!_parse_declaration(parser, decl, &declarations)) {
            return false;
        }

        *external_declaration = (external_declaration_t) {
            .kind = EXTERNAL_DECLARATION_DECLARATION,
            .value.declaration = {
                .declarations = (declaration_t**) declarations.buffer,
                .length = declarations.size,
            },
        };
    }

    return true;
}

bool parse_function_definition(parser_t *parser, const declaration_t *declarator, const token_t *body_start, function_definition_t *fn) {
    // Enter the function scope and add the parameters to the symbol table
    parser_enter_scope(parser);
    for (int i = 0; i < declarator->type->value.function.parameter_list->length; i += 1) {
        parameter_declaration_t *param = declarator->type->value.function.parameter_list->parameters[i];
        parser_symbol_t *symbol = malloc(sizeof(parser_symbol_t));
        *symbol = (parser_symbol_t) {
            .kind = SYMBOL_IDENTIFIER,
            .next_token_index = parser->next_token_index,
            .token = param->identifier,
            .type = NULL,
        };
        parser_insert_symbol(parser, symbol);
    }

    statement_t *body = malloc(sizeof(statement_t));
    if (!parse_compound_statement(parser, body, body_start)) {
        parser_leave_scope(parser);
        free(body);
        return false;
    }

    *fn = (function_definition_t) {
        .identifier = declarator->identifier,
        .return_type = declarator->type->value.function.return_type,
        .parameter_list = declarator->type->value.function.parameter_list,
        .body = body,
    };
    parser_leave_scope(parser);
    return true;
}
