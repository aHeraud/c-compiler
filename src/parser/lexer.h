#ifndef C_COMPILER_LEXER_H
#define C_COMPILER_LEXER_H

#include <stdbool.h>
#include <stdint.h>
#include "utils/vectors.h"
#include "utils/hashtable.h"

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

    /* Preprocessor tokens */
    TK_HASH, // stringification
    TK_DOUBLE_HASH, // concatenation

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
    TK_MULTIPLY_ASSIGN, // '*='
    TK_DIVIDE_ASSIGN, // '/='
    TK_MOD_ASSIGN, // '%='
    TK_PLUS_ASSIGN, // '+='
    TK_MINUS_ASSIGN, // '-='
    TK_LSHIFT_ASSIGN, // '<<='
    TK_RSHIFT_ASSIGN, // '>>='
    TK_BITWISE_AND_ASSIGN, // '&='
    TK_BITWISE_XOR_ASSIGN, // '^='
    TK_BITWISE_OR_ASSIGN, // '|='
    TK_AMPERSAND, // '&'
    TK_LOGICAL_AND, // '&&'
    TK_BITWISE_OR, // '&'
    TK_LOGICAL_OR, // '||'
    TK_BITWISE_XOR, // '^'
    TK_SEMICOLON, // ';'
    TK_COMMA, // ','
    TK_COLON, // ':'
    TK_EXCLAMATION, // '!'
    TK_LPAREN, // '('
    TK_RPAREN, // ')'
    TK_LBRACE, // '{'
    TK_RBRACE, // '}'
    TK_LBRACKET, // '['
    TK_RBRACKET, // ']'
    TK_DOT, // '.'
    TK_PLUS, // '+'
    TK_MINUS, // '-'
    TK_ARROW, // '->'
    TK_STAR, // '*'
    TK_SLASH, // '/'
    TK_EOF,
    TK_BITWISE_NOT, // '~'
    TK_INCREMENT, // '++'
    TK_DECREMENT, // '--'
    TK_EQUALS, // '=='
    TK_NOT_EQUALS, // '!='
    TK_LESS_THAN, // '<'
    TK_GREATER_THAN, // '>'
    TK_LESS_THAN_EQUAL, // '<='
    TK_GREATER_THAN_EQUAL, // '>='
    TK_ELLIPSIS, // '...'
    TK_PERCENT, // '%'
    TK_LSHIFT, // '<<'
    TK_RSHIFT, // '>>'
    TK_TERNARY, // '?'
} token_kind_t;

static const char* token_kind_names[] = {
        [TK_NONE] = "TK_NONE",
        [TK_COMMENT] = "TK_COMMENT",
        [TK_NEWLINE] = "TK_NEWLINE",

        /* Preprocessor Directives */
        [TK_PP_INCLUDE] = "TK_PP_INCLUDE",
        [TK_PP_DEFINE] = "TK_PP_DEFINE",
        [TK_PP_UNDEF] = "TK_PP_UNDEF",
        [TK_PP_IFDEF] = "TK_PP_IFDEF",
        [TK_PP_LINE] = "TK_PP_LINE",

        /* Preprocessor tokens */
        [TK_HASH] = "TK_HASH", // stringification
        [TK_DOUBLE_HASH] = "TK_DOUBLE_HASH", // concatenation

        [TK_VOID] = "TK_VOID",
        [TK_CHAR] = "TK_CHAR",
        [TK_SHORT] = "TK_SHORT",
        [TK_INT] = "TK_INT",
        [TK_LONG] = "TK_LONG",
        [TK_FLOAT] = "TK_FLOAT",
        [TK_DOUBLE] = "TK_DOUBLE",
        [TK_SIGNED] = "TK_SIGNED",
        [TK_UNSIGNED] = "TK_UNSIGNED",
        [TK_BOOL] = "TK_BOOL",
        [TK_COMPLEX] = "TK_COMPLEX",
        [TK_STRUCT] = "TK_STRUCT",
        [TK_UNION] = "TK_UNION",
        [TK_ENUM] = "TK_ENUM",
        [TK_TYPEDEF] = "TK_TYPEDEF",
        [TK_STATIC] = "TK_STATIC",
        [TK_AUTO] = "TK_AUTO",
        [TK_REGISTER] = "TK_REGISTER",
        [TK_IF] = "TK_IF",
        [TK_ELSE] = "TK_ELSE",
        [TK_SWITCH] = "TK_SWITCH",
        [TK_CASE] = "TK_CASE",
        [TK_DEFAULT] = "TK_DEFAULT",
        [TK_GOTO] = "TK_GOTO",
        [TK_CONTINUE] = "TK_CONTINUE",
        [TK_BREAK] = "TK_BREAK",
        [TK_RETURN] = "TK_RETURN",
        [TK_WHILE] = "TK_WHILE",
        [TK_DO] = "TK_DO",
        [TK_FOR] = "TK_FOR",
        [TK_SIZEOF] = "TK_SIZEOF",
        [TK_CONST] = "TK_CONST",
        [TK_RESTRICT] = "TK_RESTRICT",
        [TK_VOLATILE] = "TK_VOLATILE",
        [TK_EXTERN] = "TK_EXTERN",
        [TK_INLINE] = "TK_INLINE",

        /* Identifier */
        [TK_IDENTIFIER] = "TK_IDENTIFIER",

        /* Constants and string literals */
        [TK_CHAR_LITERAL] = "TK_CHAR_LITERAL",
        [TK_STRING_LITERAL] = "TK_STRING_LITERAL",
        [TK_INTEGER_CONSTANT] = "TK_INTEGER_CONSTANT",
        [TK_FLOATING_CONSTANT] = "TK_FLOATING_CONSTANT",

        /* Punctuators */
        [TK_ASSIGN] = "TK_ASSIGN",
        [TK_MULTIPLY_ASSIGN] = "TK_MULTIPLY_ASSIGN",
        [TK_DIVIDE_ASSIGN] = "TK_DIVIDE_ASSIGN",
        [TK_MOD_ASSIGN] = "TK_MOD_ASSIGN",
        [TK_PLUS_ASSIGN] = "TK_PLUS_ASSIGN",
        [TK_MINUS_ASSIGN] = "TK_MINUS_ASSIGN",
        [TK_LSHIFT_ASSIGN] = "TK_LSHIFT_ASSIGN",
        [TK_RSHIFT_ASSIGN] = "TK_RSHIFT_ASSIGN",
        [TK_BITWISE_AND_ASSIGN] = "TK_BITWISE_AND_ASSIGN",
        [TK_BITWISE_XOR_ASSIGN] = "TK_BITWISE_XOR_ASSIGN",
        [TK_BITWISE_OR_ASSIGN] = "TK_BITWISE_OR_ASSIGN",
        [TK_AMPERSAND] = "TK_AMPERSAND",
        [TK_LOGICAL_AND] = "TK_LOGICAL_AND",
        [TK_BITWISE_OR] = "TK_BITWISE_OR",
        [TK_LOGICAL_OR] = "TK_LOGICAL_OR",
        [TK_BITWISE_XOR] = "TK_BITWISE_XOR",
        [TK_SEMICOLON] = "TK_SEMICOLON",
        [TK_COMMA] = "TK_COMMA",
        [TK_COLON] = "TK_COLON",
        [TK_EXCLAMATION] = "TK_EXCLAMATION",
        [TK_LPAREN] = "TK_LPAREN",
        [TK_RPAREN] = "TK_RPAREN",
        [TK_LBRACE] = "TK_LBRACE",
        [TK_RBRACE] = "TK_RBRACE",
        [TK_LBRACKET] = "TK_LBRACKET",
        [TK_RBRACKET] = "TK_RBRACKET",
        [TK_DOT] = "TK_DOT",
        [TK_PLUS] = "TK_PLUS",
        [TK_MINUS] = "TK_MINUS",
        [TK_ARROW] = "TK_ARROW",
        [TK_STAR] = "TK_STAR",
        [TK_SLASH] = "TK_SLASH",
        [TK_EOF] = "TK_EOF",
        [TK_BITWISE_NOT] = "TK_BITWISE_NOT",
        [TK_INCREMENT] = "TK_INCREMENT",
        [TK_DECREMENT] = "TK_DECREMENT",
        [TK_EQUALS] = "TK_EQUALS",
        [TK_NOT_EQUALS] = "TK_NOT_EQUALS",
        [TK_LESS_THAN] = "TK_LESS_THAN",
        [TK_GREATER_THAN] = "TK_GREATER_THAN",
        [TK_LESS_THAN_EQUAL] = "TK_LESS_THAN_EQUAL",
        [TK_GREATER_THAN_EQUAL] = "TK_GREATER_THAN_EQUAL",
        [TK_ELLIPSIS] = "TK_ELLIPSIS",
        [TK_PERCENT] = "TK_PERCENT",
        [TK_LSHIFT] = "TK_LSHIFT",
        [TK_RSHIFT] = "TK_RSHIFT",
        [TK_TERNARY] = "TK_TERNARY",
};

static const char* token_kind_display_names[] = {
        [TK_NONE] = "TK_NONE",
        [TK_COMMENT] = "TK_COMMENT",
        [TK_NEWLINE] = "TK_NEWLINE",

        /* Preprocessor Directives */
        [TK_PP_INCLUDE] = "TK_PP_INCLUDE",
        [TK_PP_DEFINE] = "TK_PP_DEFINE",
        [TK_PP_UNDEF] = "TK_PP_UNDEF",
        [TK_PP_IFDEF] = "TK_PP_IFDEF",
        [TK_PP_LINE] = "TK_PP_LINE",

        /* Preprocessor tokens */
        [TK_HASH] = "#", // stringification
        [TK_DOUBLE_HASH] = "##", // concatenation

        [TK_VOID] = "void",
        [TK_CHAR] = "char",
        [TK_SHORT] = "short",
        [TK_INT] = "int",
        [TK_LONG] = "long",
        [TK_FLOAT] = "float",
        [TK_DOUBLE] = "double",
        [TK_SIGNED] = "signed",
        [TK_UNSIGNED] = "unsigned",
        [TK_BOOL] = "bool",
        [TK_COMPLEX] = "complex",
        [TK_STRUCT] = "struct",
        [TK_UNION] = "union",
        [TK_ENUM] = "enum",
        [TK_TYPEDEF] = "typedef",
        [TK_STATIC] = "static",
        [TK_AUTO] = "auto",
        [TK_REGISTER] = "register",
        [TK_IF] = "if",
        [TK_ELSE] = "else",
        [TK_SWITCH] = "switch",
        [TK_CASE] = "case",
        [TK_DEFAULT] = "default",
        [TK_GOTO] = "goto",
        [TK_CONTINUE] = "continue",
        [TK_BREAK] = "break",
        [TK_RETURN] = "return",
        [TK_WHILE] = "while",
        [TK_DO] = "do",
        [TK_FOR] = "for",
        [TK_SIZEOF] = "sizeof",
        [TK_CONST] = "const",
        [TK_RESTRICT] = "restrict",
        [TK_VOLATILE] = "volatile",
        [TK_EXTERN] = "extern",
        [TK_INLINE] = "inline",

        /* Identifier */
        [TK_IDENTIFIER] = "<identifier>",

        /* Constants and string literals */
        [TK_CHAR_LITERAL] = "<char-literal>",
        [TK_STRING_LITERAL] = "<string-literal>",
        [TK_INTEGER_CONSTANT] = "<integer-constant>",
        [TK_FLOATING_CONSTANT] = "<floating-constant>",

        /* Punctuators */
        [TK_ASSIGN] = "=",
        [TK_MULTIPLY_ASSIGN] = "*=",
        [TK_DIVIDE_ASSIGN] = "/=",
        [TK_MOD_ASSIGN] = "%=",
        [TK_PLUS_ASSIGN] = "+=",
        [TK_MINUS_ASSIGN] = "-=",
        [TK_LSHIFT_ASSIGN] = "<<=",
        [TK_RSHIFT_ASSIGN] = ">>=",
        [TK_BITWISE_AND_ASSIGN] = "&=",
        [TK_BITWISE_XOR_ASSIGN] = "^=",
        [TK_BITWISE_OR_ASSIGN] = "|=",
        [TK_AMPERSAND] = "&",
        [TK_LOGICAL_AND] = "&&",
        [TK_BITWISE_OR] = "|",
        [TK_LOGICAL_OR] = "||",
        [TK_BITWISE_XOR] = "^",
        [TK_SEMICOLON] = ";",
        [TK_COMMA] = ",",
        [TK_COLON] = ":",
        [TK_EXCLAMATION] = "!",
        [TK_LPAREN] = "(",
        [TK_RPAREN] = ")",
        [TK_LBRACE] = "[",
        [TK_RBRACE] = "]",
        [TK_LBRACKET] = "{",
        [TK_RBRACKET] = "}",
        [TK_DOT] = ".",
        [TK_PLUS] = "+",
        [TK_MINUS] = "-",
        [TK_ARROW] = "->",
        [TK_STAR] = "*",
        [TK_SLASH] = "/",
        [TK_EOF] = "EOF",
        [TK_BITWISE_NOT] = "~",
        [TK_INCREMENT] = "++",
        [TK_DECREMENT] = "--",
        [TK_EQUALS] = "==",
        [TK_NOT_EQUALS] = "!=",
        [TK_LESS_THAN] = "<",
        [TK_GREATER_THAN] = ">",
        [TK_LESS_THAN_EQUAL] = "<=",
        [TK_GREATER_THAN_EQUAL] = ">=",
        [TK_ELLIPSIS] = "...",
        [TK_PERCENT] = "%",
        [TK_LSHIFT] = "<<",
        [TK_RSHIFT] = ">>",
        [TK_TERNARY] = "?",
};

struct ReservedWord {
    char* word;
    enum TokenKind kind;
};

static struct ReservedWord RESERVED_WORDS[] = {
//        {"auto", TK_AUTO},
        {"break",    TK_BREAK},
        {"case",     TK_CASE},
        {"char",     TK_CHAR},
        {"const",    TK_CONST},
        {"continue", TK_CONTINUE},
        {"default",  TK_DEFAULT},
        {"do",       TK_DO},
        {"double",   TK_DOUBLE},
        {"else",     TK_ELSE},
        {"enum",     TK_ENUM},
        {"extern",   TK_EXTERN},
        {"float",    TK_FLOAT},
        {"for",      TK_FOR},
        {"goto",     TK_GOTO},
        {"if",       TK_IF},
        {"inline",   TK_INLINE},
        {"int",      TK_INT},
        {"long",     TK_LONG},
        {"register", TK_REGISTER},
        {"restrict", TK_RESTRICT},
        {"return",   TK_RETURN},
        {"short",    TK_SHORT},
        {"signed",   TK_SIGNED},
        {"sizeof", TK_SIZEOF},
        {"static", TK_STATIC},
        {"struct",   TK_STRUCT},
        {"switch",   TK_SWITCH},
        {"typedef",  TK_TYPEDEF},
        {"union",    TK_UNION},
        {"unsigned", TK_UNSIGNED},
        {"void",     TK_VOID},
        {"volatile", TK_VOLATILE},
        {"while",    TK_WHILE},
//        {"_Alignas", TK_ALIGNAS},
//        {"_Alignof", TK_ALIGNOF},
//        {"_Atomic", TK_ATOMIC},
        {"_Bool", TK_BOOL},
//        {"_Complex", TK_COMPLEX},
//        {"_Generic", TK_GENERIC},
//        {"_Imaginary", TK_IMAGINARY},
//        {"_Noreturn", TK_NORETURN},
//        {"_Static_assert", TK_STATIC_ASSERT},
//        {"_Thread_local", TK_THREAD_LOCAL}
};

static struct ReservedWord PREPROCESSOR_DIRECTIVES[] = {
        {"include", TK_PP_INCLUDE},
        {"define", TK_PP_DEFINE},
        {"undef", TK_PP_UNDEF},
        {"ifdef", TK_PP_IFDEF},
        {"line", TK_PP_LINE}
};

typedef struct SourcePosition {
    const char* path;
    uint32_t line;
    uint32_t column;
} source_position_t;

typedef struct SourceSpan {
    source_position_t start;
    source_position_t end;
} source_span_t;

typedef struct Token {
    enum TokenKind kind;
    const char* value;
    struct SourcePosition position;
} token_t;

typedef struct TokenVector {
    token_t* buffer;
    size_t size;
    size_t capacity;
} token_vector_t;

void append_token(token_t **buffer, size_t *size, size_t *capacity, token_t token);

typedef struct TokenPtrVector {
    token_t** buffer;
    size_t size;
    size_t capacity;
} token_ptr_vector_t;

void append_token_ptr(token_t ***buffer, size_t *size, size_t *capacity, token_t *token);

typedef struct TokenNode {
    token_t token;
    struct TokenNode* prev;
    struct TokenNode* next;
} token_node_t;

typedef struct MacroDefinition {
    const char* name;
    token_vector_t parameters; // positional parameters, if any
    bool variadic;
    token_vector_t tokens;
} macro_definition_t;

/**
 * Context shared by all lexers.
 * This is used to store global state, such as the list of include paths and macro definitions.
 */
typedef struct LexerGlobalContext {
    string_vector_t* user_include_paths;
    string_vector_t* system_include_paths;
    /**
     * Hash table of macro definitions.
     * The key is the macro name, and the value is a pointer to a heap allocated macro_definition_t.
     */
    hash_table_t macro_definitions;
    /**
     * Set to true when the lexer is parsing a macro definition.
     */
    bool disable_macro_expansion;
} lexer_global_context_t;

struct Lexer;
typedef struct Lexer {
    const char* input_path;
    const char* input;
    size_t input_offset;
    size_t input_len;
    source_position_t position;
    lexer_global_context_t* global_context;
    /**
     * A pointer to a child lexer (if any exist).
     * Mainly used for handling #includes directives, which create a new lexer to parse the included file.
     */
    struct Lexer* child;
    /**
     * Tokens that have been parsed but not yet consumed.
     * Generally, this will be the tokens that were parsed by the preprocessor as part of macro expansion,
     * as the lexer will only scan one token at a time from the input (includes directives are handled by nested lexers).
     */
    token_node_t* pending_tokens;
} lexer_t;

lexer_t linit(
        const char* input_path,
        const char* input,
        size_t input_len,
        lexer_global_context_t* global_context
);
char lpeek(struct Lexer* lexer, unsigned int count);
char ladvance(struct Lexer* lexer);
token_t lscan(lexer_t* lexer);

#endif //C_COMPILER_LEXER_H
