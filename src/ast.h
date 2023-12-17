#ifndef C_COMPILER_AST_H
#define C_COMPILER_AST_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "lexer.h"

typedef struct Expression expression_t;

typedef struct PrimaryExpression {
    enum {
        PE_IDENTIFIER,
        PE_CONSTANT,
        PE_STRING_LITERAL,
        PE_EXPRESSION, // e.g. parenthesized expression "(1 + 2)"
    } type;
    union {
        token_t token; // value of an identifier, constant, or string literal
        expression_t* expression;
    };
} primary_expression_t;

typedef enum BinaryOperator {
    BINARY_ADD,
    BINARY_SUBTRACT,
    BINARY_MULTIPLY,
    BINARY_DIVIDE,
    BINARY_MODULO,
    BINARY_BITWISE_AND,
    BINARY_BITWISE_OR,
    BINARY_BITWISE_XOR,
    BINARY_LOGICAL_AND,
    BINARY_LOGICAL_OR,
    BINARY_SHIFT_LEFT,
    BINARY_SHIFT_RIGHT,
    BINARY_EQUAL,
    BINARY_NOT_EQUAL,
    BINARY_LESS_THAN,
    BINARY_LESS_THAN_OR_EQUAL,
    BINARY_GREATER_THAN,
    BINARY_GREATER_THAN_OR_EQUAL,
    BINARY_COMMA,
    BINARY_ASSIGN,
    BINARY_ADD_ASSIGN,
    BINARY_SUBTRACT_ASSIGN,
    BINARY_MULTIPLY_ASSIGN,
    BINARY_DIVIDE_ASSIGN,
    BINARY_MODULO_ASSIGN,
    BINARY_BITWISE_AND_ASSIGN,
    BINARY_BITWISE_OR_ASSIGN,
    BINARY_BITWISE_XOR_ASSIGN,
    BINARY_SHIFT_LEFT_ASSIGN,
    BINARY_SHIFT_RIGHT_ASSIGN,
} binary_operator_t;

typedef struct BinaryExpression {
    expression_t* left;
    expression_t* right;
    binary_operator_t operator;
} binary_expression_t;

typedef struct UnaryExpression {
    expression_t* operand;
    enum {
        UNARY_ADDRESS_OF,
        UNARY_DEREFERENCE,
        UNARY_PLUS,
        UNARY_MINUS,
        UNARY_BITWISE_NOT,
        UNARY_LOGICAL_NOT,
        UNARY_PRE_INCREMENT,
        UNARY_PRE_DECREMENT,
        UNARY_POST_INCREMENT,
        UNARY_POST_DECREMENT,
    } operator;
} unary_expression_t;

typedef struct TernaryExpression {
    expression_t* condition;
    expression_t* true_expression;
    expression_t* false_expression;
} ternary_expression_t;

typedef struct Expression {
    source_span_t span;
    enum {
        EXPRESSION_PRIMARY,
        EXPRESSION_BINARY,
        EXPRESSION_UNARY,
        EXPRESSION_TERNARY,
    } type;
    union {
        primary_expression_t primary;
        binary_expression_t binary;
        unary_expression_t unary;
        ternary_expression_t ternary;
    };
} expression_t;

typedef enum StorageClassSpecifier {
    STORAGE_CLASS_SPECIFIER_TYPEDEF,
    STORAGE_CLASS_SPECIFIER_EXTERN,
    STORAGE_CLASS_SPECIFIER_STATIC,
    STORAGE_CLASS_SPECIFIER_AUTO,
    STORAGE_CLASS_SPECIFIER_REGISTER,
} storage_class_specifier_t;

static const char* storage_class_specifier_names[] = {
        [STORAGE_CLASS_SPECIFIER_TYPEDEF] = "typedef",
        [STORAGE_CLASS_SPECIFIER_EXTERN] = "extern",
        [STORAGE_CLASS_SPECIFIER_STATIC] = "static",
        [STORAGE_CLASS_SPECIFIER_AUTO] = "auto",
        [STORAGE_CLASS_SPECIFIER_REGISTER] = "register",
};

// TODO: struct, union, enum, typedef
typedef enum TypeSpecifier {
    TYPE_SPECIFIER_VOID,
    TYPE_SPECIFIER_CHAR,
    TYPE_SPECIFIER_SHORT,
    TYPE_SPECIFIER_INT,
    TYPE_SPECIFIER_LONG,
    TYPE_SPECIFIER_FLOAT,
    TYPE_SPECIFIER_DOUBLE,
    TYPE_SPECIFIER_SIGNED,
    TYPE_SPECIFIER_UNSIGNED,
    TYPE_SPECIFIER_BOOL,
    TYPE_SPECIFIER_COMPLEX,
    TYPE_SPECIFIER_STRUCT,
    TYPE_SPECIFIER_UNION,
    TYPE_SPECIFIER_ENUM,
    TYPE_SPECIFIER_TYPEDEF_NAME,
} type_specifier_t;

static const char* type_specifier_names[] = {
        [TYPE_SPECIFIER_VOID] = "void",
        [TYPE_SPECIFIER_CHAR] = "char",
        [TYPE_SPECIFIER_SHORT] = "short",
        [TYPE_SPECIFIER_INT] = "int",
        [TYPE_SPECIFIER_LONG] = "long",
        [TYPE_SPECIFIER_FLOAT] = "float",
        [TYPE_SPECIFIER_DOUBLE] = "double",
        [TYPE_SPECIFIER_SIGNED] = "signed",
        [TYPE_SPECIFIER_UNSIGNED] = "unsigned",
        [TYPE_SPECIFIER_BOOL] = "bool",
        [TYPE_SPECIFIER_COMPLEX] = "complex",
        [TYPE_SPECIFIER_STRUCT] = "struct",
        [TYPE_SPECIFIER_UNION] = "union",
        [TYPE_SPECIFIER_ENUM] = "enum",
        [TYPE_SPECIFIER_TYPEDEF_NAME] = "typedef",
};

typedef enum TypeQualifier {
    TYPE_QUALIFIER_CONST,
    TYPE_QUALIFIER_RESTRICT,
    TYPE_QUALIFIER_VOLATILE,
} type_qualifier_t;

static const char* type_qualifier_names[] = {
        [TYPE_QUALIFIER_CONST] = "const",
        [TYPE_QUALIFIER_RESTRICT] = "restrict",
        [TYPE_QUALIFIER_VOLATILE] = "volatile",
};

typedef enum FunctionSpecifier {
    FUNCTION_SPECIFIER_INLINE,
} function_specifier_t;

static const char* function_specifier_names[] = {
        [FUNCTION_SPECIFIER_INLINE] = "inline",
};



#endif //C_COMPILER_AST_H
