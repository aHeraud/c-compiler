#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <string.h>

#include "ast.h"
#include "codegen.h"
#include "types.h"
#include "util/hashtable.h"

LLVMValueRef convert_to_type(codegen_context_t *context, LLVMValueRef value, const type_t *from, const type_t *to);

codegen_context_t *codegen_init(const char* module_name) {
    codegen_context_t *context = malloc(sizeof(codegen_context_t));
    *context = (codegen_context_t) {
        .global_scope = (scope_t) {
            .symbols = hash_table_create(1000),
            .parent = NULL,
        },
        .current_scope = &context->global_scope,
        .current_function = NULL,
        .llvm_module = LLVMModuleCreateWithName(module_name),
        .llvm_current_function = NULL,
        .llvm_builder = NULL,
        .llvm_current_block = NULL,
    };

    return context;
}

void codegen_finalize(codegen_context_t *context, const char* output_filename) {
    char *message;
    LLVMVerifyModule(context->llvm_module, LLVMAbortProcessAction, &message);
    LLVMDisposeMessage(message);
    LLVMPrintModuleToFile(context->llvm_module, output_filename, &message);
    LLVMDisposeModule(context->llvm_module);
    free(context);
}

void enter_scope(codegen_context_t *context) {
    assert(context != NULL);
    scope_t *scope = malloc(sizeof(scope_t));
    *scope = (scope_t) {
        .symbols = hash_table_create(1000),
        .parent = context->current_scope,
    };
    context->current_scope = scope;
}

void leave_scope(codegen_context_t *context) {
    assert(context != NULL);
    scope_t *scope = context->current_scope;
    context->current_scope = scope->parent;
    // TODO: free symbols
    free(scope);
}

void enter_function(codegen_context_t *context, const function_definition_t *function) {
    assert(context != NULL && function != NULL);
    context->current_function = function;

    LLVMTypeRef  *param_types = malloc(0);
    LLVMTypeRef return_type = llvm_type_for(&function->return_type);
    LLVMTypeRef function_type = LLVMFunctionType(return_type, param_types, 0, false);
    LLVMValueRef function_value = LLVMAddFunction(context->llvm_module, function->identifier->value, function_type);
    LLVMSetLinkage(function_value, LLVMExternalLinkage); // TODO: determine linkage for functions
    context->llvm_current_function = function_value;
    context->llvm_current_block = LLVMAppendBasicBlock(function_value, "entry");
    context->llvm_builder = LLVMCreateBuilder();
    LLVMPositionBuilderAtEnd(context->llvm_builder, context->llvm_current_block);
}

void leave_function(codegen_context_t *context) {
    assert(context != NULL);
    LLVMDisposeBuilder(context->llvm_builder);
    context->llvm_builder = NULL;
    context->llvm_current_block = NULL;
    context->current_function = NULL;
}

symbol_t *lookup_symbol(const codegen_context_t *context, const char *name) {
    // TODO: symbol lookup
    return NULL;
}

void visit_function_definition(codegen_context_t *context, const function_definition_t *function) {
    assert(context != NULL && function != NULL);
    enter_function(context, function);
    enter_scope(context);

    // TODO: declare parameters

    assert(function->body != NULL && function->body->type == STATEMENT_COMPOUND);
    ptr_vector_t *statements = &function->body->compound.statements;
    for (size_t i = 0; i < statements->size; i++) {
        const statement_t *statement = statements->buffer[i];
        visit_statement(context, statement);
    }

    // Implicit return statement
    // TODO: add validation for return type, and add implicit return statement if necessary for non-void functions
    LLVMValueRef last_ins = LLVMGetLastInstruction(context->llvm_current_block);
    LLVMOpcode last_op = LLVMGetInstructionOpcode(last_ins);
    if (last_op != LLVMRet) {
        LLVMBuildRetVoid(context->llvm_builder);
    }

    leave_scope(context);
    leave_function(context);
}

void visit_statement(codegen_context_t *context, const statement_t *statement) {
    assert(context != NULL && statement != NULL);

    switch (statement->type) {
        case STATEMENT_EMPTY:
            break;
        case STATEMENT_COMPOUND:
            enter_scope(context);
            for (size_t i = 0; i < statement->compound.statements.size; i++) {
                const statement_t *stmt = statement->compound.statements.buffer[i];
                visit_statement(context, stmt);
            }
            leave_scope(context);
            break;
        case STATEMENT_EXPRESSION:
            visit_expression(context, statement->expression);
            break;
        case STATEMENT_RETURN: {
            const expression_t *expr = statement->return_.expression;
            if (expr != NULL) {
                expression_result_t value = visit_expression(context, expr);
                // TODO: validate return type, convert if necessary
                LLVMBuildRet(context->llvm_builder, value.llvm_value);
            } else {
                LLVMBuildRetVoid(context->llvm_builder);
            }
            break;
        }
    }
}

expression_result_t visit_expression(codegen_context_t *context, const expression_t *expression) {
    assert(context != NULL && expression != NULL);
    switch (expression->type) {
        case EXPRESSION_PRIMARY:
            return visit_primary_expression(context, expression);
        case EXPRESSION_UNARY:
            visit_unary_expression(context, expression);
        case EXPRESSION_BINARY:
            return visit_binary_expression(context, expression);
        case EXPRESSION_TERNARY:
            return visit_ternary_expression(context, expression);
        case EXPRESSION_CALL:
            assert(false && "Function call codegen not yet implemented");
            break;
        case EXPRESSION_ARRAY_SUBSCRIPT:
            assert(false && "Array subscript codegen not yet implemented");
            break;
        case EXPRESSION_MEMBER_ACCESS:
            assert(false && "Member access codegen not yet implemented");
            break;
    }
}

expression_result_t visit_arithmetic_binary_expression(codegen_context_t *context, const expression_t *expression);
expression_result_t visit_bitwise_binary_expression(codegen_context_t *context, const expression_t *expression);
expression_result_t visit_comma_binary_expression(codegen_context_t *context, const expression_t *expression);
expression_result_t visit_logical_binary_expression(codegen_context_t *context, const expression_t *expression);
expression_result_t visit_comparison_binary_expression(codegen_context_t *context, const expression_t *expression);
expression_result_t visit_assignment_binary_expression(codegen_context_t *context, const expression_t *expression);

expression_result_t visit_binary_expression(codegen_context_t *context, const expression_t *expression) {
    assert(context != NULL && expression != NULL && expression->type == EXPRESSION_BINARY);
    assert(expression->binary.left != NULL && expression->binary.right != NULL);

    switch (expression->binary.type) {
        case BINARY_ARITHMETIC:
            return visit_arithmetic_binary_expression(context, expression);
        case BINARY_ASSIGNMENT:
            return visit_assignment_binary_expression(context, expression);
        case BINARY_COMMA:
            return visit_comma_binary_expression(context, expression);
        case BINARY_COMPARISON:
            return visit_comparison_binary_expression(context, expression);
        case BINARY_BITWISE:
            return visit_bitwise_binary_expression(context, expression);
        case BINARY_LOGICAL:
            return visit_logical_binary_expression(context, expression);
        default:
            assert(false && "Invalid binary expression type");
    }
}

expression_result_t visit_arithmetic_binary_expression(codegen_context_t *context, const expression_t *expression) {
    assert(context != NULL && expression != NULL && expression->type == EXPRESSION_BINARY && expression->binary.type == BINARY_ARITHMETIC);
    assert(expression->binary.left != NULL && expression->binary.right != NULL);

    expression_result_t left = visit_expression(context, expression->binary.left);
    expression_result_t right = visit_expression(context, expression->binary.right);

    // Validate the types of the left and right operands.
    if (expression->binary.arithmetic_operator == BINARY_ARITHMETIC_MODULO) {
        // The arguments to the modulo operator must be integers.
        // TODO: better error handling
        if (!is_integer_type(left.type) || !is_integer_type(right.type)) {
            source_position_t position = expression->binary.operator->position;
            fprintf(stderr, "%s:%d:%d: error: invalid operands to modulo operator\n",
                    position.path, position.line, position.column);
            exit(1);
        }
    } else {
        // Otherwise, they must be arithmetic types (integers, float).
        // TODO: better error handling
        // TODO: pointer arithmetic
        if (!is_arithmetic_type(left.type) || !is_arithmetic_type(right.type)) {
            source_position_t position = expression->binary.operator->position;
            fprintf(stderr, "%s:%d:%d: error: invalid operands to arithmetic operator\n",
                    position.path, position.line, position.column);
            exit(1);
        }
    }

    // Handle implicit type conversions
    const type_t *common_type = get_common_type(left.type, right.type);
    if (!types_equal(left.type, common_type)) {
        left = (expression_result_t) {
                .type = common_type,
                .llvm_value = convert_to_type(context, left.llvm_value, left.type, common_type),
                .llvm_type = llvm_type_for(common_type),
        };
    }
    if (!types_equal(right.type, common_type)) {
        right = (expression_result_t) {
                .type = common_type,
                .llvm_value = convert_to_type(context, right.llvm_value, right.type, common_type),
                .llvm_type = llvm_type_for(common_type),
        };
    }

    LLVMValueRef result;

    switch (expression->binary.arithmetic_operator) {
        case BINARY_ARITHMETIC_ADD:
            if (common_type->kind == TYPE_FLOATING) {
                result = LLVMBuildFAdd(context->llvm_builder, left.llvm_value, right.llvm_value, "addtmp");
            } else {
                result = LLVMBuildAdd(context->llvm_builder, left.llvm_value, right.llvm_value, "addtmp");
            }
            break;
        case BINARY_ARITHMETIC_SUBTRACT:
            if (common_type->kind == TYPE_FLOATING) {
                result = LLVMBuildFSub(context->llvm_builder, left.llvm_value, right.llvm_value, "subtmp");
            } else {
                result = LLVMBuildSub(context->llvm_builder, left.llvm_value, right.llvm_value, "subtmp");
            }
            break;
        case BINARY_ARITHMETIC_MULTIPLY:
            if (common_type->kind == TYPE_FLOATING) {
                result = LLVMBuildFMul(context->llvm_builder, left.llvm_value, right.llvm_value, "multmp");
            } else {
                result = LLVMBuildMul(context->llvm_builder, left.llvm_value, right.llvm_value, "multmp");
            }
            break;
        case BINARY_ARITHMETIC_DIVIDE:
            if (common_type->kind == TYPE_FLOATING) {
                result = LLVMBuildFDiv(context->llvm_builder, left.llvm_value, right.llvm_value, "divtmp");
            } else {
                if (left.type->integer.is_signed) {
                    result = LLVMBuildSDiv(context->llvm_builder, left.llvm_value, right.llvm_value, "divtmp");
                } else {
                    result = LLVMBuildUDiv(context->llvm_builder, left.llvm_value, right.llvm_value, "divtmp");
                }
            }
            break;
        case BINARY_ARITHMETIC_MODULO:
            if (left.type->integer.is_signed) {
                result = LLVMBuildSRem(context->llvm_builder, left.llvm_value, right.llvm_value, "modtmp");
            } else {
                result = LLVMBuildURem(context->llvm_builder, left.llvm_value, right.llvm_value, "modtmp");
            }
            break;
        default:
            assert(false && "Invalid arithmetic operator");
    }

    return (expression_result_t) {
            .type = left.type,
            .llvm_value = result,
            .llvm_type = left.llvm_type,
    };
}

expression_result_t visit_bitwise_binary_expression(codegen_context_t *context, const expression_t *expression) {
    assert(context != NULL && expression != NULL && expression->type == EXPRESSION_BINARY && expression->binary.type == BINARY_BITWISE);
    assert(expression->binary.left != NULL && expression->binary.right != NULL);

    expression_result_t left = visit_expression(context, expression->binary.left);
    expression_result_t right = visit_expression(context, expression->binary.right);

    // Validate operand types: for bitwise operators, the operands must be integers.
    if (left.type->kind != TYPE_INTEGER || right.type->kind != TYPE_INTEGER) {
        source_position_t position = expression->binary.operator->position;
        fprintf(stderr, "%s:%d:%d: error: invalid operands to bitwise operator\n",
                position.path, position.line, position.column);
        exit(1);
    }

    // Handle implicit type conversions
    const type_t *common_type = get_common_type(left.type, right.type);
    if (!types_equal(left.type, common_type)) {
        left = (expression_result_t) {
                .type = common_type,
                .llvm_value = convert_to_type(context, left.llvm_value, left.type, common_type),
                .llvm_type = llvm_type_for(common_type),
        };
    }
    if (!types_equal(right.type, common_type)) {
        right = (expression_result_t) {
                .type = common_type,
                .llvm_value = convert_to_type(context, right.llvm_value, right.type, common_type),
                .llvm_type = llvm_type_for(common_type),
        };
    }

    assert(left.type->kind == TYPE_INTEGER && right.type->kind == TYPE_INTEGER); // TODO: report error
    assert(left.type->integer.size == right.type->integer.size); // TODO: implicit type conversion
    assert(left.type->integer.is_signed == right.type->integer.is_signed); // TODO: implicit type conversion

    LLVMValueRef result;

    switch (expression->binary.bitwise_operator) {
        case BINARY_BITWISE_AND:
            result = LLVMBuildAnd(context->llvm_builder, left.llvm_value, right.llvm_value, "andtmp");
            break;
        case BINARY_BITWISE_OR:
            result = LLVMBuildOr(context->llvm_builder, left.llvm_value, right.llvm_value, "ortmp");
            break;
        case BINARY_BITWISE_XOR:
            result = LLVMBuildXor(context->llvm_builder, left.llvm_value, right.llvm_value, "xortmp");
            break;
        case BINARY_BITWISE_SHIFT_LEFT:
            result = LLVMBuildShl(context->llvm_builder, left.llvm_value, right.llvm_value, "shltmp");
            break;
        case BINARY_BITWISE_SHIFT_RIGHT:
            if (common_type->integer.is_signed) {
                result = LLVMBuildAShr(context->llvm_builder, left.llvm_value, right.llvm_value, "shrtmp");
            } else {
                result = LLVMBuildLShr(context->llvm_builder, left.llvm_value, right.llvm_value, "shrtmp");
            }
            break;
        default:
            assert(false && "Invalid bitwise operator");
    }

    return (expression_result_t) {
            .type = left.type,
            .llvm_value = result,
            .llvm_type = left.llvm_type,
    };
}

expression_result_t visit_comma_binary_expression(codegen_context_t *context, const expression_t *expression) {
    assert(context != NULL && expression != NULL && expression->type == EXPRESSION_BINARY && expression->binary.type == BINARY_COMMA);
    assert(expression->binary.left != NULL && expression->binary.right != NULL);

    visit_expression(context, expression->binary.left); // The left expression is evaluated for side effects
    return visit_expression(context, expression->binary.right); // The right expression is the result
}

expression_result_t visit_logical_binary_expression(codegen_context_t *context, const expression_t *expression) {
    assert(context != NULL && expression != NULL && expression->type == EXPRESSION_BINARY && expression->binary.type == BINARY_LOGICAL);
    assert(expression->binary.left != NULL && expression->binary.right != NULL);

    assert(false && "Logical operator codegen not yet implemented");
}

expression_result_t visit_comparison_binary_expression(codegen_context_t *context, const expression_t *expression) {
    assert(context != NULL && expression != NULL && expression->type == EXPRESSION_BINARY && expression->binary.type == BINARY_COMPARISON);
    assert(expression->binary.left != NULL && expression->binary.right != NULL);

    expression_result_t left = visit_expression(context, expression->binary.left);
    expression_result_t right = visit_expression(context, expression->binary.right);
    assert(left.type->kind == TYPE_INTEGER && right.type->kind == TYPE_INTEGER); // TODO: handle other types
    assert(left.type->integer.size == right.type->integer.size); // TODO: implicit type conversion

    LLVMValueRef result;

    switch (expression->binary.comparison_operator) {
        case BINARY_COMPARISON_EQUAL:
            result = LLVMBuildICmp(context->llvm_builder, LLVMIntEQ, left.llvm_value, right.llvm_value, "cmptmp");
            break;
        case BINARY_COMPARISON_NOT_EQUAL:
            result = LLVMBuildICmp(context->llvm_builder, LLVMIntNE, left.llvm_value, right.llvm_value, "cmptmp");
            break;
        case BINARY_COMPARISON_LESS_THAN:
            if (left.type->integer.is_signed) {
                result = LLVMBuildICmp(context->llvm_builder, LLVMIntSLT, left.llvm_value, right.llvm_value, "cmptmp");
            } else {
                result = LLVMBuildICmp(context->llvm_builder, LLVMIntULT, left.llvm_value, right.llvm_value, "cmptmp");
            }
            break;
        case BINARY_COMPARISON_LESS_THAN_OR_EQUAL:
            if (left.type->integer.is_signed) {
                result = LLVMBuildICmp(context->llvm_builder, LLVMIntSLE, left.llvm_value, right.llvm_value, "cmptmp");
            } else {
                result = LLVMBuildICmp(context->llvm_builder, LLVMIntULE, left.llvm_value, right.llvm_value, "cmptmp");
            }
            break;
        case BINARY_COMPARISON_GREATER_THAN:
            if (left.type->integer.is_signed) {
                result = LLVMBuildICmp(context->llvm_builder, LLVMIntSGT, left.llvm_value, right.llvm_value, "cmptmp");
            } else {
                result = LLVMBuildICmp(context->llvm_builder, LLVMIntUGT, left.llvm_value, right.llvm_value, "cmptmp");
            }
            break;
        case BINARY_COMPARISON_GREATER_THAN_OR_EQUAL:
            if (left.type->integer.is_signed) {
                result = LLVMBuildICmp(context->llvm_builder, LLVMIntSGE, left.llvm_value, right.llvm_value, "cmptmp");
            } else {
                result = LLVMBuildICmp(context->llvm_builder, LLVMIntUGE, left.llvm_value, right.llvm_value, "cmptmp");
            }
            break;
    }

    return (expression_result_t) {
            .type = left.type, // TODO: boolean type
            .llvm_value = result,
            .llvm_type = LLVMTypeOf(result),
    };
}

expression_result_t visit_assignment_binary_expression(codegen_context_t *context, const expression_t *expression) {
    assert(context != NULL && expression != NULL && expression->type == EXPRESSION_BINARY && expression->binary.type == BINARY_ASSIGNMENT);
    assert(expression->binary.left != NULL && expression->binary.right != NULL);
    assert(false && "Assignment operator codegen not yet implemented");
}

expression_result_t visit_unary_expression(codegen_context_t *context, const expression_t *expression) {
    assert(context != NULL && expression != NULL && expression->type == EXPRESSION_UNARY);
    assert(false && "Unary operator codegen not yet implemented");
}

expression_result_t visit_ternary_expression(codegen_context_t *context, const expression_t *expression) {
    assert(context != NULL && expression != NULL && expression->type == EXPRESSION_TERNARY);
    assert(expression->ternary.condition != NULL && expression->ternary.true_expression != NULL && expression->ternary.false_expression != NULL);

    LLVMBasicBlockRef true_block = LLVMAppendBasicBlock(context->llvm_current_function, "ternary-true");
    LLVMBasicBlockRef false_block = LLVMAppendBasicBlock(context->llvm_current_function, "ternary-false");
    LLVMBasicBlockRef merge_block = LLVMAppendBasicBlock(context->llvm_current_function, "ternary-merge");

    expression_result_t condition = visit_expression(context, expression->ternary.condition);
    LLVMValueRef boolean_condition = LLVMBuildICmp(context->llvm_builder, LLVMIntNE, condition.llvm_value, LLVMConstInt(condition.llvm_type, 0, false), "cmptmp");
    LLVMBuildCondBr(context->llvm_builder, boolean_condition, true_block, false_block);

    LLVMPositionBuilderAtEnd(context->llvm_builder, true_block);
    context->llvm_current_block = true_block;
    expression_result_t true_expression = visit_expression(context, expression->ternary.true_expression);
    LLVMBuildBr(context->llvm_builder, merge_block);
    LLVMBasicBlockRef true_block_end = context->llvm_current_block;

    LLVMPositionBuilderAtEnd(context->llvm_builder, false_block);
    context->llvm_current_block = false_block;
    expression_result_t false_expression = visit_expression(context, expression->ternary.false_expression);
    LLVMBuildBr(context->llvm_builder, merge_block);
    LLVMBasicBlockRef false_block_end = context->llvm_current_block;

    // TODO: validate types of the true and false expressions, and handle implicit type conversion
    const type_t *type = true_expression.type;
    LLVMTypeRef llvm_type = llvm_type_for(type);

    context->llvm_current_block = merge_block;
    LLVMPositionBuilderAtEnd(context->llvm_builder, merge_block);
    LLVMValueRef phi = LLVMBuildPhi(context->llvm_builder, llvm_type, "phi");
    LLVMAddIncoming(phi, &true_expression.llvm_value, &true_block_end, 1);
    LLVMAddIncoming(phi, &false_expression.llvm_value, &false_block_end, 1);

    return (expression_result_t) {
            .type = type,
            .llvm_value = phi,
            .llvm_type = llvm_type,
    };
}

expression_result_t visit_primary_expression(codegen_context_t *context, const expression_t *expr) {
    assert(context != NULL && expr != NULL && expr->type == EXPRESSION_PRIMARY);
    switch (expr->primary.type) {
        case PE_IDENTIFIER: {
            symbol_t *symbol = lookup_symbol(context, expr->primary.token.value);
            if (symbol == NULL) {
                source_position_t position = expr->primary.token.position;
                fprintf(stderr, "%s:%d:%d: error: undeclared identifier '%s'\n",
                        position.path, position.line, position.column, expr->primary.token.value);
                exit(1);
            }

            // TODO: return symbol reference
            assert(false && "Symbol references not yet implemented");
            break;
        }
        case PE_CONSTANT: {
            return visit_constant(context, expr);
        }
        case PE_STRING_LITERAL:
            assert(false && "String literals not yet implemented");
        case PE_EXPRESSION:
            return visit_expression(context, expr->primary.expression);
    }

}

expression_result_t visit_constant(codegen_context_t *context, const expression_t *expr) {
    assert(context != NULL && expr != NULL && expr->type == EXPRESSION_PRIMARY && expr->primary.type == PE_CONSTANT);

    switch (expr->primary.token.kind) {
        case TK_CHAR_LITERAL: {
            // In C, char literals are promoted to int.
            LLVMTypeRef llvm_type = llvm_type_for(&INT);
            // TODO: handle escape sequences?
            // TODO: handle wide character literals?
            // We expect the value of the char literal token to be of the form 'c'.
            assert(strlen(expr->primary.token.value) == 3 && "Invalid char literal");
            char value = expr->primary.token.value[1];
            return (expression_result_t) {
                    .type = &INT,
                    .llvm_value = LLVMConstInt(llvm_type, value, false),
                    .llvm_type = llvm_type,
            };
        }
        case TK_INTEGER_CONSTANT: {
            // For details on the sizes of integer constants, see section 6.4.4.1 of the C language specification:
            // https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf

            // TODO: parse integer sign/size suffix
            // TODO: handle hex, binary, octal integer constants
            unsigned long long value = strtoull(expr->primary.token.value, NULL, 0);
            // TODO: determine integer size based on the suffix and the size of the constant
            const type_t *type = &INT;
            LLVMTypeRef llvm_type = LLVMInt32Type();
            return (expression_result_t) {
                    .type = type,
                    .llvm_value = LLVMConstInt(llvm_type, value, false),
                    .llvm_type = llvm_type,
            };
        }
        case TK_FLOATING_CONSTANT: {
            // TODO: parse floating point suffix
            double value = strtod(expr->primary.token.value, NULL);
            // TODO: determine floating point size based on the suffix
            const type_t *type = &FLOAT;
            LLVMTypeRef llvm_type = LLVMFloatType();
            return (expression_result_t) {
                    .type = type,
                    .llvm_value = LLVMConstReal(llvm_type, value),
                    .llvm_type = llvm_type,
            };
        }
        default:
            assert(false && "Invalid token kind for constant expression");
    }
}

/**
 * Returns the LLVM type corresponding to the given C type.
 * @param type C type
 * @return LLVM type corresponding to the given C type
 */
LLVMTypeRef llvm_type_for(const type_t *type) {
    switch (type->kind) {
        case TYPE_VOID:
            return LLVMVoidType();
        case TYPE_INTEGER:
            // TODO: architecture dependent integer sizes
            switch (type->integer.size) {
                case INTEGER_TYPE_BOOL:
                    return LLVMInt1Type();
                case INTEGER_TYPE_CHAR:
                    return LLVMInt8Type();
                case INTEGER_TYPE_SHORT:
                    return LLVMInt16Type();
                case INTEGER_TYPE_INT:
                    return LLVMInt32Type();
                case INTEGER_TYPE_LONG:
                    return LLVMInt64Type();
                case INTEGER_TYPE_LONG_LONG:
                    return LLVMInt128Type();
            }
        case TYPE_FLOATING:
            switch (type->floating) {
                case FLOAT_TYPE_FLOAT:
                    return LLVMFloatType();
                case FLOAT_TYPE_DOUBLE:
                    return LLVMDoubleType();
                case FLOAT_TYPE_LONG_DOUBLE:
                    return LLVMFP128Type();
            }
    }
}

LLVMValueRef convert_to_type(codegen_context_t *context, LLVMValueRef value, const type_t *from, const type_t *to) {
    if (types_equal(from, to)) {
        return value;
    }

    if (is_floating_type(from) && is_floating_type(to)) {
        // Both types are floating point types, so we just need to extend or truncate the value.
        if (FLOAT_TYPE_RANKS[from->floating] < FLOAT_TYPE_RANKS[to->floating]) {
            return LLVMBuildFPTrunc(context->llvm_builder, value, llvm_type_for(to), "fptrunctmp");
        } else {
            return LLVMBuildFPExt(context->llvm_builder, value, llvm_type_for(to), "fpexttmp");
        }
    } else if (is_integer_type(from) && is_integer_type(to)) {
        // Both types are integer types, so we just need to extend (sign or zero, depending on the sign value of 'to')
        // or truncate the value.
        if (INTEGER_TYPE_RANKS[from->integer.size] < INTEGER_TYPE_RANKS[to->integer.size]) {
            return LLVMBuildTrunc(context->llvm_builder, value, llvm_type_for(to), "trunctmp");
        } else if (INTEGER_TYPE_RANKS[from->integer.size] > INTEGER_TYPE_RANKS[to->integer.size]) {
            if (to->integer.is_signed) {
                return LLVMBuildSExt(context->llvm_builder, value, llvm_type_for(to), "sexttmp");
            } else {
                return LLVMBuildZExt(context->llvm_builder, value, llvm_type_for(to), "zexttmp");
            }
        } else {
            // The sizes are the same, but the signedness is different.
            // The representation of equivalent sized unsigned and signed integers is the same, so we don't need to
            // do anything.
            return value;
        }
    } else if (is_floating_type(from) && is_integer_type(to)) {
        if (to->integer.is_signed) {
            return LLVMBuildFPToSI(context->llvm_builder, value, llvm_type_for(to), "fptositmp");
        } else {
            return LLVMBuildFPToUI(context->llvm_builder, value, llvm_type_for(to), "fptouitmp");
        }
    } else if (is_integer_type(from) && is_floating_type(to)) {
        if (from->integer.is_signed) {
            return LLVMBuildSIToFP(context->llvm_builder, value, llvm_type_for(to), "sitofptmp");
        } else {
            return LLVMBuildUIToFP(context->llvm_builder, value, llvm_type_for(to), "uitofptmp");
        }
    } else {
        assert(false && "Invalid type conversion");
    }
}
