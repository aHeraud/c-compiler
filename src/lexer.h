#ifndef C_COMPILER_PREPROCESSOR_LEXER_H
#define C_COMPILER_PREPROCESSOR_LEXER_H
#include <stdint.h>
#include "util/vectors.h"

typedef enum TokenKind {
    TK_NONE,
    TK_COMMENT,
    TK_NEWLINE,

    /* Preprocessor Directives */
    TK_PP_INCLUDE,
    TK_PP_DEFINE,
    TK_PP_UNDEF,
    TK_PP_IFDEF,
    TK_PP_LINE,

    TK_VOID,
    TK_CHAR,
    TK_SHORT,
    TK_INT,
    TK_LONG,
    TK_FLOAT,
    TK_DOUBLE,
    TK_SIGNED,
    TK_UNSIGNED,
    TK_BOOL,
    TK_COMPLEX,
    TK_STRUCT,
    TK_UNION,
    TK_ENUM,
    TK_TYPEDEF,
    TK_STATIC,
    TK_AUTO,
    TK_REGISTER,
    TK_IF,
    TK_ELSE,
    TK_SWITCH,
    TK_CASE,
    TK_DEFAULT,
    TK_GOTO,
    TK_CONTINUE,
    TK_BREAK,
    TK_RETURN,
    TK_WHILE,
    TK_DO,
    TK_FOR,
    TK_SIZEOF,
    TK_CONST,
    TK_RESTRICT,
    TK_VOLATILE,
    TK_EXTERN,
    TK_INLINE,

    /* Identifier */
    TK_IDENTIFIER,

    /* Constants and string literals */
    TK_CHAR_LITERAL,
    TK_STRING_LITERAL,
    TK_INTEGER_CONSTANT,
    TK_FLOATING_CONSTANT,

    /* Punctuators */
    TK_ASSIGN, // '='
    TK_ASTERISK,
    TK_AMPERSAND,
    TK_SEMICOLON,
    TK_COMMA,
    TK_COLON,
    TK_EXCLAMATION,
    TK_LPAREN,
    TK_RPAREN,
    TK_LBRACE,
    TK_RBRACE,
    TK_LBRACKET,
    TK_RBRACKET,
    TK_DOT,
    TK_PLUS,
    TK_MINUS,
    TK_ARROW,
    TK_STAR,
    TK_SLASH,
    TK_EOF,
    TK_TILDE,
    TK_INCREMENT,
    TK_DECREMENT,
    TK_EQUALS, // '=='
    TK_NOT_EQUALS, // '!='
    TK_LESS_THAN, // '<'
    TK_GREATER_THAN, // '>'
    TK_LESS_THAN_EQUAL, // '<='
    TK_GREATER_THAN_EQUAL, // '>='
    TK_ELLIPSIS, // '...'
} token_kind_t;

typedef struct SourcePosition {
    const char* path;
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
    size_t size;
    size_t capacity;
} token_vector_t;

void append_token(token_t** buffer, size_t *size, size_t* capacity, token_t token);

typedef struct Lexer lexer_t;
typedef struct Lexer {
    const char* input_path;
    const char* input;
    size_t input_offset;
    size_t input_len;
    source_position_t position;
    lexer_t* child; // Used for nested lexers, e.g. for #include
} lexer_t;

lexer_t linit(const char* input_path, const char* input, size_t input_len);
void lfree(lexer_t* lexer);
token_t lscan(lexer_t* lexer);

#endif //C_COMPILER_PREPROCESSOR_LEXER_H
