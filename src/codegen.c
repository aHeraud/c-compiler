#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <string.h>

#include "ast.h"
#include "codegen.h"
#include "util/hashtable.h"

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

expression_result_t visit_binary_expression(codegen_context_t *context, const expression_t *expression) {
    assert(context != NULL && expression != NULL && expression->type == EXPRESSION_BINARY);
    assert(expression->binary.left != NULL && expression->binary.right != NULL);

    expression_result_t left = visit_expression(context, expression->binary.left);
    expression_result_t right = visit_expression(context, expression->binary.right);

    // TODO: handle integer promotions (see: https://stackoverflow.com/questions/46073295/implicit-type-promotion-rules)
    // TODO: handle usual arithmetic conversions
    // TODO: handle floating point conversions

    if (left.type != right.type) {
        assert(false && "Implicit type conversion not yet implemented");
    }

    if (left.type->kind != TYPE_INTEGER) {
        assert(false && "Only integer types are supported");
    }

    const type_t *type = left.type; // The type of the operands (after promotions/conversions), and the result type.
    LLVMTypeRef llvm_type = llvm_type_for(type);
    LLVMValueRef result;

    switch (expression->binary.binary_operator) {
        case BINARY_ADD:
            result = LLVMBuildAdd(context->llvm_builder, left.llvm_value, right.llvm_value, "add");
            break;
        case BINARY_SUBTRACT:
            result = LLVMBuildSub(context->llvm_builder, left.llvm_value, right.llvm_value, "sub");
            break;
        case BINARY_MULTIPLY:
            result = LLVMBuildMul(context->llvm_builder, left.llvm_value, right.llvm_value, "mul");
            break;
        case BINARY_DIVIDE:
            if (type->integer.is_signed) {
                result = LLVMBuildSDiv(context->llvm_builder, left.llvm_value, right.llvm_value, "div");
            } else {
                result = LLVMBuildUDiv(context->llvm_builder, left.llvm_value, right.llvm_value, "div");
            }
            break;
        case BINARY_MODULO:
            if (type->integer.is_signed) {
                result = LLVMBuildSRem(context->llvm_builder, left.llvm_value, right.llvm_value, "mod");
            } else {
                result = LLVMBuildURem(context->llvm_builder, left.llvm_value, right.llvm_value, "mod");
            }
            break;
        case BINARY_BITWISE_AND:
            result = LLVMBuildBinOp(context->llvm_builder, LLVMAnd, left.llvm_value, right.llvm_value, "bitand");
            break;
        case BINARY_BITWISE_OR:
            result = LLVMBuildBinOp(context->llvm_builder, LLVMOr, left.llvm_value, right.llvm_value, "bitor");
            break;
        case BINARY_BITWISE_XOR:
            result = LLVMBuildXor(context->llvm_builder, left.llvm_value, right.llvm_value, "xor");
            break;
        case BINARY_LOGICAL_AND:
            // TODO: short circuiting
            // TODO: is this actually the correct way to implement logical and?
            result = LLVMBuildAnd(context->llvm_builder, left.llvm_value, right.llvm_value, "and");
            break;
        case BINARY_LOGICAL_OR:
            // TODO: short circuiting
            // TODO: is this actually the correct way to implement logical and?
            result = LLVMBuildOr(context->llvm_builder, left.llvm_value, right.llvm_value, "or");
            break;
        case BINARY_SHIFT_LEFT:
            result = LLVMBuildShl(context->llvm_builder, left.llvm_value, right.llvm_value, "shl");
            break;
        case BINARY_SHIFT_RIGHT:
            if (type->integer.is_signed) {
                result = LLVMBuildAShr(context->llvm_builder, left.llvm_value, right.llvm_value, "shr");
            } else {
                result = LLVMBuildLShr(context->llvm_builder, left.llvm_value, right.llvm_value, "shr");
            }
            break;
        case BINARY_EQUAL:
            result = LLVMBuildICmp(context->llvm_builder, LLVMIntEQ, left.llvm_value, right.llvm_value, "eq");
            break;
        case BINARY_NOT_EQUAL:
            result = LLVMBuildICmp(context->llvm_builder, LLVMIntNE, left.llvm_value, right.llvm_value, "ne");
            break;
        case BINARY_LESS_THAN:
            if (type->integer.is_signed) {
                result = LLVMBuildICmp(context->llvm_builder, LLVMIntSLT, left.llvm_value, right.llvm_value, "lt");
            } else {
                result = LLVMBuildICmp(context->llvm_builder, LLVMIntULT, left.llvm_value, right.llvm_value, "lt");
            }
            break;
        case BINARY_LESS_THAN_OR_EQUAL:
            if (type->integer.is_signed) {
                result = LLVMBuildICmp(context->llvm_builder, LLVMIntSLE, left.llvm_value, right.llvm_value, "le");
            } else {
                result = LLVMBuildICmp(context->llvm_builder, LLVMIntULE, left.llvm_value, right.llvm_value, "le");
            }
            break;
        case BINARY_GREATER_THAN:
            if (type->integer.is_signed) {
                result = LLVMBuildICmp(context->llvm_builder, LLVMIntSGT, left.llvm_value, right.llvm_value, "gt");
            } else {
                result = LLVMBuildICmp(context->llvm_builder, LLVMIntUGT, left.llvm_value, right.llvm_value, "gt");
            }
            break;
        case BINARY_GREATER_THAN_OR_EQUAL:
            if (type->integer.is_signed) {
                result = LLVMBuildICmp(context->llvm_builder, LLVMIntSGE, left.llvm_value, right.llvm_value, "ge");
            } else {
                result = LLVMBuildICmp(context->llvm_builder, LLVMIntUGE, left.llvm_value, right.llvm_value, "ge");
            }
            break;
        case BINARY_COMMA:
            // Evaluates and discards the result of the left expression, then evaluates and returns the right.
            // There is a sequence point between the left and right expressions.
            return right;
        case BINARY_ASSIGN:
        case BINARY_ADD_ASSIGN:
        case BINARY_SUBTRACT_ASSIGN:
        case BINARY_MULTIPLY_ASSIGN:
        case BINARY_DIVIDE_ASSIGN:
        case BINARY_MODULO_ASSIGN:
        case BINARY_BITWISE_AND_ASSIGN:
        case BINARY_BITWISE_OR_ASSIGN:
        case BINARY_BITWISE_XOR_ASSIGN:
        case BINARY_SHIFT_LEFT_ASSIGN:
        case BINARY_SHIFT_RIGHT_ASSIGN:
            assert(false && "Assignment operator codegen not yet implemented");
    }

    return (expression_result_t) {
            .type = type,
            .llvm_value = result,
            .llvm_type = llvm_type,
    };
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
    LLVMValueRef boolean_condition = LLVMBuildICmp(context->llvm_builder, LLVMIntNE, condition.llvm_value, LLVMConstInt(condition.llvm_type, 0, false), "cmp");
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
        case TK_FLOATING_CONSTANT:
            assert(false && "Floating point constants not yet implemented");
        default:
            assert(false && "Invalid token kind for constant expression");
    }
}

LLVMTypeRef llvm_type_for(const type_t *type) {
    switch (type->kind) {
        case TYPE_VOID:
            return LLVMVoidType();
        case TYPE_INTEGER:
            // TODO: architecture dependent integer sizes
            switch (type->integer.size) {
                case INTEGER_TYPE_CHAR:
                    return LLVMInt8Type();
                case INTEGER_TYPE_SHORT:
                    return LLVMInt16Type();
                case INTEGER_TYPE_INT:
                    return LLVMInt32Type();
                case INTEGER_TYPE_LONG:
                case INTEGER_TYPE_LONG_LONG:
                    return LLVMInt64Type();
            }
    }
}
