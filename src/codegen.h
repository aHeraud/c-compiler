#ifndef C_COMPILER_CODEGEN_H
#define C_COMPILER_CODEGEN_H

#include <llvm-c/Core.h>


typedef struct CodegenContext codegen_context_t;
typedef struct Scope scope_t;
typedef struct Symbol symbol_t;

typedef struct Scope {
    hash_table_t symbols;
    scope_t *parent;
} scope_t;

typedef struct Symbol {
    const char *name;
    const type_t *type;
} symbol_t;

typedef struct CodegenContext {
    scope_t global_scope;
    scope_t *current_scope;
    const function_definition_t *current_function;

    // LLVM Module CodegenContext
    LLVMModuleRef llvm_module;
    LLVMValueRef llvm_current_function;
    LLVMBuilderRef llvm_builder;
    LLVMBasicBlockRef llvm_current_block;
} codegen_context_t;

codegen_context_t *codegen_init(const char* module_name);
void codegen_finalize(codegen_context_t *context, const char* output_filename);

void enter_scope(codegen_context_t *context);
void leave_scope(codegen_context_t *context);
void enter_function(codegen_context_t *context, const function_definition_t *function);
void leave_function(codegen_context_t *context);
bool declare_symbol(codegen_context_t *context, symbol_t *symbol);
symbol_t *lookup_symbol(const codegen_context_t *context, const char *name);

typedef struct ExpressionResult {
    const type_t *type;
    LLVMValueRef llvm_value;
    LLVMTypeRef llvm_type;
} expression_result_t;

void visit_function_definition(codegen_context_t *context, const function_definition_t *function);
void visit_statement(codegen_context_t *context, const statement_t *statement);

expression_result_t visit_expression(codegen_context_t *context, const expression_t *expression);
expression_result_t visit_primary_expression(codegen_context_t *context, const expression_t *expr);
expression_result_t visit_unary_expression(codegen_context_t *context, const expression_t *expression);
expression_result_t visit_binary_expression(codegen_context_t *context, const expression_t *expression);
expression_result_t visit_ternary_expression(codegen_context_t *context, const expression_t *expression);
expression_result_t visit_constant(codegen_context_t *context, const expression_t *expr);

LLVMTypeRef llvm_type_for(const type_t *type);

#endif //C_COMPILER_CODEGEN_H