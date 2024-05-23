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

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "parser.h"
#include "lexer.h"

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

void print_parse_error(FILE *__restrict stream, parse_error_t *error) {
    source_position_t position = error->token->position;
    fprintf(stream, "%s:%d:%d: error: ", position.path, position.line, position.column);
    switch (error->type) {
        case PARSE_ERROR_EXPECTED_TOKEN:
            fprintf(stream, error->expected_token.expected_count > 1 ? "expected one of " : "expected ");
            for (size_t i = 0; i < error->expected_token.expected_count; i++) {
                fprintf(stream, "%s", token_kind_display_names[error->expected_token.expected[i]]);
                if (i < error->expected_token.expected_count - 1) {
                    fprintf(stream, ", ");
                } else if (i == error->expected_token.expected_count - 2) {
                    fprintf(stream, " or ");
                }
            }

            if (error->previous_production_name != NULL ) {
                fprintf(stream, " after %s\n", error->previous_production_name);
            }
            break;
        case PARSE_ERROR_UNEXPECTED_END_OF_INPUT:
            fprintf(stream, "Unexpected end of input\n");
            fprintf(stream, "Expected token: %s\n", token_kind_display_names[error->unexpected_end_of_input.expected]);
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

void append_parse_error(parse_error_vector_t* vec, parse_error_t error) {
    VEC_APPEND(vec, error);
}

parser_t pinit(lexer_t lexer) {
    return (parser_t) {
            .lexer = lexer,
            .tokens = {.size = 0, .capacity = 0, .buffer = NULL},
            .errors = {.size = 0, .capacity = 0, .buffer = NULL},
            .next_token_index = 0,
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
                    .type = PARSE_ERROR_UNEXPECTED_END_OF_INPUT,
                    .unexpected_end_of_input = {
                            .expected = kind,
                    },
            };
        } else {
            error = (parse_error_t) {
                    .token = token,
                    .previous_token = previous_token,
                    .production_name = production_name,
                    .previous_production_name = previous_production_name,
                    .type = PARSE_ERROR_EXPECTED_TOKEN,
                    .expected_token = {
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
        // This is legal, but useless.
        // TODO: warning for empty declaration
        return true;
    }

    // If we didn't find a semicolon, then we need to attempt to parse an <init-declarator-list>.
    if (first_declarator != NULL) {
        // We've already parsed a declarator.
        // Check if we still need to parse an initializer, then parse the rest of the init-declarator-list.
        if (accept(parser, TK_ASSIGN, NULL)) {
            expression_t *expr = malloc(sizeof(expression_t));
            if (!parse_assignment_expression(parser, expr)) {
                // TODO: error recovery
                free(expr);
                return false;
            }
            first_declarator->initializer = expr;
        }
        append_ptr(&declarations->buffer, &declarations->size, &declarations->capacity, first_declarator);

        if (!accept(parser, TK_COMMA, NULL)) {
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
            .type = PARSE_ERROR_ILLEGAL_DECLARATION_SPECIFIERS,
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
 * TODO: The parsing of structs, unions, and enums is not yet implemented.
 * TODO: Inlining is not yet implemented, and the keyword will be silently ignored.
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
    token_t *struct_ = NULL;
    token_t *union_ = NULL;
    token_t *enum_ = NULL;

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
                            .type = PARSE_ERROR_ILLEGAL_DECLARATION_SPECIFIERS,
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
                    .type = PARSE_ERROR_ILLEGAL_USE_OF_RESTRICT,
            });
        } else if (accept(parser, TK_VOLATILE, &token)) {
            is_volatile = true;
        } else if (accept(parser, TK_VOID, &token)) {
            token_t *conflict = (token_t *) FIRST_NON_NULL(void_, bool_, char_, short_, int_, long_, long_long, float_,
                                                           double_, signed_, unsigned_, complex_, struct_, union_, enum_);
            if (void_ != NULL) {
                append_parse_error(&parser->errors, illegal_declaration_specifiers(token, void_));
            } else {
                void_ = token;
            }
        } else if (accept(parser, TK_CHAR, &token)) {
            token_t *conflict = (token_t *) FIRST_NON_NULL(void_, bool_, char_, short_, int_, long_, long_long, float_,
                                                           double_, complex_, struct_, union_, enum_);
            if (conflict != NULL) {
                append_parse_error(&parser->errors, illegal_declaration_specifiers(token, conflict));
            } else {
                char_ = token;
            }
        } else if (accept(parser, TK_SHORT, &token)) {
            token_t *conflict = (token_t *) FIRST_NON_NULL(void_, char_, short_, long_, long_long, float_, double_,
                                                           bool_, complex_, struct_, union_, enum_);
            if (conflict != NULL) {
                append_parse_error(&parser->errors, illegal_declaration_specifiers(token, conflict));
            } else {
                short_ = token;
            }
        } else if (accept(parser, TK_INT, &token)) {
            token_t *conflict = (token_t *) FIRST_NON_NULL(void_, bool_, int_, float_, double_, complex_, struct_, union_, enum_);
            if (conflict != NULL) {
                append_parse_error(&parser->errors, illegal_declaration_specifiers(token, conflict));
            } else {
                int_ = token;
            }
        } else if (accept(parser, TK_LONG, &token)) {
            token_t *conflict = (token_t *) FIRST_NON_NULL(void_, bool_, long_long, float_, double_, struct_, union_, enum_);
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
                                                           signed_, unsigned_, struct_, union_, enum_);
            if (conflict != NULL) {
                append_parse_error(&parser->errors, illegal_declaration_specifiers(token, conflict));
            } else {
                float_ = token;
            }
        } else if (accept(parser, TK_DOUBLE, &token)) {
            token_t *conflict = (token_t *) FIRST_NON_NULL(void_, bool_, char_, short_, int_, long_long, float_,
                                                           double_, signed_, unsigned_, struct_, union_, enum_);
            if (conflict != NULL) {
                append_parse_error(&parser->errors, illegal_declaration_specifiers(token, conflict));
            } else {
                double_ = token;
            }
        } else if (accept(parser, TK_SIGNED, &token)) {
            token_t *conflict = (token_t *) FIRST_NON_NULL(void_, bool_, float_, double_, signed_, unsigned_, struct_, union_, enum_);
            if (conflict != NULL) {
                append_parse_error(&parser->errors, illegal_declaration_specifiers(token, conflict));
            } else {
                signed_ = token;
            }
        } else if (accept(parser, TK_UNSIGNED, &token)) {
            token_t *conflict = (token_t *) FIRST_NON_NULL(void_, bool_, float_, double_, signed_, unsigned_, struct_, union_, enum_);
            if (conflict != NULL) {
                append_parse_error(&parser->errors, illegal_declaration_specifiers(token, conflict));
            } else {
                unsigned_ = token;
            }
        } else if (accept(parser, TK_BOOL, &token)) {
            token_t *conflict = (token_t *) FIRST_NON_NULL(void_, bool_, char_, short_, int_, long_, long_long, float_,
                                                           double_, signed_, unsigned_, complex_, struct_, union_, enum_);
            if (conflict != NULL) {
                append_parse_error(&parser->errors, illegal_declaration_specifiers(token, conflict));
            } else {
                bool_ = token;
            }
        } else if (accept(parser, TK_COMPLEX, &token)) {
            token_t *conflict = (token_t *) FIRST_NON_NULL(void_, bool_, char_, short_, int_, long_long, signed_, unsigned_, struct_, union_, enum_);
            if (conflict != NULL) {
                append_parse_error(&parser->errors, illegal_declaration_specifiers(token, conflict));
            } else {
                complex_ = token;
            }
        } else if (accept(parser, TK_STRUCT, &token)) {
            assert(false && "Parsing of struct/union/enum not yet implemented");
        } else if (accept(parser, TK_UNION, &token)) {
            assert(false && "Parsing of struct/union/enum not yet implemented");
        } else if (accept(parser, TK_ENUM, &token)) {
            assert(false && "Parsing of struct/union/enum not yet implemented");
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

    if (ANY_NON_NULL(bool_, char_, short_, int_, long_long, signed_, unsigned_)) {
        type->kind = TYPE_INTEGER;
        type->integer.is_signed = unsigned_ == NULL;
        if (bool_ != NULL) {
            type->integer.size = INTEGER_TYPE_BOOL;
        } else if (char_ != NULL) {
            type->integer.size = INTEGER_TYPE_CHAR;
        } else if (short_ != NULL) {
            type->integer.size = INTEGER_TYPE_SHORT;
        } else if (long_long != NULL) {
            type->integer.size = INTEGER_TYPE_LONG_LONG;
        } else if (long_ != NULL) {
            type->integer.size = INTEGER_TYPE_LONG;
        } else {
            type->integer.size = INTEGER_TYPE_INT;
        }
    } else if (float_ != NULL || double_ != NULL) {
        type->kind = TYPE_FLOATING;
        if (double_ != NULL) {
            if (long_ != NULL) {
                type->floating = FLOAT_TYPE_LONG_DOUBLE;
            } else {
                type->floating = FLOAT_TYPE_DOUBLE;
            }
        } else {
            type->floating = FLOAT_TYPE_FLOAT;
        }
    } else if (long_ != NULL) {
        type->kind = TYPE_INTEGER;
        type->integer.is_signed = true;
        type->integer.size = INTEGER_TYPE_LONG;
    } else if (void_ != NULL) {
        type->kind = TYPE_VOID;
    } else {
        // Implicit int. This is an error, but we can recover from it.
        append_parse_error(&parser->errors, (parse_error_t) {
                .token = next_token(parser),
                .previous_token = NULL,
                .production_name = "declaration-specifiers",
                .previous_production_name = NULL,
                .type = PARSE_ERROR_TYPE_SPECIFIER_MISSING,
        });
        type->kind = TYPE_INTEGER;
        type->integer.size = INTEGER_TYPE_INT;
        type->integer.is_signed = true;
    }

    return true;
}

bool parse_declaration_specifiers(parser_t *parser, type_t *type) {
    parse_specifiers(parser, true, type);
}

bool parse_specifier_qualifier_list(parser_t *parser, type_t *type) {
    parse_specifiers(parser, false, type);
}

bool parse_init_declarator(parser_t *parser, type_t base_type, declaration_t *decl) {
    if (!parse_declarator(parser, base_type, decl)) {
        return false;
    }

    token_t *token;
    if (accept(parser, TK_ASSIGN, &token)) {
        expression_t *expr = malloc(sizeof(expression_t));
        if (!parse_assignment_expression(parser, expr)) {
            free(expr);
            return false;
        }
        decl->initializer = expr;
    }

    return true;
}

type_t* get_innermost_incomplete_type(type_t *type) {
    type_t *current = type;
    while (current->kind == TYPE_POINTER || current->kind == TYPE_ARRAY || current->kind == TYPE_FUNCTION) {
        if (current->kind == TYPE_POINTER && current->pointer.base != NULL) {
            current = current->pointer.base;
        } else if (current->kind == TYPE_ARRAY && current->array.element_type != NULL) {
            current = current->array.element_type;
        } else if (current->kind == TYPE_FUNCTION && current->function.return_type != NULL) {
            current = current->function.return_type;
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
            current->pointer.base = next;
        } else if (current->kind == TYPE_ARRAY) {
            current->array.element_type = next;
        } else if (current->kind == TYPE_FUNCTION) {
            current->function.return_type = next;
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

    // Move the base type to the heap
    type_t *base = malloc(sizeof(token_t));
    *base = base_type;

    if (type == NULL) {
        type = base;
    } else {
        type_t *inner = get_innermost_incomplete_type(type);
        if (inner->kind == TYPE_POINTER) {
            inner->pointer.base = base;
        } else if (inner->kind == TYPE_ARRAY) {
            inner->array.element_type = base;
        } else if (inner->kind == TYPE_FUNCTION) {
            inner->function.return_type = base;
        } else {
            assert(false); // Invalid type stack
        }
    }

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
            .pointer = {
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
                .type = PARSE_ERROR_EXPECTED_TOKEN,
                .expected_token = {
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
                .array = {
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
                .function = {
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
                            .type = PARSE_ERROR_PARAMETER_TYPE_MALFORMED,
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
    type_t *base = malloc(sizeof(token_t));
    *base = base_type;

    if (type == NULL) {
        type = base;
    } else {
        type_t *inner = get_innermost_incomplete_type(type);
        if (inner->kind == TYPE_POINTER) {
            inner->pointer.base = base;
        } else if (inner->kind == TYPE_ARRAY) {
            inner->array.element_type = base;
        } else if (inner->kind == TYPE_FUNCTION) {
            inner->function.return_type = base;
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
                    .type = PARSE_ERROR_EXPECTED_TOKEN,
                    .expected_token = {
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
                .type = STATEMENT_EMPTY,
                .terminator = terminator,
        };
        return true;
    }

    token_t *begin = NULL;
    if (accept(parser, TK_LBRACE, &begin)) {
        return parse_compound_statement(parser, stmt, begin);
    } else if (accept(parser, TK_IF, &begin)) {
        return parse_if_statement(parser, stmt, begin);
    } else if (accept(parser, TK_RETURN, &begin)) {
        return parse_return_statement(parser, stmt, begin);
    } else if (accept(parser, TK_WHILE, &begin)) {
        return parse_while_statement(parser, stmt, begin);
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
                              TK_UNION, TK_ENUM)) {
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
                        .type = BLOCK_ITEM_DECLARATION,
                        .declaration =  declarations.buffer[i],
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
                    .type = BLOCK_ITEM_STATEMENT,
                    .statement = statement,
            };
            append_ptr((void ***) &block_items.buffer, &block_items.size, &block_items.capacity, block_item);
        }
    }
    shrink_ptr_vector((void ***) &block_items.buffer, &block_items.size, &block_items.capacity);

    if (last_token->kind == TK_RBRACE) {
        *stmt = (statement_t) {
            .type = STATEMENT_COMPOUND,
            .compound = {
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
                .type = PARSE_ERROR_UNEXPECTED_END_OF_INPUT,
                .unexpected_end_of_input = {
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
        .type = STATEMENT_IF,
        .if_ = {
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
            .type = STATEMENT_RETURN,
            .return_ = {
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

    if (!require(parser, TK_RPAREN, NULL, "while-statement", NULL)) return false;

    statement_t *body = malloc(sizeof(statement_t));
    if (!parse_statement(parser, body)) {
        free(body);
        return false;
    }

    *statement = (statement_t) {
        .type = STATEMENT_WHILE,
        .while_ = {
            .keyword = keyword,
            .condition = condition,
            .body = body,
        },
    };

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
            .type = STATEMENT_EXPRESSION,
            .expression = expr,
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
                .type = EXPRESSION_BINARY,
                .binary = {
                        .type = BINARY_COMMA,
                        .left = left,
                        .right = right,
                        .operator = token,
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
                .type = EXPRESSION_BINARY,
                .binary = {
                        .type = BINARY_ASSIGNMENT,
                        .left = left,
                        .right = right,
                        .operator = token,
                        .assignment_operator = assignment_operator,
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
                .type = EXPRESSION_TERNARY,
                .ternary = {
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
                .type = EXPRESSION_BINARY,
                .binary = {
                        .type = BINARY_LOGICAL,
                        .left = left,
                        .right = right,
                        .operator = token,
                        .logical_operator = BINARY_LOGICAL_OR,
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
                .type = EXPRESSION_BINARY,
                .binary = {
                        .type = BINARY_LOGICAL,
                        .left = left,
                        .right = right,
                        .operator = token,
                        .logical_operator = BINARY_LOGICAL_AND,
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
                .type = EXPRESSION_BINARY,
                .binary = {
                        .type = BINARY_BITWISE,
                        .left = left,
                        .right = right,
                        .operator = token,
                        .bitwise_operator = BINARY_BITWISE_OR,
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
                .type = EXPRESSION_BINARY,
                .binary = {
                        .type = BINARY_BITWISE,
                        .left = left,
                        .right = right,
                        .operator = token,
                        .bitwise_operator = BINARY_BITWISE_XOR,
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
                .type = EXPRESSION_BINARY,
                .binary = {
                        .type = BINARY_BITWISE,
                        .left = left,
                        .right = right,
                        .operator = token,
                        .bitwise_operator = BINARY_BITWISE_AND,
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
            .type = EXPRESSION_BINARY,
            .binary = {
                    .type = BINARY_COMPARISON,
                    .left = left,
                    .right = right,
                    .operator = operator,
                    .comparison_operator = comparison_operator
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
            .type = EXPRESSION_BINARY,
            .binary = {
                    .type = BINARY_COMPARISON,
                    .left = left,
                    .right = right,
                    .operator = operator,
                    .comparison_operator = binary_operator,
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
            .type = EXPRESSION_BINARY,
            .binary = {
                    .type = BINARY_BITWISE,
                    .left = left,
                    .right = right,
                    .operator = operator,
                    .bitwise_operator = binary_operator,
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
            .type = EXPRESSION_BINARY,
            .binary = {
                    .type = BINARY_ARITHMETIC,
                    .left = left,
                    .right = right,
                    .operator = operator,
                    .arithmetic_operator = binary_operator,
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
            .type = EXPRESSION_BINARY,
            .binary = {
                .left = left,
                .right = right,
                .operator = operator,
                .arithmetic_operator = binary_operator,
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
    if (accept(parser, TK_LPAREN, &token)) {
        type_t *type = NULL;
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
            .type = EXPRESSION_CAST,
            .cast = {
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
            .type = EXPRESSION_UNARY,
            .unary = {
                    .operator = operator,
                    .operand = operand,
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
            .type = EXPRESSION_UNARY,
            .unary = {
                .operator = UNARY_PRE_INCREMENT,
                .operand = operand,
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
            .type = EXPRESSION_UNARY,
            .unary = {
                .operator = UNARY_PRE_DECREMENT,
                .operand = operand,
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
                    .type = EXPRESSION_UNARY,
                    .unary = {
                        .operator = UNARY_SIZEOF,
                        .operand = inner,
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
                    .type = PARSE_ERROR_EXPECTED_EXPRESSION_OR_TYPE_NAME_AFTER_SIZEOF,
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
                .type = EXPRESSION_SIZEOF,
                .sizeof_type = type,
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
                .type = EXPRESSION_UNARY,
                .unary = {
                    .operator = UNARY_SIZEOF,
                    .operand = inner,
                },
            };
            return true;
        }
    } else {
        return parse_postfix_expression(parser, expr);
    }
}

bool parse_postfix_expression(parser_t *parser, expression_t *expr) {
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
                    .type = EXPRESSION_ARRAY_SUBSCRIPT,
                    .array_subscript = {
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
                if (!parse_assignment_expression(parser, argument)) {
                    free(argument);
                    // TODO: cleanup arguments
                    return false;
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
                    .type = EXPRESSION_CALL,
                    .call = {
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
                    .type = EXPRESSION_MEMBER_ACCESS,
                    .member_access = {
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
                    .type = EXPRESSION_UNARY,
                    .unary = {
                            .operator = UNARY_POST_INCREMENT,
                            .operand = operand,
                    },
            };
        } else if (accept(parser, TK_DECREMENT, NULL)) {
            expression_t *operand = current;
            current = malloc(sizeof(expression_t));

            *current = (expression_t) {
                    .span = {
                            .start = operand->span.start,
                            .end = *current_position(parser),
                    },
                    .type = EXPRESSION_UNARY,
                    .unary = {
                            .operator = UNARY_POST_DECREMENT,
                            .operand = operand,
                    },
            };
            return true;
        }
    }

    *expr = *current;
    free(current);

    return true;
}

bool parse_primary_expression(parser_t* parser, expression_t* expr) {
    token_t *token;
    source_position_t start = *current_position(parser);

    if (accept(parser, TK_IDENTIFIER, &token)) {
        *expr = (expression_t) {
            .span = SPAN_STARTING(start),
            .type = EXPRESSION_PRIMARY,
            .primary = {
                .type = PE_IDENTIFIER,
                .token = *token,
            }
        };
        return true;
    } else if (accept(parser, TK_INTEGER_CONSTANT, &token) ||
               accept(parser, TK_FLOATING_CONSTANT, &token) ||
               accept(parser, TK_CHAR_LITERAL, &token)) {
        *expr = (expression_t) {
            .span = SPAN_STARTING(start),
            .type = EXPRESSION_PRIMARY,
            .primary = {
                .type = PE_CONSTANT,
                .token = *token,
            },
        };
        return true;
    } else if (accept(parser, TK_STRING_LITERAL, &token)) {
        *expr = (expression_t) {
            .span = SPAN_STARTING(start),
            .type = EXPRESSION_PRIMARY,
            .primary = {
                .type = PE_STRING_LITERAL,
                .token = *token,
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
            .type = EXPRESSION_PRIMARY,
            .primary = {
                .type = PE_EXPRESSION,
                .expression = expr2,
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
            .type = PARSE_ERROR_EXPECTED_TOKEN,
            .expected_token = {
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

    declaration_t *decl = malloc(sizeof(declaration_t));
    if (!parse_declarator(parser, type, decl)) {
        return false;
    }

    token_t *body_start = NULL;
    if (decl->type->kind == TYPE_FUNCTION && accept(parser, TK_LBRACE, &body_start)) {
        // This is a function definition
        function_definition_t *fn = malloc(sizeof(function_definition_t));
        if (!parse_function_definition(parser, decl, body_start, fn)) {
            free(fn);
            return false;
        }
        *external_declaration = (external_declaration_t) {
            .type = EXTERNAL_DECLARATION_FUNCTION_DEFINITION,
            .function_definition = fn,
        };
    } else {
        // This is a declaration
        ptr_vector_t declarations = {.size = 0, .capacity = 0, .buffer = NULL};
        if (!_parse_declaration(parser, decl, &declarations)) {
            return false;
        }

        *external_declaration = (external_declaration_t) {
            .type = EXTERNAL_DECLARATION_DECLARATION,
            .declaration = {
                .declarations = (declaration_t**) declarations.buffer,
                .length = declarations.size,
            },
        };
    }

    return true;
}

bool parse_function_definition(parser_t *parser, declaration_t *declarator, const token_t *body_start, function_definition_t *fn) {
    statement_t *body = malloc(sizeof(statement_t));
    if (!parse_compound_statement(parser, body, body_start)) {
        free(body);
        return false;
    }

    *fn = (function_definition_t) {
        .identifier = declarator->identifier,
        .return_type = declarator->type->function.return_type,
        .parameter_list = declarator->type->function.parameter_list,
        .body = body,
    };

    return true;
}
