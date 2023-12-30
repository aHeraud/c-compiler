#ifndef C_COMPILER_AST_H
#define C_COMPILER_AST_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "lexer.h"
#include "types.h"

typedef struct Statement statement_t;
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

typedef enum BinaryArithmeticOperator {
    BINARY_ARITHMETIC_ADD,
    BINARY_ARITHMETIC_SUBTRACT,
    BINARY_ARITHMETIC_MULTIPLY,
    BINARY_ARITHMETIC_DIVIDE,
    BINARY_ARITHMETIC_MODULO,
} binary_arithmetic_operator_t;

typedef enum BinaryBitwiseOperator {
    BINARY_BITWISE_AND,
    BINARY_BITWISE_OR,
    BINARY_BITWISE_XOR,
    BINARY_BITWISE_SHIFT_LEFT,
    BINARY_BITWISE_SHIFT_RIGHT,
} binary_bitwise_operator_t;

typedef enum BinaryLogicalOperator {
    BINARY_LOGICAL_AND,
    BINARY_LOGICAL_OR,
} binary_logical_operator_t;

typedef enum BinaryComparisonOperator {
    BINARY_COMPARISON_EQUAL,
    BINARY_COMPARISON_NOT_EQUAL,
    BINARY_COMPARISON_LESS_THAN,
    BINARY_COMPARISON_LESS_THAN_OR_EQUAL,
    BINARY_COMPARISON_GREATER_THAN,
    BINARY_COMPARISON_GREATER_THAN_OR_EQUAL,
} binary_comparison_operator_t;

typedef enum BinaryAssignmentOperator {
    BINARY_ASSIGN,
    BINARY_BITWISE_AND_ASSIGN,
    BINARY_BITWISE_OR_ASSIGN,
    BINARY_BITWISE_XOR_ASSIGN,
    BINARY_SHIFT_LEFT_ASSIGN,
    BINARY_SHIFT_RIGHT_ASSIGN,
    BINARY_ADD_ASSIGN,
    BINARY_SUBTRACT_ASSIGN,
    BINARY_MULTIPLY_ASSIGN,
    BINARY_DIVIDE_ASSIGN,
    BINARY_MODULO_ASSIGN,
} binary_assignment_operator_t;

typedef struct BinaryExpression {
    enum {
        BINARY_ARITHMETIC,
        BINARY_ASSIGNMENT,
        BINARY_COMMA,
        BINARY_COMPARISON,
        BINARY_BITWISE,
        BINARY_LOGICAL,
    } type;
    expression_t *left;
    expression_t *right;
    const token_t *operator;
    union {
        binary_arithmetic_operator_t arithmetic_operator;
        binary_bitwise_operator_t bitwise_operator;
        binary_logical_operator_t logical_operator;
        binary_comparison_operator_t comparison_operator;
        binary_assignment_operator_t assignment_operator;
    };
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

typedef struct CallExpression {
    expression_t* callee;
    ptr_vector_t arguments;
} call_expression_t;

typedef struct ArraySubscriptExpression {
    expression_t* array;
    expression_t* index;
} array_subscript_expression_t;

typedef struct MemberAccessExpression {
    expression_t* struct_or_union;
    token_t operator; // "." or "->"
    token_t member; // identifier
} member_access_expression_t;

typedef struct Expression {
    source_span_t span;
    enum {
        EXPRESSION_PRIMARY,
        EXPRESSION_BINARY,
        EXPRESSION_UNARY,
        EXPRESSION_TERNARY,
        EXPRESSION_CALL,
        EXPRESSION_ARRAY_SUBSCRIPT,
        EXPRESSION_MEMBER_ACCESS,
    } type;
    union {
        primary_expression_t primary;
        binary_expression_t binary;
        unary_expression_t unary;
        ternary_expression_t ternary;
        call_expression_t call;
        array_subscript_expression_t array_subscript;
        member_access_expression_t member_access;
    };
} expression_t;

typedef enum StorageClassSpecifier {
    STORAGE_CLASS_SPECIFIER_TYPEDEF,
    STORAGE_CLASS_SPECIFIER_EXTERN,
    STORAGE_CLASS_SPECIFIER_STATIC,
    STORAGE_CLASS_SPECIFIER_AUTO,
    STORAGE_CLASS_SPECIFIER_REGISTER,
} storage_class_specifier_t;

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

typedef enum TypeQualifier {
    TYPE_QUALIFIER_CONST,
    TYPE_QUALIFIER_RESTRICT,
    TYPE_QUALIFIER_VOLATILE,
} type_qualifier_t;

typedef enum FunctionSpecifier {
    FUNCTION_SPECIFIER_INLINE,
} function_specifier_t;

typedef struct Statement {
    enum {
        STATEMENT_COMPOUND,
        STATEMENT_EMPTY,
        STATEMENT_EXPRESSION,
        STATEMENT_RETURN,
    } type;
    union {
        struct {
            token_t *open_brace;
            ptr_vector_t block_items;
        } compound;
        expression_t *expression;
        struct {
            token_t *keyword;
            expression_t *expression;
        } return_;
    };
    token_t *terminator;
} statement_t;

// Very primitive definition for now, add support for parameters, etc. later
typedef struct FunctionDefinition {
    type_t return_type;
    token_t *identifier;
    statement_t *body;
} function_definition_t;

typedef struct Declaration {
    type_t *type;
    token_t *identifier;
    expression_t *initializer;
} declaration_t;

typedef struct BlockItem {
    enum {
        BLOCK_ITEM_STATEMENT,
        BLOCK_ITEM_DECLARATION,
    } type;
    union {
        statement_t *statement;
        declaration_t *declaration;
    };
} block_item_t;

#endif //C_COMPILER_AST_H
