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
                    error->value.use_of_undeclared_identifier.identifier);
            break;
        }
        case ERR_INVALID_BINARY_EXPRESSION_OPERANDS: {
            // TODO: print types
            fprintf(stderr, ERROR_PREFIX "Invalid operands to binary expression: %s\n",
                    error->location.path, error->location.line, error->location.column,
                    error->value.invalid_binary_expression_operands.operator);
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
                    error->value.redefinition_of_symbol.redefinition->value);
            break;
        }
        case ERR_REDEFINITION_OF_TAG: {
            fprintf(stderr, ERROR_PREFIX "Redefinition oftag '%s'\n",
                    error->location.path, error->location.line, error->location.column,
                    error->value.redefinition_of_tag.redefinition->value);
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
        case ERR_INVALID_MEMBER_ACCESS_TARGET: {
            const type_t *type = error->value.invalid_member_access_target.type;
            token_t operator = error->value.invalid_member_access_target.operator;
            if (type->kind != TYPE_STRUCT_OR_UNION &&
                (type->kind != TYPE_POINTER || type->value.pointer.base->kind != TYPE_STRUCT_OR_UNION)) {
                // TODO: print type
                fprintf(stderr, ERROR_PREFIX "Member reference base type is not a struct or struct pointer\n",
                    error->location.path, error->location.line, error->location.column);
            } else if (operator.kind == TK_ARROW) {
                fprintf(stderr, ERROR_PREFIX "Member reference type is not a pointer, but accessed with '%s'\n",
                    error->location.path, error->location.line, error->location.column, operator.value);
            } else {
                fprintf(stderr, ERROR_PREFIX "Member reference type is a pointer, but accessed with '%s'\n",
                    error->location.path, error->location.line, error->location.column, operator.value);
            }
            break;
        }
        case ERR_INVALID_STRUCT_FIELD_REFERENCE: {
            const type_t *type = error->value.invalid_struct_field_reference.type;
            const token_t field = error->value.invalid_struct_field_reference.field;
            assert(type->kind == TYPE_STRUCT_OR_UNION);
            const char *struct_or_union = type->value.struct_or_union.is_union ? "union" : "struct";
            const char *identifier = "anonymous";
            if (type->value.struct_or_union.identifier != NULL)
                identifier = type->value.struct_or_union.identifier->value;
            fprintf(stderr, ERROR_PREFIX "%s %s has no field named %s\n",
                error->location.path, error->location.line, error->location.column,
                struct_or_union, identifier, field.value);
            break;
        }
        case ERR_USE_OF_UNDECLARED_LABEL: {
            fprintf(stderr, ERROR_PREFIX "Use of undeclared label %s\n",
                error->location.path, error->location.line, error->location.column,
                error->value.use_of_undeclared_label.label.value);
             break;
        }
        case ERR_REDEFINITION_OF_LABEL: {
            const source_position_t prev = error->value.redefinition_of_label.previous_definition.position;
            fprintf(stderr, ERROR_PREFIX "Redefinition of label %s, previous definition: %s:%d:%d\n",
                error->location.path, error->location.line, error->location.column,
                error->value.redefinition_of_label.label.value,
                prev.path, prev.line, prev.column);
            break;
        }
        case ERR_BREAK_OUTSIDE_OF_LOOP_OR_SWITCH: {
            fprintf(stderr, ERROR_PREFIX "break statement is only allowed inside of the body of a loop or switch case\n",
                error->location.path, error->location.line, error->location.column);
            break;
        }
        case ERR_CONTINUE_OUTSIDE_OF_LOOP: {
            fprintf(stderr, ERROR_PREFIX "continue statement is only allowed inside the body of a loop\n",
                error->location.path, error->location.line, error->location.column);
            break;
        }
        case ERR_CANNOT_INCREMENT_DECREMENT_TYPE: {
            // TODO: display type
            fprintf(stderr, ERROR_PREFIX "cannot increment/decrement value of type\n",
                error->location.path, error->location.line, error->location.column);
            break;
        }
        case ERR_INVALID_UNARY_ARITHMETIC_OPERATOR_TYPE: {
            const char *suffix;
            switch (error->value.invalid_unary_arithmetic_operator_type.operator.kind) {
                case TK_EXCLAMATION:
                    // unary logical not
                    suffix = ", operand must have scalar type";
                    break;
                case TK_BITWISE_NOT:
                    suffix = ", operand must have integer type";
                    break;
                case TK_PLUS:
                case TK_MINUS:
                    suffix = ", operand must have arithmetic type";
                    break;
                default:
                    suffix = "";
            }
            fprintf(stderr, ERROR_PREFIX "Invalid operand type for unary operator '%s'%s\n",
                    error->location.path, error->location.line, error->location.column,
                    error->value.invalid_unary_arithmetic_operator_type.operator.value, suffix);
            break;
        }
        case ERR_NON_VOID_FUNCTION_RETURNS_VOID: {
            fprintf(stderr, ERROR_PREFIX "Returning void from non-void function %s",
                error->location.path, error->location.line, error->location.column,
                error->value.non_void_function_returns_void.fn->identifier->value);
            break;
        }
        case ERR_INVALID_SWITCH_EXPRESSION_TYPE: {
            // TODO: print type
            fprintf(stderr, ERROR_PREFIX "Switch statement expression must have integer type\n",
                error->location.path, error->location.line, error->location.column);
            break;
        }
        case ERR_INVALID_CASE_EXPRESSION: {
            fprintf(stderr, ERROR_PREFIX "Case statement expression must be constant and have integer type\n",
                error->location.path, error->location.line, error->location.column);
            break;
        }
        case ERR_CASE_STATEMENT_OUTSIDE_OF_SWITCH: {
            fprintf(stderr, ERROR_PREFIX "Case/default statement outside of switch statement\n",
                error->location.path, error->location.line, error->location.column);
            break;
        }
        case ERR_DUPLICATE_SWITCH_CASE: {
            // TODO: show location of the first case as extra info?
            if (error->value.duplicate_switch_case.keyword->kind == TK_DEFAULT) {
                fprintf(stderr, ERROR_PREFIX "Duplicate default case in switch statement\n",
                    error->location.path, error->location.line, error->location.column);
            } else {
                fprintf(stderr, ERROR_PREFIX "Duplicate case in switch statement with value %lli\n",
                    error->location.path, error->location.line, error->location.column,
                    error->value.duplicate_switch_case.value);
            }
            break;
        }
        case ERR_EXPECTED_CONSTANT_EXPRESSION: {
            fprintf(stderr, ERROR_PREFIX "Expected constant expression\n",
                    error->location.path, error->location.line, error->location.column);
            break;
        }
        case ERR_ENUMERATION_CONSTANT_MUST_HAVE_INTEGER_TYPE: {
            fprintf(stderr, ERROR_PREFIX "Expression defining the value of an enumeration constant must have integer type\n",
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
