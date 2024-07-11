#include <stdio.h>
#include "errors.h"
#include "util/vectors.h"

#define ERROR_PREFIX "%s:%d:%d: error: "

void append_compilation_error(compilation_error_vector_t *errors, compilation_error_t error) {
    VEC_APPEND(errors, error);
}

void print_compilation_error(const compilation_error_t *error) {
    switch (error->kind) {
        case ERR_INVALID_TOKEN: {
            // TODO: Not currently used, missing position/contents
            fprintf(stderr, "Invalid token\n");
            break;
        }
        case ERR_USE_OF_UNDECLARED_IDENTIFIER: {
            fprintf(stderr, ERROR_PREFIX "Use of undeclared identifier '%s'\n",
                    error->location.path, error->location.line, error->location.column,
                    error->use_of_undeclared_identifier.identifier);
            break;
        }
        case ERR_INVALID_BINARY_EXPRESSION_OPERANDS: {
            // TODO: print types
            fprintf(stderr, ERROR_PREFIX "Invalid operands to binary expression: %s\n",
                    error->location.path, error->location.line, error->location.column,
                    error->invalid_binary_expression_operands.operator);
            break;
        }
        case ERR_INVALID_ASSIGNMENT_TARGET: {
            fprintf(stderr, ERROR_PREFIX "Invalid assignment target\n",
                    error->location.path, error->location.line, error->location.column);
            break;
        }
        case ERR_REDEFINITION_OF_SYMBOL: {
            fprintf(stderr, ERROR_PREFIX "Redefinition of symbol '%s'\n",
                    error->location.path, error->location.line, error->location.column,
                    error->redefinition_of_symbol.redefinition->value);
            break;
        }
        case ERR_REDEFINITION_OF_TAG: {
            fprintf(stderr, ERROR_PREFIX "Redefinition oftag '%s'\n",
                    error->location.path, error->location.line, error->location.column,
                    error->redefinition_of_tag.redefinition->value);
            break;
        }
        case ERR_INVALID_INITIALIZER_TYPE: {
            fprintf(stderr, ERROR_PREFIX "Invalid initializer type\n",
                    error->location.path, error->location.line, error->location.column);
            break;
        }
        case ERR_GLOBAL_INITIALIZER_NOT_CONSTANT: {
            fprintf(stderr, ERROR_PREFIX "Global initializer is not a constant expression\n",
                    error->location.path, error->location.line, error->location.column);
            break;
        }
        case ERR_INVALID_IF_CONDITION_TYPE: {
            fprintf(stderr, ERROR_PREFIX "Invalid if condition type\n",
                    error->location.path, error->location.line, error->location.column);
            break;
        }
        case ERR_INVALID_TERNARY_CONDITION_TYPE: {
            fprintf(stderr, ERROR_PREFIX "Invalid ternary condition type\n",
                    error->location.path, error->location.line, error->location.column);
            break;
        }
        case ERR_INVALID_TERNARY_EXPRESSION_OPERANDS: {
            fprintf(stderr, ERROR_PREFIX "Invalid ternary expression operands\n",
                    error->location.path, error->location.line, error->location.column);
            break;
        }
        case ERR_CALL_TARGET_NOT_FUNCTION: {
            fprintf(stderr, ERROR_PREFIX "Call target is not a function\n",
                    error->location.path, error->location.line, error->location.column);
            break;
        }
        case ERR_CALL_ARGUMENT_COUNT_MISMATCH: {
            fprintf(stderr, ERROR_PREFIX "Call argument count mismatch\n",
                    error->location.path, error->location.line, error->location.column);
            break;
        }
        case ERR_INVALID_LOOP_CONDITION_TYPE: {
            fprintf(stderr, ERROR_PREFIX "Invalid loop condition type\n",
                    error->location.path, error->location.line, error->location.column);
            break;
        }
        case ERR_INVALID_UNARY_NOT_OPERAND_TYPE: {
            fprintf(stderr, ERROR_PREFIX "Invalid operand type for unary operator\n",
                    error->location.path, error->location.line, error->location.column);
            break;
        }
        case ERR_INVALID_LOGICAL_BINARY_EXPRESSION_OPERAND_TYPE: {
            fprintf(stderr, ERROR_PREFIX "Invalid operand type for logical binary operator\n",
                    error->location.path, error->location.line, error->location.column);
            break;
        }
        case ERR_INVALID_CONVERSION_TO_BOOLEAN: {
            fprintf(stderr, ERROR_PREFIX "Invalid conversion to boolean\n",
                    error->location.path, error->location.line, error->location.column);
            break;
        }
        case ERR_UNARY_INDIRECTION_OPERAND_NOT_PTR_TYPE: {
            fprintf(stderr, ERROR_PREFIX "Indirection operand is not a pointer type\n",
                    error->location.path, error->location.line, error->location.column);
            break;
        }
        case ERR_INVALID_SUBSCRIPT_TARGET: {
            fprintf(stderr, ERROR_PREFIX "Invalid subscript target\n",
                    error->location.path, error->location.line, error->location.column);
            break;
        }
        case ERR_INVALID_SUBSCRIPT_TYPE: {
            fprintf(stderr, ERROR_PREFIX "Invalid subscript type\n",
                    error->location.path, error->location.line, error->location.column);
            break;
        }
        default: {
            fprintf(stderr, ERROR_PREFIX "Unknown error kind\n",
                    error->location.path, error->location.line, error->location.column);
            assert(false);
            exit(1);
        }
    }
}
