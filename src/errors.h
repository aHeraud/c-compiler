#ifndef C_COMPILER_ERRORS_H
#define C_COMPILER_ERRORS_H

#include "lexer.h"
#include "types.h"

typedef enum CompilationErrorKind {
    /* Lexical Errors */
    ERR_INVALID_TOKEN,

    /* Syntax Errors */

    /* Semantic Errors */
    ERR_USE_OF_UNDECLARED_IDENTIFIER,
    ERR_INVALID_BINARY_EXPRESSION_OPERANDS,
    ERR_INVALID_ASSIGNMENT_TARGET,
    ERR_REDEFINITION_OF_SYMBOL,
    ERR_INVALID_INITIALIZER_TYPE,
} compilation_error_kind_t;

typedef struct CompilationError {
    compilation_error_kind_t kind;
    /**
     * The location of the error in the source code.
     */
    source_position_t location;
    union {
        struct {
            const char *identifier;
        } use_of_undeclared_identifier;
        struct {
            // could also include the positions of the operands for better error messages
            const char *operator;
            const type_t *left_type;
            const type_t *right_type;
        } invalid_binary_expression_operands;
        struct {
            const token_t *redefinition;
            const token_t *previous_definition;
        } redefinition_of_symbol;
        struct {
            const token_t *target;
            const type_t *lhs_type;
            const type_t *rhs_type;
        } invalid_initializer_type;
    };
} compilation_error_t;

typedef struct CompilationErrorVector {
    compilation_error_t *buffer;
    size_t size;
    size_t capacity;
} compilation_error_vector_t;

void append_compilation_error(compilation_error_vector_t *errors, compilation_error_t error);

#endif //C_COMPILER_ERRORS_H
