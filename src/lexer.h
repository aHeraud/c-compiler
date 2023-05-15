#ifndef C_COMPILER_PREPROCESSOR_LEXER_H
#define C_COMPILER_PREPROCESSOR_LEXER_H
#include <stdint.h>
#include "util/vectors.h"

typedef enum TokenKind {
    TK_NONE,
    TK_COMMENT,

    /* Preprocessor Directives */
    TK_PP_INCLUDE,
    TK_PP_DEFINE,
    TK_PP_UNDEF,
    TK_PP_IFDEF,
    TK_PP_LINE,

    TK_TYPE_VOID,
    TK_TYPE_CHAR,
    TK_TYPE_SHORT,
    TK_TYPE_INT,
    TK_TYPE_LONG,
    TK_TYPE_FLOAT,
    TK_TYPE_DOUBLE,
    TK_TYPE_SIGNED,
    TK_TYPE_UNSIGNED,
    TK_TYPE_STRUCT,
    TK_TYPE_UNION,
    TK_TYPE_ENUM,
    TK_TYPE_TYPEDEF,

    /* Flow Control */
    TK_IF,
    TK_ELSE,
    TK_RETURN,

    TK_IDENTIFIER,
    TK_CHAR_LITERAL,
    TK_STRING_LITERAL,
    TK_INTEGER_CONSTANT,

    TK_SEMICOLON,
    TK_COMMA,
    TK_LPAREN,
    TK_RPAREN,
    TK_LBRACE,
    TK_RBRACE,
    TK_DOT,
    TK_PLUS,
    TK_MINUS,
    TK_ARROW,
    TK_STAR,
    TK_SLASH,
    TK_NEWLINE,
    TK_EOF
} token_kind_t;

typedef struct SourcePosition {
    uint32_t line;
    uint32_t column;
} source_position_t;

typedef struct Token {
    enum TokenKind kind;
    char* value;
    struct SourcePosition position;
} token_t;

/**
 * Vector of tokens.
 * Pointers to tokens in this vector are only guaranteed to be valid until the next call to lex(),
 * as the buffer may be reallocated.
 *
 * TODO: use some sort of linked list, or linked list of arrays, to avoid reallocating the buffer
 */
typedef struct TokenVector {
    token_t* buffer;
    size_t len;
    size_t max_len;
} token_vector_t;

typedef struct Lexer {
    const char* input_path;
    const char* input;
    size_t input_offset;
    size_t input_len;
    source_position_t position;
} lexer_t;

lexer_t linit(const char* input_path, const char* input, size_t input_len);
void lfree(lexer_t* lexer);
token_t lscan(lexer_t* lexer);

#endif //C_COMPILER_PREPROCESSOR_LEXER_H
