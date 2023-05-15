#include <assert.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <ctype.h>

#include "lexer.h"
#include "util/vectors.h"

struct ReservedWord {
    char* word;
    enum TokenKind kind;
};

struct ReservedWord RESERVED_WORDS[] = {
//        {"auto", TK_AUTO},
//        {"break", TK_BREAK},
//        {"case", TK_CASE},
        {"char", TK_TYPE_CHAR},
//        {"const", TK_CONST},
//        {"continue", TK_CONTINUE},
//        {"default", TK_DEFAULT},
//        {"do", TK_DO},
        {"double", TK_TYPE_DOUBLE},
        {"else", TK_ELSE},
        {"enum", TK_TYPE_ENUM},
//        {"extern", TK_EXTERN},
        {"float", TK_TYPE_FLOAT},
//        {"for", TK_FOR},
//        {"goto", TK_GOTO},
        {"if", TK_IF},
//        {"inline", TK_INLINE},
        {"int", TK_TYPE_INT},
        {"long", TK_TYPE_LONG},
//        {"register", TK_REGISTER},
//        {"restrict", TK_RESTRICT},
        {"return", TK_RETURN},
        {"short", TK_TYPE_SHORT},
        {"signed", TK_TYPE_SIGNED},
//        {"sizeof", TK_SIZEOF},
//        {"static", TK_STATIC},
        {"struct", TK_TYPE_STRUCT},
//        {"switch", TK_SWITCH},
        {"typedef", TK_TYPE_TYPEDEF},
        {"union", TK_TYPE_UNION},
        {"unsigned", TK_TYPE_UNSIGNED},
        {"void", TK_TYPE_VOID},
//        {"volatile", TK_VOLATILE},
//        {"while", TK_WHILE},
//        {"_Alignas", TK_ALIGNAS},
//        {"_Alignof", TK_ALIGNOF},
//        {"_Atomic", TK_ATOMIC},
//        {"_Bool", TK_BOOL},
//        {"_Complex", TK_COMPLEX},
//        {"_Generic", TK_GENERIC},
//        {"_Imaginary", TK_IMAGINARY},
//        {"_Noreturn", TK_NORETURN},
//        {"_Static_assert", TK_STATIC_ASSERT},
//        {"_Thread_local", TK_THREAD_LOCAL}
};

struct ReservedWord PREPROCESSOR_DIRECTIVES[] = {
        {"include", TK_PP_INCLUDE},
        {"define", TK_PP_DEFINE},
        {"undef", TK_PP_UNDEF},
        {"ifdef", TK_PP_IFDEF},
        {"line", TK_PP_LINE}
};

void string_literal(struct Lexer* lexer, struct Token* token);
void char_literal(struct Lexer* lexer, struct Token* token);
void integer_constant(struct Lexer* lexer, struct Token* token);
void identifier_or_reserved_word(struct Lexer* lexer, struct Token* token);
void comment(struct Lexer* lexer, token_t* token);

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

lexer_t linit(const char* input_path, const char* input, size_t input_len) {
    lexer_t lexer = {
        .input_path = input_path,
        .input = input,
        .input_offset = 0,
        .input_len = input_len,
        .position = {
            .line = 1,
            .column = 0,
        }
    };
    return lexer;
}

void lfree(lexer_t* lexer) {
    free(lexer->input);
    lexer->input = NULL;
}

token_t lscan(struct Lexer* lexer) {
    // skip whitespace
    char c0 = lpeek(lexer, 1);
    while (c0 == ' ' || c0 == '\t') {
        ladvance(lexer);
        c0 = lpeek(lexer, 1);
    }

    struct Token token;
    token.kind = TK_NONE;
    token.position = lexer->position;

    c0 = lpeek(lexer, 1); // lookahead 1
    char c1 = lpeek(lexer, 2); // lookahead 2
    switch (c0) {
        case '\r':
        case '\n':
            ladvance(lexer);
            token.kind = TK_NEWLINE;
            token.value = "\n";
            break;
        case ';':
            ladvance(lexer);
            token.kind = TK_SEMICOLON;
            token.value = ";";
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
        default:
            if (isalpha(c0) || c0 == '_') {
                identifier_or_reserved_word(lexer, &token);
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
    struct SourcePosition match_start = {lexer->position.line, lexer->position.column};
    struct CharVector buffer = {malloc(512), 0, 512};
    char c = ladvance(lexer);
    assert(c == '"');
    append_char(&buffer.buffer, &buffer.len, &buffer.max_len, c);
    while ((c = ladvance(lexer)) && c != '"') {
        if (c == '\r' || c == '\n') {
            // Illegal newline in string literal
            // TODO: better error handling
            fprintf(stderr, "Illegal newline in string literal at %s:%d:%d\n",
                    lexer->input_path, lexer->position.line, lexer->position.column);
            exit(1);
        } else if (c == '\\' && lpeek(lexer, 1) == '"') {
            // Special handling for escaped double quote
            append_char(&buffer.buffer, &buffer.len, &buffer.max_len, c);
            append_char(&buffer.buffer, &buffer.len, &buffer.max_len, ladvance(lexer));
        } else {
            append_char(&buffer.buffer, &buffer.len, &buffer.max_len, c);
        }
    }

    if (c != '"') {
        // TODO: better error handling
        fprintf(stderr, "Unterminated string literal at %s:%d:%d\n",
                lexer->input_path, lexer->position.line, lexer->position.column);
        exit(1);
    }
    append_char(&buffer.buffer, &buffer.len, &buffer.max_len, c);
    token->kind = TK_STRING_LITERAL;
    token->position = match_start;
    token->value = realloc(buffer.buffer, buffer.len);
}

void char_literal(struct Lexer* lexer, struct Token* token) {
    struct SourcePosition match_start = {lexer->position.line, lexer->position.column};
    struct CharVector buffer = {malloc(4), 0, 4};
    char c = ladvance(lexer);
    assert(c == '\'');
    append_char(&buffer.buffer, &buffer.len, &buffer.max_len, c);
    while ((c = ladvance(lexer)) && c != '\'') {
        if (c == '\r' || c == '\n') {
            // Illegal newline in character literal
            // TODO: better error handling
            fprintf(stderr, "Illegal newline in character literal at %s:%d:%d\n",
                    lexer->input_path, lexer->position.line, lexer->position.column);
            exit(1);
        } else if (c == '\\' && lpeek(lexer, 1) == '\'') {
            // Special handling for escaped single quote
            append_char(&buffer.buffer, &buffer.len, &buffer.max_len, c);
            append_char(&buffer.buffer, &buffer.len, &buffer.max_len, ladvance(lexer));
        } else {
            append_char(&buffer.buffer, &buffer.len, &buffer.max_len, c);
        }
    }

    if (c != '\'') {
        // TODO: better error handling
        fprintf(stderr, "Unterminated character literal at %s:%d:%d\n",
                lexer->input_path, lexer->position.line, lexer->position.column);
        exit(1);
    }
    append_char(&buffer.buffer, &buffer.len, &buffer.max_len, c);

    token->position = match_start;
    token->kind = TK_CHAR_LITERAL;
    token->value = realloc(buffer.buffer, buffer.len);
}

void integer_constant(struct Lexer* lexer, struct Token* token) {
    struct SourcePosition match_start = {lexer->position.line, lexer->position.column};
    struct CharVector buffer = {malloc(32), 0, 32};

    // TODO: handle hex, octal, and binary literals
    char c = ladvance(lexer);
    assert(isdigit(c));

    do {
        append_char(&buffer.buffer, &buffer.len, &buffer.max_len, c);
    } while ((c = lpeek(lexer, 1)) && isdigit(c));

    append_char(&buffer.buffer, &buffer.len, &buffer.max_len, '\0');

    token->kind = TK_INTEGER_CONSTANT;
    token->position = match_start;
    token->value = realloc(buffer.buffer, buffer.len);
}

void identifier_or_reserved_word(struct Lexer* lexer, struct Token* token) {
    struct SourcePosition match_start = {lexer->position.line, lexer->position.column};
    struct CharVector buffer = {malloc(32), 0, 32};

    char c = ladvance(lexer);
    assert(isalpha(c) || c == '_');
    append_char(&buffer.buffer, &buffer.len, &buffer.max_len, c);

    while ((c = lpeek(lexer, 1)) && (isalnum(c) || c == '_' )) {
        append_char(&buffer.buffer, &buffer.len, &buffer.max_len, ladvance(lexer));
    }
    append_char(&buffer.buffer, &buffer.len, &buffer.max_len, '\0');

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
    token->value = realloc(buffer.buffer, buffer.len);
    token->position = match_start;
}

void comment(struct Lexer* lexer, token_t* token) {
    char_vector_t buffer = {malloc(64), 0, 64};

    char c = ladvance(lexer);
    assert(c == '/');
    append_char(&buffer.buffer, &buffer.len, &buffer.max_len, c);
    c = ladvance(lexer);
    assert(c == '/' || c == '*');
    append_char(&buffer.buffer, &buffer.len, &buffer.max_len, c);

    if (c == '*') {
        while ((c = ladvance(lexer)) && c != '*' && lpeek(lexer, 1) != '/') {
            append_char(&buffer.buffer, &buffer.len, &buffer.max_len, c);
        }

        if (c != '*' || lpeek(lexer, 1) != '/') {
            // error
        }
        append_char(&buffer.buffer, &buffer.len, &buffer.max_len, c);
        append_char(&buffer.buffer, &buffer.len, &buffer.max_len, ladvance(lexer));
    } else {
        while ((c = lpeek(lexer, 1)) && c != '\n') {
            append_char(&buffer.buffer, &buffer.len, &buffer.max_len, ladvance(lexer));
        }
    }

    append_char(&buffer.buffer, &buffer.len, &buffer.max_len, '\0');

    token->kind = TK_COMMENT;
    token->value = realloc(buffer.buffer, buffer.len);
}
