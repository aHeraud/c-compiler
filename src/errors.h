#ifndef C_COMPILER_ERRORS_H
#define C_COMPILER_ERRORS_H

#include "ast.h"
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
    ERR_REDEFINITION_OF_TAG,
    ERR_INVALID_INITIALIZER_TYPE,
    ERR_GLOBAL_INITIALIZER_NOT_CONSTANT,
    ERR_INVALID_IF_CONDITION_TYPE,
    ERR_INVALID_TERNARY_CONDITION_TYPE,
    ERR_INVALID_TERNARY_EXPRESSION_OPERANDS,
    ERR_CALL_TARGET_NOT_FUNCTION,
    ERR_CALL_ARGUMENT_COUNT_MISMATCH,
    ERR_INVALID_LOOP_CONDITION_TYPE,
    ERR_INVALID_LOGICAL_BINARY_EXPRESSION_OPERAND_TYPE,
    ERR_INVALID_CONVERSION_TO_BOOLEAN,
    ERR_UNARY_INDIRECTION_OPERAND_NOT_PTR_TYPE,
    ERR_INVALID_SUBSCRIPT_TARGET,
    ERR_INVALID_SUBSCRIPT_TYPE,
    ERR_INVALID_MEMBER_ACCESS_TARGET,
    ERR_INVALID_STRUCT_FIELD_REFERENCE,
    ERR_USE_OF_UNDECLARED_LABEL,
    ERR_REDEFINITION_OF_LABEL,
    ERR_BREAK_OUTSIDE_OF_LOOP_OR_SWITCH_CASE,
    ERR_CONTINUE_OUTSIDE_OF_LOOP,
    ERR_CANNOT_INCREMENT_DECREMENT_TYPE,
    ERR_INVALID_UNARY_ARITHMETIC_OPERATOR_TYPE,
    ERR_NON_VOID_FUNCTION_RETURNS_VOID,
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
            const token_t *redefinition;
            const token_t *previous_definition;
        } redefinition_of_tag;
        struct {
            const token_t *target;
            const type_t *lhs_type;
            const type_t *rhs_type;
        } invalid_initializer_type;
        struct {
            const declaration_t *declaration;
        } global_initializer_not_constant;
        struct {
            const type_t *type;
        } call_target_not_function;
        struct {
            // TODO: could also include the prototype/definition of the function
            size_t expected;
            size_t actual;
        } call_argument_count_mismatch;
        struct {
            const type_t *type;
        } invalid_ternary_condition_type;
        struct {
            const type_t *true_type;
            const type_t *false_type;
        } invalid_ternary_expression_operands;
        struct {
            const type_t *type;
        } invalid_loop_condition_type;
        struct {
            const type_t *type;
        } invalid_logical_binary_expression_operand_type;
        struct {
            const type_t *type;
        } invalid_conversion_to_boolean;
        struct {
            const type_t *type;
            token_t operator;
        } invalid_member_access_target;
        struct {
            const type_t *type;
            token_t field;
        } invalid_struct_field_reference;
        struct {
            token_t label;
        } use_of_undeclared_label;
        struct {
            token_t label;
            token_t previous_definition;
        } redefinition_of_label;
        struct {
            token_t keyword;
        } break_outside_of_loop_or_switch_case;
        struct {
            token_t keyword;
        } continue_outside_of_loop;
        struct {
            const type_t *type;
        } cannot_increment_decrement_type;
        struct {
            const type_t *type;
            token_t operator;
        } invalid_unary_arithmetic_operator_type;
        struct {
            const token_t *ret;
            const function_definition_t *fn;
        } non_void_function_returns_void;
    } value;
} compilation_error_t;

typedef struct CompilationErrorVector {
    compilation_error_t *buffer;
    size_t size;
    size_t capacity;
} compilation_error_vector_t;

void append_compilation_error(compilation_error_vector_t *errors, compilation_error_t error);
void print_compilation_error(const compilation_error_t *error);

#endif //C_COMPILER_ERRORS_H
