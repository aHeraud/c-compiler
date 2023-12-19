#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "llvm-c/Core.h"
#include "llvm-c/Analysis.h"
#include "codegen.h"
#include "ast.h"

typedef struct CodegenContext {
    LLVMModuleRef module;
} codegen_context_t;

codegen_context_t *codegen_init(const char* source_file_name);
void codegen_function(codegen_context_t *context, const function_definition_t *function);
void codegen_statement(codegen_context_t *context, LLVMBuilderRef *builder, const statement_t *statement);
LLVMValueRef codegen_expression(codegen_context_t *context, LLVMBuilderRef *builder, const expression_t *expression);
LLVMValueRef codegen_expression_primary(codegen_context_t *context, LLVMBuilderRef *builder, const expression_t *expr);
LLVMValueRef codegen_expression_binary(codegen_context_t *context, LLVMBuilderRef *builder, const expression_t *expr);
LLVMValueRef codegen_expression_unary(codegen_context_t *context, LLVMBuilderRef *builder, const expression_t *expr);

LLVMTypeRef resolve_type(const type_t *type);

void codegen(const char* source_file_name, const char* output_filename, const function_definition_t *function) {
    codegen_context_t *context = codegen_init(source_file_name);
    codegen_function(context, function);
    char *message = NULL;
    LLVMVerifyModule(context->module, LLVMAbortProcessAction, &message);
    LLVMDisposeMessage(message);
    LLVMPrintModuleToFile(context->module, output_filename, &message);
    LLVMDisposeModule(context->module);
    free(context);
}

codegen_context_t *codegen_init(const char* source_file_name) {
    codegen_context_t *context = malloc(sizeof(codegen_context_t));
    context->module = LLVMModuleCreateWithName("program"); // TODO: get name from file
    LLVMSetSourceFileName(context->module, source_file_name, strlen(source_file_name));
    return context;
}

void codegen_function(codegen_context_t *context, const function_definition_t *function) {
    LLVMTypeRef  *param_types = malloc(0);
    LLVMTypeRef return_type = resolve_type(&function->return_type);
    LLVMTypeRef function_type = LLVMFunctionType(return_type, param_types, 0, false);
    LLVMValueRef function_value = LLVMAddFunction(context->module, function->identifier->value, function_type);
    LLVMSetLinkage(function_value, LLVMExternalLinkage);

    LLVMBuilderRef builder = LLVMCreateBuilder();
    LLVMPositionBuilderAtEnd(builder, LLVMAppendBasicBlock(function_value, "entry"));

    assert(function->body->type == STATEMENT_COMPOUND);
    for (size_t i = 0; i < function->body->compound.statements.size; i += 1) {
        statement_t *statement = function->body->compound.statements.buffer[i];
        codegen_statement(context, &builder, statement);
    }

    LLVMDisposeBuilder(builder);
    free(param_types);
}

void codegen_statement(codegen_context_t *context, LLVMBuilderRef *builder, const statement_t *statement) {
    switch (statement->type) {
        case STATEMENT_EXPRESSION:
            codegen_expression(context, builder, statement->expression);
            break;
        case STATEMENT_COMPOUND:
            for (size_t i = 0; i < statement->compound.statements.size; i++) {
                codegen_statement(context, builder, statement->compound.statements.buffer[i]);
            }
            break;
        case STATEMENT_RETURN: {
                if (statement->return_.expression != NULL) {
                    LLVMValueRef value = codegen_expression(context, builder, statement->return_.expression);
                    LLVMBuildRet(*builder, value);
                } else {
                    LLVMBuildRetVoid(*builder);
                }
                break;
            }
        case STATEMENT_EMPTY:
            break;
    }
}

LLVMValueRef codegen_expression(codegen_context_t *context, LLVMBuilderRef *builder, const expression_t *expression) {
    assert(expression != NULL);
    switch (expression->type) {
        case EXPRESSION_PRIMARY:
            return codegen_expression_primary(context, builder, expression);
        case EXPRESSION_BINARY:
            return codegen_expression_binary(context, builder, expression);
        case EXPRESSION_UNARY:
            return codegen_expression_unary(context, builder, expression);
        default:
            assert(false); // unimplemented
            break;
    }
}

LLVMValueRef codegen_expression_primary(codegen_context_t *context, LLVMBuilderRef *builder, const expression_t *expr) {
    switch (expr->primary.type) {
        case PE_IDENTIFIER: {
                assert(false); //TODO
                break;
            }
        case PE_CONSTANT: {
            // TODO: negative constants?
            unsigned long long value = strtoull(expr->primary.token.value, NULL, 10); // TODO: handle other bases (e.g. 0x
            return LLVMConstInt(LLVMInt32Type(), value, false);
        }
        case PE_STRING_LITERAL:
            return LLVMConstString(expr->primary.token.value, strlen(expr->primary.token.value), false);
        case PE_EXPRESSION:
            return codegen_expression(context, builder, expr->primary.expression);
    }
}

LLVMValueRef codegen_expression_binary(codegen_context_t *context, LLVMBuilderRef *builder, const expression_t *expr) {
    LLVMValueRef left = codegen_expression(context, builder, expr->binary.left);
    LLVMValueRef right = codegen_expression(context, builder, expr->binary.right);

    switch (expr->binary.binary_operator) {
        case BINARY_ADD:
            return LLVMBuildAdd(*builder, left, right, "addtmp"); // TODO: name
        case BINARY_SUBTRACT:
            return LLVMBuildSub(*builder, left, right, "subtmp"); // TODO: name
        case BINARY_MULTIPLY:
            return LLVMBuildMul(*builder, left, right, "multmp"); // TODO: name
        case BINARY_DIVIDE:
            return LLVMBuildSDiv(*builder, left, right, "divtmp"); // TODO: name
        case BINARY_MODULO:
            return LLVMBuildSRem(*builder, left, right, "modtmp"); // TODO: name
        case BINARY_BITWISE_AND:
            return LLVMBuildAnd(*builder, left, right, "andtmp"); // TODO: name
        case BINARY_BITWISE_OR:
            return LLVMBuildOr(*builder, left, right, "ortmp"); // TODO: name
        case BINARY_BITWISE_XOR:
            return LLVMBuildXor(*builder, left, right, "xortmp"); // TODO: name
        case BINARY_LOGICAL_AND:
            return LLVMBuildAnd(*builder, left, right, "andtmp"); // TODO: name
        case BINARY_LOGICAL_OR:
            return LLVMBuildOr(*builder, left, right, "ortmp"); // TODO: name
        case BINARY_SHIFT_LEFT:
            return LLVMBuildShl(*builder, left, right, "shltmp"); // TODO: name
        case BINARY_SHIFT_RIGHT:
            return LLVMBuildLShr(*builder, left, right, "shrtmp"); // TODO: name
        case BINARY_EQUAL:
            // TODO: handle floating point comparison
            return LLVMBuildICmp(*builder, LLVMIntEQ, left, right, "eqtmp"); // TODO: name
        case BINARY_NOT_EQUAL:
            // TODO: handle floating point comparison
            return LLVMBuildICmp(*builder, LLVMIntNE, left, right, "netmp"); // TODO: name
        case BINARY_LESS_THAN:
            // TODO: handle floating point comparison
            return LLVMBuildICmp(*builder, LLVMIntSLT, left, right, "lttmp"); // TODO: name
        case BINARY_LESS_THAN_OR_EQUAL:
            // TODO: handle floating point comparison
            return LLVMBuildICmp(*builder, LLVMIntSLE, left, right, "letmp"); // TODO: name
        case BINARY_GREATER_THAN:
            // TODO: handle floating point comparison
            return LLVMBuildICmp(*builder, LLVMIntSGT, left, right, "gttmp"); // TODO: name
        case BINARY_GREATER_THAN_OR_EQUAL:
            // TODO: handle floating point comparison
            return LLVMBuildICmp(*builder, LLVMIntSGE, left, right, "getmp"); // TODO: name
        case BINARY_COMMA:
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
            assert(false); // TODO: unimplemented
    }
}

LLVMValueRef codegen_expression_unary(codegen_context_t *context, LLVMBuilderRef *builder, const expression_t *expr) {
    LLVMValueRef value = codegen_expression(context, builder, expr->unary.operand);

    switch (expr->unary.operator) {
        case UNARY_ADDRESS_OF:
            assert(false); // TODO
        case UNARY_DEREFERENCE:
            assert(false); // TODO
        case UNARY_PLUS:
            // is this a real operator?
            // or a no-op?
            return value;
        case UNARY_MINUS:
            return LLVMBuildNeg(*builder, value, "negtmp"); // TODO: name
        case UNARY_BITWISE_NOT:
            assert(false); // TODO
        case UNARY_LOGICAL_NOT:
            assert(false); // TODO
        case UNARY_PRE_INCREMENT:
            assert(false); // TODO
        case UNARY_PRE_DECREMENT:
            assert(false); // TODO
        case UNARY_POST_INCREMENT:
            assert(false); // TODO
        case UNARY_POST_DECREMENT:
            assert(false); // TODO
    }
}

LLVMTypeRef resolve_type(const type_t *type) {
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
