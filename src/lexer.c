#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

#include "lexer.h"
#include "preprocessor.h"
#include "util/vectors.h"

void string_literal(struct Lexer* lexer, struct Token* token);
void char_literal(struct Lexer* lexer, struct Token* token);
void integer_constant(struct Lexer* lexer, struct Token* token);
void identifier_or_reserved_word(struct Lexer* lexer, struct Token* token);
void comment(struct Lexer* lexer, token_t* token);

void append_token(token_t** buffer, size_t *size, size_t* capacity, token_t token) {
    if (*size + 1 > *capacity) {
        *capacity > 0 ? (*capacity *= 2) : (*capacity = 1);
        *buffer = realloc(*buffer, *capacity * sizeof(token_t));
        assert(buffer != NULL);
    }

    (*buffer)[*size] = token;
    *size += 1;
}

char ladvance(struct Lexer* lexer) {
    if (lexer->input_offset >= lexer->input_len) {
        return '\0';
    }

    char c0 = lexer->input[lexer->input_offset++];
    char c1 = (c0 == '\0') ? '\0' : (lexer->input)[lexer->input_offset]; // lookahead 1
    char c2 = (c1 == '\0') ? '\0' : (lexer->input)[lexer->input_offset + 1]; // lookahead 2

    if (c0 == '\n' || c0 == '\r') {
        // normalize line endings as '\n'
        if ((c0 == '\n' && c1 == '\r') || (c0 == '\r' && c1 == '\n')) {
            // skip the second character of the line ending
            lexer->input_offset++;
        }
        lexer->position.line++;
        lexer->position.column = 0;
        return '\n';
    } else if (c0 == '\\' && (c1 == '\n' || c1 == '\r')) {
        // Handle line continuations
        if ((c1 == '\n' && c2 == '\r') || (c1 == '\r' && c2 == '\n')) {
            // skip the second character of the line ending
            lexer->input_offset++;
        }
        lexer->position.line++;
        lexer->position.column = 0;
        return ladvance(lexer);
    } else {
        lexer->position.column++;
        return c0;
    }
}

char lpeek(struct Lexer* lexer, unsigned int count) {
    assert(count > 0);
    source_position_t pos = lexer->position; // save position
    size_t input_offset = lexer->input_offset; // save input offset
    char c;
    for (unsigned int i = 0; i < count; i++) {
        c = ladvance(lexer);
        if (c == '\0') {
            break;
        }
    }
    lexer->position = pos; // rewind
    lexer->input_offset = input_offset; // rewind
    return c;
}

lexer_t linit(
        const char* input_path,
        const char* input,
        size_t input_len,
        lexer_global_context_t* global_context
) {
    lexer_t lexer = {
        .input_path = input_path,
        .input = input,
        .input_offset = 0,
        .input_len = input_len,
        .position = {
            .path = input_path,
            .line = 1,
            .column = 0,
        },
        .global_context = global_context,
    };

    return lexer;
}

token_t lscan(struct Lexer* lexer) {
    if (lexer->child != NULL) {
        token_t token = lscan(lexer->child);
        if (token.kind == TK_NONE || token.kind == TK_EOF) {
            // child lexer is done, clean it up
            free(lexer->child);
            lexer->child = NULL;
        } else {
            return token;
        }
    }

    if (lexer->pending_tokens != NULL) {
        // These tokens were already parsed by this lexer (probably by a macro expansion).
        // Return them instead of scanning the input for a new token.
        token_node_t* node = lexer->pending_tokens;
        token_t token = node->token;
        lexer->pending_tokens = node->next;
        free(node);
        return token;
    }

    bool start_of_line = lexer->position.column == 0;

    // skip whitespace
    char c0 = lpeek(lexer, 1);
    while (c0 == ' ' || c0 == '\t' || c0 == '\n') {
        ladvance(lexer);
        c0 = lpeek(lexer, 1);
    }

    struct Token token;
    token.kind = TK_NONE;
    token.position = lexer->position;

    c0 = lpeek(lexer, 1); // lookahead 1
    char c1 = lpeek(lexer, 2); // lookahead 2
    switch (c0) {
//        case '\r':
//        case '\n':
//            ladvance(lexer);
//            token.kind = TK_NEWLINE;
//            token.value = "\n";
//            break;
        case ';':
            ladvance(lexer);
            token.kind = TK_SEMICOLON;
            token.value = ";";
            break;
        case ':':
            ladvance(lexer);
            token.kind = TK_COLON;
            token.value = ":";
            break;
        case ',':
            ladvance(lexer);
            token.kind = TK_COMMA;
            token.value = ",";
            break;
        case '(':
            ladvance(lexer);
            token.kind = TK_LPAREN;
            token.value = "(";
            break;
        case ')':
            ladvance(lexer);
            token.kind = TK_RPAREN;
            token.value = ")";
            break;
        case '{':
            ladvance(lexer);
            token.kind = TK_LBRACE;
            token.value = "{";
            break;
        case '}':
            ladvance(lexer);
            token.kind = TK_RBRACE;
            token.value = "}";
            break;
        case '[':
            ladvance(lexer);
            token.kind = TK_LBRACKET;
            token.value = "[";
            break;
        case ']':
            ladvance(lexer);
            token.kind = TK_RBRACKET;
            token.value = "]";
            break;
        case '/':
            if (c1 == '/' || c1 == '*') {
                // comment
                comment(lexer, &token);
            } else {
                ladvance(lexer);
                token.kind = TK_SLASH;
                token.value = "/";
            }
            break;
        case '\'':
            char_literal(lexer, &token);
            break;
        case '"':
            string_literal(lexer, &token);
            break;
        case '.':
            ladvance(lexer);

            if (lpeek(lexer, 1) == '.' && lpeek(lexer, 2) == '.') {
                ladvance(lexer);
                ladvance(lexer);
                token.kind = TK_ELLIPSIS;
                token.value = "...";
                break;
            }

            // TODO: floating point constants

            token.kind = TK_DOT;
            token.value = ".";
            break;
        case '+':
            ladvance(lexer);
            token.kind = TK_PLUS;
            token.value = "+";
            break;
        case '-':
            ladvance(lexer);
            if (lpeek(lexer, 1) == '>') {
                ladvance(lexer);
                token.kind = TK_ARROW;
                token.value = "->";
            } else {
                token.kind = TK_MINUS;
                token.value = "-";
            }
            break;
        case '*':
            ladvance(lexer);
            token.kind = TK_STAR;
            token.value = "*";
            break;
        case '=':
            ladvance(lexer);
            if (c1 == '=') {
                ladvance(lexer);
                token.kind = TK_EQUALS;
                token.value = "==";
            } else {
                token.kind = TK_ASSIGN;
                token.value = "=";
            }
            break;
        case '#':
            if (start_of_line) {
                // preprocessor directive
                token_t directive_name;
                preprocessor_directive(lexer, &directive_name);
                switch (directive_name.kind) {
                    case TK_PP_INCLUDE:
                        preprocessor_include(lexer);
                        return lscan(lexer); // scan next token
                    case TK_PP_DEFINE:
                        {
                            macro_definition_t* macro_definition = malloc(sizeof(macro_definition_t));
                            preprocessor_define(lexer, macro_definition);
                            hash_table_insert(&lexer->global_context->macro_definitions,
                                              macro_definition->name,
                                              macro_definition);
                        }
                        return lscan(lexer); // scan next token
                    default:
                        fprintf(stderr, "%s:%d:%d: Unknown preprocessor directive '%s'\n",
                                directive_name.position.path, directive_name.position.line, directive_name.position.column, directive_name.value);
                        exit(1); // TODO: error recovery
                }
            } else {
                // only really valid while processing macros, but the parser can handle it
                ladvance(lexer); // consume '#'
                char next = lpeek(lexer, 1);
                if (next == '#') {
                    ladvance(lexer); // consume next '#'
                    token.kind = TK_DOUBLE_HASH;
                    token.value = "##";
                } else {
                    token.kind = TK_HASH;
                    token.value = "#";
                }
                break;
            }
            break;
        default:
            if (isalpha(c0) || c0 == '_') {
                identifier_or_reserved_word(lexer, &token);
                if (token.kind == TK_IDENTIFIER) {
                    // check if this is a macro
                    macro_definition_t* macro_definition;
                    if (hash_table_lookup(&lexer->global_context->macro_definitions, token.value, (void**) &macro_definition)) {
                        // expand macro
                        macro_parameters_t parameters;
                        preprocessor_parse_macro_invocation_parameters(lexer, macro_definition, &parameters);
                        preprocessor_expand_macro(lexer, macro_definition, parameters);
                        return lscan(lexer); // skip past the macro name and scan next token
                    }
                }
            } else if (isdigit(c0)) {
                // TODO: handle floats generally
                // TODO: handle floating point numbers beginning with '.' (e.g. .5f)
                // TODO: handle floats in scientific notation (e.g. 1.0e-5f)
                integer_constant(lexer, &token);
            } else if (c0 == '\0') {
                token.kind = TK_EOF;
                token.value = "EOF";
            } else {
                fprintf(stderr, "Unexpected character '%c' at %d:%d\n",
                        c0, lexer->position.line, lexer->position.column);
                exit(1);
            }
            break;
    }

    return token;
}

void string_literal(struct Lexer* lexer, struct Token* token) {
    struct SourcePosition match_start = {lexer->position.path, lexer->position.line, lexer->position.column};
    struct CharVector buffer = {malloc(512), 0, 512};
    char c = ladvance(lexer);
    assert(c == '"');

    while ((c = ladvance(lexer)) && c != '"') {
        if (c == '\r' || c == '\n') {
            // Illegal newline in string literal
            // TODO: better error handling
            fprintf(stderr, "Illegal newline in string literal at %s:%d:%d\n",
                    lexer->input_path, lexer->position.line, lexer->position.column);
            exit(1);
        } else if (c == '\\' && lpeek(lexer, 1) == '"') {
            // Special handling for escaped double quote
            append_char(&buffer.buffer, &buffer.size, &buffer.capacity, c);
            append_char(&buffer.buffer, &buffer.size, &buffer.capacity, ladvance(lexer));
        } else {
            append_char(&buffer.buffer, &buffer.size, &buffer.capacity, c);
        }
    }

    if (c != '"') {
        // TODO: better error handling
        fprintf(stderr, "Unterminated string literal at %s:%d:%d\n",
                lexer->input_path, lexer->position.line, lexer->position.column);
        exit(1);
    }
    append_char(&buffer.buffer, &buffer.size, &buffer.capacity, '\0');
    token->kind = TK_STRING_LITERAL;
    token->position = match_start;
    token->value = realloc(buffer.buffer, buffer.size);
}

void char_literal(struct Lexer* lexer, struct Token* token) {
    struct SourcePosition match_start = {lexer->position.path, lexer->position.line, lexer->position.column};
    struct CharVector buffer = {malloc(4), 0, 4};
    char c = ladvance(lexer);
    assert(c == '\'');
    append_char(&buffer.buffer, &buffer.size, &buffer.capacity, c);
    while ((c = ladvance(lexer)) && c != '\'') {
        if (c == '\r' || c == '\n') {
            // Illegal newline in character literal
            // TODO: better error handling
            fprintf(stderr, "Illegal newline in character literal at %s:%d:%d\n",
                    lexer->input_path, lexer->position.line, lexer->position.column);
            exit(1);
        } else if (c == '\\' && lpeek(lexer, 1) == '\'') {
            // Special handling for escaped single quote
            append_char(&buffer.buffer, &buffer.size, &buffer.capacity, c);
            append_char(&buffer.buffer, &buffer.size, &buffer.capacity, ladvance(lexer));
        } else {
            append_char(&buffer.buffer, &buffer.size, &buffer.capacity, c);
        }
    }

    if (c != '\'') {
        // TODO: better error handling
        fprintf(stderr, "Unterminated character literal at %s:%d:%d\n",
                lexer->input_path, lexer->position.line, lexer->position.column);
        exit(1);
    }
    append_char(&buffer.buffer, &buffer.size, &buffer.capacity, c);

    token->position = match_start;
    token->kind = TK_CHAR_LITERAL;
    token->value = realloc(buffer.buffer, buffer.size);
}

void integer_constant(struct Lexer* lexer, struct Token* token) {
    struct SourcePosition match_start = {lexer->position.path, lexer->position.line, lexer->position.column};
    struct CharVector buffer = {malloc(32), 0, 32};

    // TODO: handle hex, octal, and binary literals
    char c = ladvance(lexer);
    assert(isdigit(c));

    do {
        append_char(&buffer.buffer, &buffer.size, &buffer.capacity, c);
    } while ((c = lpeek(lexer, 1)) && isdigit(c));

    append_char(&buffer.buffer, &buffer.size, &buffer.capacity, '\0');

    token->kind = TK_INTEGER_CONSTANT;
    token->position = match_start;
    token->value = realloc(buffer.buffer, buffer.size);
}

void identifier_or_reserved_word(struct Lexer* lexer, struct Token* token) {
    struct SourcePosition match_start = {lexer->position.path, lexer->position.line, lexer->position.column};
    struct CharVector buffer = {malloc(32), 0, 32};

    char c = ladvance(lexer);
    assert(isalpha(c) || c == '_');
    append_char(&buffer.buffer, &buffer.size, &buffer.capacity, c);

    while ((c = lpeek(lexer, 1)) && (isalnum(c) || c == '_' )) {
        append_char(&buffer.buffer, &buffer.size, &buffer.capacity, ladvance(lexer));
    }
    append_char(&buffer.buffer, &buffer.size, &buffer.capacity, '\0');

    // Is this a reserved word?
    for(int i = 0; i < sizeof(RESERVED_WORDS) / sizeof(struct ReservedWord); i++) {
        struct ReservedWord reserved_word = RESERVED_WORDS[i];
        char* word = reserved_word.word;
        if (strcmp(buffer.buffer, word) == 0) {
            token->kind = reserved_word.kind;
            token->value = buffer.buffer;
            token->position = match_start;
            return;
        }
    }

    token->kind = TK_IDENTIFIER;
    token->value = realloc(buffer.buffer, buffer.size);
    token->position = match_start;
}

void comment(struct Lexer* lexer, token_t* token) {
    char_vector_t buffer = {malloc(64), 0, 64};

    char c = ladvance(lexer);
    assert(c == '/');
    append_char(&buffer.buffer, &buffer.size, &buffer.capacity, c);
    c = ladvance(lexer);
    assert(c == '/' || c == '*');
    append_char(&buffer.buffer, &buffer.size, &buffer.capacity, c);

    if (c == '*') {
        while ((c = ladvance(lexer)) && c != '*' && lpeek(lexer, 1) != '/') {
            append_char(&buffer.buffer, &buffer.size, &buffer.capacity, c);
        }

        if (c != '*' || lpeek(lexer, 1) != '/') {
            // error
        }
        append_char(&buffer.buffer, &buffer.size, &buffer.capacity, c);
        append_char(&buffer.buffer, &buffer.size, &buffer.capacity, ladvance(lexer));
    } else {
        while ((c = lpeek(lexer, 1)) && c != '\n') {
            append_char(&buffer.buffer, &buffer.size, &buffer.capacity, ladvance(lexer));
        }
    }

    append_char(&buffer.buffer, &buffer.size, &buffer.capacity, '\0');

    token->kind = TK_COMMENT;
    token->value = realloc(buffer.buffer, buffer.size);
}
