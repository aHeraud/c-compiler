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
#include "util/strings.h"
#include "numeric-constants.h"

LLVMValueRef convert_to_type(codegen_context_t *context, LLVMValueRef value, const type_t *from, const type_t *to);
LLVMValueRef get_rvalue(codegen_context_t *context, expression_result_t expr);
LLVMTypeRef get_function_type(const type_t *function);
LLVMValueRef as_boolean(codegen_context_t *context, expression_result_t value);
type_t *type_of_function_definition(const function_definition_t *function_definition);
bool function_types_equivalent(const type_t *a, const type_t* b);

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

void enter_function(codegen_context_t *context, const function_definition_t *function, symbol_t *symtab_entry) {
    assert(context != NULL && function != NULL);
    context->current_function = function;

    context->llvm_current_function = symtab_entry->llvm_value;
    context->llvm_current_block = LLVMAppendBasicBlock(context->llvm_current_function, "entry");
    context->llvm_builder = LLVMCreateBuilder();
    context->llvm_entry_block = context->llvm_current_block;
    context->llvm_last_alloca = NULL;
    LLVMPositionBuilderAtEnd(context->llvm_builder, context->llvm_current_block);
}

void leave_function(codegen_context_t *context) {
    assert(context != NULL);
    LLVMDisposeBuilder(context->llvm_builder);
    context->llvm_builder = NULL;
    context->llvm_current_block = NULL;
    context->current_function = NULL;
    context->llvm_entry_block = NULL;
    context->llvm_last_alloca = NULL;
}

symbol_t *lookup_symbol(const codegen_context_t *context, const char *name) {
    const scope_t *scope = context->current_scope;
    while (scope != NULL) {
        symbol_t *symbol = NULL;
        if (hash_table_lookup(&scope->symbols, name, (void**) &symbol)) {
            return symbol;
        }
        scope = scope->parent;
    }
    return NULL;
}

void declare_symbol(codegen_context_t *context, symbol_t *symbol) {
    assert(context != NULL && symbol != NULL);
    bool inserted = hash_table_insert(&context->current_scope->symbols, symbol->identifier->value, (void*) symbol);
    assert(inserted);
}

void visit_translation_unit(codegen_context_t *context, const translation_unit_t *translation_unit) {
    assert(context != NULL && translation_unit != NULL);
    // A translation unit is composed of a sequence of external declarations.
    for (int i = 0; i < translation_unit->length; i += 1) {
        external_declaration_t *item = translation_unit->external_declarations[i];
        // Each external declaration is either a function definition or a declaration.
        switch (item->type) {
            case EXTERNAL_DECLARATION_DECLARATION:
                // A single declaration may declare multiple variables.
                for (int j = 0; j < item->declaration.length; j += 1) {
                    visit_declaration(context, item->declaration.declarations[j]);
                }
                break;
            case EXTERNAL_DECLARATION_FUNCTION_DEFINITION:
                visit_function_definition(context, item->function_definition);
                break;
        }
    }
}

void visit_function_definition(codegen_context_t *context, const function_definition_t *function) {
    assert(context != NULL && function != NULL);

    // TODO: Before calling enter_function, verify that the function was not previously declared with a different
    //       signature. If it was, emit an error.
    //       Alternatively, that logic can be moved to enter_function.

    type_t *function_type = type_of_function_definition(function);

    // Verify that the function was not previously defined with a different signature.
    symbol_t *entry = lookup_symbol(context, function->identifier->value);
    if (entry != NULL) {
        if (entry->kind != SYMBOL_FUNCTION || entry->type->kind != TYPE_FUNCTION) {
            source_position_t position = function->identifier->position;
            fprintf(stderr, "%s:%d:%d: error: redefinition of '%s' as a different kind of symbol\n",
                    position.path, position.line, position.column, function->identifier->value);
            exit(1);
        }

        // Compare function signatures, the types must match exactly (excluding the parameter names)
        if (!function_types_equivalent(entry->type, function_type)) {
            source_position_t position = function->identifier->position;
            fprintf(stderr, "%s:%d:%d: error: redefinition of '%s' with a different signature\n",
                    position.path, position.line, position.column, function->identifier->value);
            exit(1);
        }
    } else {
        LLVMValueRef llvm_function_value = NULL;

        // Parameters
        bool has_varargs = function->parameter_list->variadic;
        size_t param_count = function->parameter_list->length;
        LLVMTypeRef *param_types = malloc(sizeof(LLVMTypeRef) * param_count);
        for (int i = 0; i < param_count; i += 1) {
            param_types[i] = llvm_type_for(function->parameter_list->parameters[i]->type);
        }

        LLVMTypeRef return_type = llvm_type_for(function->return_type);
        LLVMTypeRef llvm_function_type = LLVMFunctionType(return_type, param_types, param_count, has_varargs);
        llvm_function_value = LLVMAddFunction(context->llvm_module, function->identifier->value, llvm_function_type);

        // For now everything has internal linkage other than main.
        // TODO: determine linkage for functions
        if (strcmp(function->identifier->value, "main") == 0) {
            LLVMSetLinkage(llvm_function_value, LLVMExternalLinkage);
        } else {
            LLVMSetLinkage(llvm_function_value, LLVMInternalLinkage);
        }

        // Add the function to the symbol table
        entry = malloc(sizeof(symbol_t));
        *entry = (symbol_t) {
            .kind = SYMBOL_FUNCTION,
            .type = function_type,
            .identifier = function->identifier,
            .llvm_type = llvm_function_type,
            .llvm_value = llvm_function_value,
        };
        declare_symbol(context, entry);
    }

    enter_function(context, function, entry);
    enter_scope(context);

    // Declare the function parameters as local variables.
    for (int i = 0; i < function->parameter_list->length; i += 1) {
        parameter_declaration_t *param = function->parameter_list->parameters[i];
        LLVMValueRef param_value = LLVMGetParam(context->llvm_current_function, i);
        // TODO: handle identifiers that shadow other identifiers in enclosing scopes
        LLVMValueRef param_alloca = LLVMBuildAlloca(context->llvm_builder, llvm_type_for(param->type), param->identifier->value);
        LLVMBuildStore(context->llvm_builder, param_value, param_alloca);
        symbol_t *symbol = malloc(sizeof(symbol_t));
        *symbol = (symbol_t) {
            .kind = SYMBOL_LOCAL_VARIABLE,
            .type = param->type,
            .identifier = param->identifier,
            .unique_name = param->identifier->value,
            .llvm_type = llvm_type_for(param->type),
            .llvm_value = param_alloca,
        };
        declare_symbol(context, symbol);
    }

    assert(function->body != NULL && function->body->type == STATEMENT_COMPOUND);
    ptr_vector_t *block_items = &function->body->compound.block_items;
    for (size_t i = 0; i < block_items->size; i++) {
        const block_item_t *item = block_items->buffer[i];
        if (item->type == BLOCK_ITEM_DECLARATION) {
            visit_declaration(context, item->declaration);
        } else {
            visit_statement(context, item->statement);
        }
    }

    // Implicit return statement
    // TODO: add validation for return type, and add implicit return statement if necessary for non-void functions
    LLVMValueRef last_ins = LLVMGetLastInstruction(context->llvm_current_block);
    LLVMOpcode last_op = LLVMGetInstructionOpcode(last_ins);
    // TODO: this only returns from the last basic block, not from all terminating basic blocks
    if (last_op != LLVMRet) {
        if (function->return_type->kind == TYPE_VOID) {
            LLVMBuildRetVoid(context->llvm_builder);
        } else {
            // TODO: return value for struct/union types
            // Will return 0 for integer types, 0.0 for floating point types, and null for pointer types.
            LLVMValueRef val = LLVMConstInt(llvm_type_for(&INT), 0, false);
            val = convert_to_type(context, val, &INT, function->return_type);
            LLVMBuildRet(context->llvm_builder, val);
        }
    }

    leave_scope(context);
    leave_function(context);
}

void visit_declaration(codegen_context_t *context, const declaration_t *declaration) {
    assert(context != NULL && declaration != NULL);

    symbol_t *symbol = NULL;
    if (hash_table_lookup(&context->current_scope->symbols, declaration->identifier->value, (void**)&symbol)) {
        source_position_t position = declaration->identifier->position;
        source_position_t prev_position = symbol->identifier->position;
        fprintf(stderr, "%s:%d:%d: error: redeclaration of '%s'\n",
                position.path, position.line, position.column, declaration->identifier->value);
        fprintf(stderr, "%s:%d:%d: note: previous declaration of '%s' was here\n",
                prev_position.path, prev_position.line, prev_position.column, declaration->identifier->value);
        exit(1);
    }

    symbol = malloc(sizeof(symbol_t));
    if (lookup_symbol(context, declaration->identifier->value) != NULL) {
        // This shadows a symbol in an enclosing scope.
        // TODO: generate a unique name
    }

    LLVMTypeRef llvm_type = llvm_type_for(declaration->type);
    LLVMValueRef var;
    LLVMValueRef value = NULL;

    if (declaration->type->kind == TYPE_FUNCTION) {
        LLVMTypeRef function_type = get_function_type(declaration->type);
        LLVMValueRef function_value = LLVMAddFunction(context->llvm_module, declaration->identifier->value, function_type);
        symbol = malloc(sizeof(symbol_t));
        *symbol = (symbol_t) {
            .kind = SYMBOL_FUNCTION,
            .type = declaration->type,
            .identifier = declaration->identifier,
            .unique_name = declaration->identifier->value,
            .llvm_type = function_type,
            .llvm_value = function_value,
        };
        declare_symbol(context, symbol);
        return;
    }

    if (declaration->initializer != NULL) {
        expression_result_t initializer = visit_expression(context, declaration->initializer);
        if (initializer.is_lvalue) {
            initializer.llvm_value = get_rvalue(context, initializer);
        }

        // TODO: Implement the rules from 6.7.9 Initialization/6.5.16.1 Simple assignment in the C standard:
        //       https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1256.pdf
        if (types_equal(declaration->type, initializer.type)) {
            // Valid initializer, no type conversion required.
            value = initializer.llvm_value;
        } else if (is_arithmetic_type(declaration->type) && is_arithmetic_type(initializer.type)) {
            // Arithmetic type conversion
            value = convert_to_type(context, initializer.llvm_value, initializer.type, declaration->type);
        } else if (is_pointer_type(declaration->type) && is_pointer_type(initializer.type)) {
            // Pointer type conversion
            // TODO: validate that the pointer types are compatible (warning)
            // Is this a no-op in LLVM? Do we need to modify the pointer type?
            value = initializer.llvm_value;
        } else {
            // TODO: struct, union, array, enum initializers
            // The type of the initializer does not match the type of the variable.
            source_position_t position = declaration->identifier->position;
            fprintf(stderr, "%s:%d:%d: error: invalid initializer type\n",
                    position.path, position.line, position.column);
            exit(1);
        }
    }

    if (context->current_function != NULL) {
        // This is a local (stack allocated) variable.
        // TODO: unique name when shadowing global variable or variable in enclosing scope

        // The alloca instructions need to be at the beginning of the first basic block of the function (the 'entry' block).
        // We need to temporarily move the builder to the entry block to insert the alloca instruction.
        if (context->llvm_last_alloca == NULL) {
            // Position the builder at the beginning of the entry block.
            LLVMValueRef first = LLVMGetFirstInstruction(context->llvm_entry_block);
            if (first != NULL) {
                LLVMPositionBuilderBefore(context->llvm_builder, first);
            } else {
                LLVMPositionBuilderAtEnd(context->llvm_builder, context->llvm_entry_block);
            }
        } else {
            // Position the builder after the last alloca instruction.
            LLVMPositionBuilder(context->llvm_builder, context->llvm_entry_block, context->llvm_last_alloca);
        }
        var = LLVMBuildAlloca(context->llvm_builder, llvm_type, declaration->identifier->value);
        // Reset the builder to the end of the current block.
        LLVMPositionBuilderAtEnd(context->llvm_builder, context->llvm_current_block);

        if (value != NULL) {
            LLVMBuildStore(context->llvm_builder, value, var);
        }
    } else {
        // This is a global variable.
        // TODO: support uninitialized static global variables (that are initialized in another translation unit)
        // TODO: initialize global variables with no initializer?
        // TODO: global initializers must be constant expressions
        var = LLVMAddGlobal(context->llvm_module, llvm_type, declaration->identifier->value);
        if (value != NULL) {
            LLVMSetInitializer(var, value);
        }
    }

    *symbol = (symbol_t) {
        .kind = SYMBOL_LOCAL_VARIABLE,
        .identifier = declaration->identifier,
        .type = declaration->type,
        .llvm_type = llvm_type,
        .llvm_value = var,
    };
    declare_symbol(context, symbol);
}

void visit_if_statement(codegen_context_t *context, const statement_t *statement);

void visit_statement(codegen_context_t *context, const statement_t *statement) {
    assert(context != NULL && statement != NULL);

    switch (statement->type) {
        case STATEMENT_EMPTY:
            break;
        case STATEMENT_COMPOUND:
            enter_scope(context);

            for (size_t i = 0; i < statement->compound.block_items.size; i++) {
                const block_item_t *item = statement->compound.block_items.buffer[i];
                if (item->type == BLOCK_ITEM_DECLARATION) {
                    assert(false && "Declaration codegen not yet implemented");
                } else {
                    visit_statement(context, item->statement);
                }
            }
            leave_scope(context);
            break;
        case STATEMENT_EXPRESSION:
            visit_expression(context, statement->expression);
            break;
        case STATEMENT_IF:
            visit_if_statement(context, statement);
            break;
        case STATEMENT_RETURN: {
            const expression_t *expr = statement->return_.expression;
            if (expr != NULL) {
                expression_result_t value = visit_expression(context, expr);
                if (value.is_lvalue) {
                    value.llvm_value = get_rvalue(context, value);
                }
                // TODO: validate return type, convert if necessary
                LLVMBuildRet(context->llvm_builder, value.llvm_value);
            } else {
                LLVMBuildRetVoid(context->llvm_builder);
            }
            break;
        }
    }
}

void visit_if_statement(codegen_context_t *context, const statement_t *statement) {
    assert(context != NULL && statement != NULL && statement->type == STATEMENT_IF);
    assert(statement->if_.condition != NULL && statement->if_.true_branch != NULL);

    expression_result_t condition = visit_expression(context, statement->if_.condition);
    if (condition.is_lvalue) {
        condition.llvm_value = get_rvalue(context, condition);
    }

    LLVMBasicBlockRef true_block = LLVMAppendBasicBlock(context->llvm_current_function, "btrue");
    LLVMBasicBlockRef false_block = LLVMAppendBasicBlock(context->llvm_current_function, "bfalse");
    LLVMBasicBlockRef merge_block = LLVMAppendBasicBlock(context->llvm_current_function, "merge");

    // Convert the condition to a boolean value, and jump to the true block if it is true, or the false block if it is false.
    LLVMValueRef boolean_condition = convert_to_type(context, condition.llvm_value, condition.type, &BOOL);
    LLVMBuildCondBr(context->llvm_builder, boolean_condition, true_block, false_block);

    LLVMPositionBuilderAtEnd(context->llvm_builder, true_block);
    context->llvm_current_block = true_block;
    visit_statement(context, statement->if_.true_branch);
    LLVMBuildBr(context->llvm_builder, merge_block);

    LLVMPositionBuilderAtEnd(context->llvm_builder, false_block);
    context->llvm_current_block = false_block;
    if (statement->if_.false_branch != NULL) {
        visit_statement(context, statement->if_.false_branch);
    }
    LLVMBuildBr(context->llvm_builder, merge_block);

    context->llvm_current_block = merge_block;
    LLVMPositionBuilderAtEnd(context->llvm_builder, merge_block);
}

expression_result_t visit_arithmetic_binary_expression(codegen_context_t *context, const expression_t *expression);
expression_result_t visit_bitwise_binary_expression(codegen_context_t *context, const expression_t *expression);
expression_result_t visit_comma_binary_expression(codegen_context_t *context, const expression_t *expression);
expression_result_t visit_logical_binary_expression(codegen_context_t *context, const expression_t *expression);
expression_result_t visit_comparison_binary_expression(codegen_context_t *context, const expression_t *expression);
expression_result_t visit_assignment_binary_expression(codegen_context_t *context, const expression_t *expression);
expression_result_t visit_call_expression(codegen_context_t *context, const expression_t *expression);

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
            return visit_call_expression(context, expression);
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

    if (left.is_lvalue) {
        left.llvm_value = get_rvalue(context, left);
        left.is_lvalue = false;
    }
    if (right.is_lvalue) {
        right.llvm_value = get_rvalue(context, right);
        right.is_lvalue = false;
    }

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
            .expression = expression,
            .type = common_type,
            .llvm_value = convert_to_type(context, left.llvm_value, left.type, common_type),
            .llvm_type = llvm_type_for(common_type),
            .is_lvalue = false,
        };
    }
    if (!types_equal(right.type, common_type)) {
        right = (expression_result_t) {
            .expression = expression,
            .type = common_type,
            .llvm_value = convert_to_type(context, right.llvm_value, right.type, common_type),
            .llvm_type = llvm_type_for(common_type),
            .is_lvalue = false,
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
        .expression = expression,
        .type = left.type,
        .llvm_value = result,
        .llvm_type = left.llvm_type,
        .is_lvalue = false,
    };
}

expression_result_t visit_bitwise_binary_expression(codegen_context_t *context, const expression_t *expression) {
    assert(context != NULL && expression != NULL && expression->type == EXPRESSION_BINARY && expression->binary.type == BINARY_BITWISE);
    assert(expression->binary.left != NULL && expression->binary.right != NULL);

    expression_result_t left = visit_expression(context, expression->binary.left);
    expression_result_t right = visit_expression(context, expression->binary.right);

    if (left.is_lvalue) {
        left.llvm_value = get_rvalue(context, left);
        left.is_lvalue = false;
    }
    if (right.is_lvalue) {
        right.llvm_value = get_rvalue(context, right);
        right.is_lvalue = false;
    }

    // Validate operand types: for bitwise operators, the operands must be integers.
    if (left.type->kind != TYPE_INTEGER || right.type->kind != TYPE_INTEGER) {
        source_position_t position = expression->binary.operator->position;
        fprintf(stderr, "%s:%d:%d: error: invalid operands to bitwise operator\n",
                position.path, position.line, position.column);
        exit(1);
    }

    // Handle implicit type conversions
    const type_t *common_type;
    if (expression->binary.bitwise_operator != BINARY_BITWISE_SHIFT_LEFT &&
        expression->binary.bitwise_operator != BINARY_BITWISE_SHIFT_RIGHT) {
        common_type = get_common_type(left.type, right.type);
    } else {
        // For shift operations, the type of the result is the type of the left operand.
        common_type = left.type;
    }
    if (!types_equal(left.type, common_type)) {
        left = (expression_result_t) {
            .expression = left.expression,
            .type = common_type,
            .llvm_value = convert_to_type(context, left.llvm_value, left.type, common_type),
            .llvm_type = llvm_type_for(common_type),
        };
    }
    if (!types_equal(right.type, common_type)) {
        right = (expression_result_t) {
            .expression = right.expression,
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
            if (left.type->integer.is_signed) {
                result = LLVMBuildAShr(context->llvm_builder, left.llvm_value, right.llvm_value, "shrtmp");
            } else {
                result = LLVMBuildLShr(context->llvm_builder, left.llvm_value, right.llvm_value, "shrtmp");
            }
            break;
        default:
            assert(false && "Invalid bitwise operator");
    }

    return (expression_result_t) {
        .expression = expression,
        .type = left.type,
        .llvm_value = result,
        .llvm_type = left.llvm_type,
        .is_lvalue = false,
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

    if (left.is_lvalue) {
        left.llvm_value = get_rvalue(context, left);
        left.is_lvalue = false;
    }
    if (right.is_lvalue) {
        right.llvm_value = get_rvalue(context, right);
        right.is_lvalue = false;
    }

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
        .expression = expression,
        .type = left.type, // TODO: boolean type
        .llvm_value = result,
        .llvm_type = LLVMTypeOf(result),
        .is_lvalue = false,
    };
}

expression_result_t visit_assignment_binary_expression(codegen_context_t *context, const expression_t *expression) {
    assert(context != NULL && expression != NULL && expression->type == EXPRESSION_BINARY && expression->binary.type == BINARY_ASSIGNMENT);
    assert(expression->binary.left != NULL && expression->binary.right != NULL);
    assert(expression->binary.assignment_operator == BINARY_ASSIGN && "Assignment operator codegen not yet implemented");

    expression_result_t left = visit_expression(context, expression->binary.left);
    if (!left.is_lvalue) {
        source_position_t position = expression->binary.operator->position;
        fprintf(stderr, "%s:%d:%d: error: lvalue required as left operand of assignment\n",
                position.path, position.line, position.column);
        exit(1);
    }
    expression_result_t right = visit_expression(context, expression->binary.right);

    // TODO: validate that the types are compatible, and handle implicit type conversion

    LLVMValueRef converted_value = convert_to_type(context, right.llvm_value, right.type, left.type);
    LLVMValueRef assignment = LLVMBuildStore(context->llvm_builder, converted_value, left.llvm_value);

    return (expression_result_t) {
        .expression = expression,
        .llvm_value = assignment,
        .type = left.type,
        .llvm_type = left.llvm_type,
        .is_lvalue = false,
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
    if (condition.is_lvalue) {
        condition.llvm_value = get_rvalue(context, condition);
        condition.is_lvalue = false;
    }

    // TODO: validate that the condition is a scalar type (integer, float, pointer), and convert if necessary
    LLVMValueRef boolean_condition = LLVMBuildICmp(context->llvm_builder, LLVMIntNE, condition.llvm_value, LLVMConstInt(condition.llvm_type, 0, false), "cmptmp");
    LLVMBuildCondBr(context->llvm_builder, boolean_condition, true_block, false_block);

    LLVMPositionBuilderAtEnd(context->llvm_builder, true_block);
    context->llvm_current_block = true_block;
    expression_result_t true_expression = visit_expression(context, expression->ternary.true_expression);
    if (true_expression.is_lvalue) {
        true_expression.llvm_value = get_rvalue(context, true_expression);
        true_expression.is_lvalue = false;
    }
    LLVMBuildBr(context->llvm_builder, merge_block);
    LLVMBasicBlockRef true_block_end = context->llvm_current_block;

    LLVMPositionBuilderAtEnd(context->llvm_builder, false_block);
    context->llvm_current_block = false_block;
    expression_result_t false_expression = visit_expression(context, expression->ternary.false_expression);
    if (false_expression.is_lvalue) {
        false_expression.llvm_value = get_rvalue(context, false_expression);
        false_expression.is_lvalue = false;
    }
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
        .expression = expression,
        .type = type,
        .llvm_value = phi,
        .llvm_type = llvm_type,
        .is_lvalue = false,
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

            return (expression_result_t) {
                .expression = expr,
                .type = symbol->type,
                .llvm_value = symbol->llvm_value,
                .llvm_type = symbol->llvm_type,
                .is_lvalue = symbol->kind == SYMBOL_LOCAL_VARIABLE || symbol->kind == SYMBOL_GLOBAL_VARIABLE,
            };
        }
        case PE_CONSTANT: {
            return visit_constant(context, expr);
        }
        case PE_STRING_LITERAL: {
            char* literal = replace_escape_sequences(expr->primary.token.value);
            LLVMValueRef ptr = LLVMBuildGlobalStringPtr(context->llvm_builder, literal, ".str");
            return (expression_result_t) {
                    .expression = expr,
                    .type = &CONST_CHAR_PTR,
                    .llvm_value = ptr,
                    .llvm_type = LLVMTypeOf(ptr),
                    .is_lvalue = false,
            };
        }
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
                .expression = expr,
                .type = &INT,
                .llvm_value = LLVMConstInt(llvm_type, value, false),
                .llvm_type = llvm_type,
                .is_lvalue = false,
            };
        }
        case TK_INTEGER_CONSTANT: {
            const char *raw_value = expr->primary.token.value;
            unsigned long long value = 0;
            const type_t *type;

            decode_integer_constant(&expr->primary.token, &value, &type);
            LLVMTypeRef llvm_type = llvm_type_for(type);
            return (expression_result_t) {
                    .type = type,
                    .llvm_value = LLVMConstInt(llvm_type, value, false),
                    .llvm_type = llvm_type,
                    .is_lvalue = false,
            };
        }
        case TK_FLOATING_CONSTANT: {
            const type_t *type;
            long double value;
            decode_float_constant(&expr->primary.token, &value, &type);
            LLVMTypeRef llvm_type = llvm_type_for(type);
            return (expression_result_t) {
                    .type = type,
                    .llvm_value = LLVMConstReal(llvm_type, (double)value), // loss of precision?
                    .llvm_type = llvm_type,
                    .is_lvalue = false,
            };
        }
        default:
            assert(false && "Invalid token kind for constant expression");
    }
}

expression_result_t visit_call_expression(codegen_context_t *context, const expression_t *expression) {
    assert(context != NULL && expression != NULL && expression->type == EXPRESSION_CALL);

    expression_result_t function = visit_expression(context, expression->call.callee);
    if (function.is_lvalue) {
        function.llvm_value = get_rvalue(context, function);
        function.is_lvalue = false;
    }

    // The callee must be one of:
    // - a function
    // - a function pointer
    if (function.type->kind != TYPE_FUNCTION && (function.type->kind != TYPE_POINTER && function.type->pointer.base->kind != TYPE_FUNCTION)) {
        source_position_t position = expression->call.callee->primary.token.position;
        fprintf(stderr, "%s:%d:%d: error: called object is not a function or function pointer\n",
                position.path, position.line, position.column);
        exit(1);
    }

    LLVMValueRef callee = function.llvm_value;
    if (function.type->kind == TYPE_POINTER) {
        // The function is a function pointer, so we need to load the function pointer.
        callee = LLVMBuildLoad2(context->llvm_builder, function.llvm_type, function.llvm_value, "loadtmp");
    }

    // Get function type
    LLVMTypeRef fn_type = get_function_type(function.type);

    // Validate the parameters of the function match the provided arguments.
    // TODO: improve error message
    // TODO: codegen for variadic functions
    // TODO: codegen for functions with no parameters
    const parameter_type_list_t *parameters = function.type->function.parameter_list;
    if (parameters->variadic) {
        if (parameters->length > expression->call.arguments.size) {
            source_position_t position = expression->call.callee->primary.token.position;
            fprintf(stderr, "%s:%d:%d: error: too few arguments in variadic function call\n",
                    position.path, position.line, position.column);
            exit(1);
        }
    } else {
        if (parameters->length != expression->call.arguments.size) {
            source_position_t position = expression->call.callee->primary.token.position;
            fprintf(stderr, "%s:%d:%d: error: incorrect number of arguments in function call\n",
                    position.path, position.line, position.column);
            exit(1);
        }
    }

    // Set up the arguments
    size_t num_args = expression->call.arguments.size;
    LLVMValueRef *args = malloc(sizeof(LLVMValueRef) * expression->call.arguments.size);
    for (int i = 0; i < num_args; i += 1) {
        const parameter_declaration_t *parameter = i >= parameters->length ?
                parameters->parameters[parameters->length - 1] : parameters->parameters[i];
        const expression_t *argument = expression->call.arguments.buffer[i];
        expression_result_t resolved_argument = visit_expression(context, argument);
        if (resolved_argument.is_lvalue) {
            resolved_argument.llvm_value = get_rvalue(context, resolved_argument);
            resolved_argument.is_lvalue = false;
        }

        // Validate the type of the argument matches the type of the parameter (after implicit type conversion).
        if (is_arithmetic_type(parameter->type) && is_arithmetic_type(resolved_argument.type)) {
            // The argument and parameter are both arithmetic types, so we perform integer promotions and implicit
            // conversions as necessary.
            const type_t *common_type = get_common_type(parameter->type, resolved_argument.type);
            if (!types_equal(resolved_argument.type, common_type)) {
                resolved_argument = (expression_result_t) {
                        .type = common_type,
                        .llvm_value = convert_to_type(context, resolved_argument.llvm_value, resolved_argument.type, common_type),
                        .llvm_type = llvm_type_for(common_type),
                        .is_lvalue = false,
                };
            }
        } else if (is_pointer_type(parameter->type) && is_pointer_type(resolved_argument.type)) {
            // The argument and parameter are both pointer types, so no real conversion is necessary.
            // TODO: validate that the pointer types are compatible (warning)?
        } else if (is_pointer_type(parameter->type) && is_integer_type(resolved_argument.type)) {
            // Integer -> pointer conversion
            // First, we need to convert the integer to an integer of the same size as the pointer.
            // Then, we need to convert the integer to a pointer.
            LLVMValueRef arg = convert_to_type(context, resolved_argument.llvm_value, resolved_argument.type, resolved_argument.type->integer.is_signed ? &LONG : &UNSIGNED_LONG);
            resolved_argument = (expression_result_t) {
                    .type = parameter->type,
                    .llvm_value = LLVMBuildIntToPtr(context->llvm_builder, arg, llvm_type_for(parameter->type), parameter->identifier->value),
                    .llvm_type = llvm_type_for(parameter->type),
                    .is_lvalue = false,
            };
        } else {
            // TODO: struct, union, array, enum arguments
            fprintf(stderr, "%s:%d: struct, union, array, and enum arguments not implemented\n",
                    __FILE__, __LINE__);
            exit(1);
        }

        args[i] = resolved_argument.llvm_value;
    }

    LLVMValueRef result = LLVMBuildCall2(context->llvm_builder, fn_type, callee, args, num_args, "");
    return (expression_result_t) {
        .expression = expression,
        .type = function.type->function.return_type,
        .llvm_value = result,
        .llvm_type = llvm_type_for(function.type->function.return_type),
        .is_lvalue = false,
    };
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
        case TYPE_POINTER:
            return LLVMPointerType(llvm_type_for(type->pointer.base), 0);
        case TYPE_FUNCTION:
            return get_function_type(type);
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
        if (INTEGER_TYPE_RANKS[from->integer.size] > INTEGER_TYPE_RANKS[to->integer.size]) {
            return LLVMBuildTrunc(context->llvm_builder, value, llvm_type_for(to), "trunctmp");
        } else if (INTEGER_TYPE_RANKS[from->integer.size] < INTEGER_TYPE_RANKS[to->integer.size]) {
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

LLVMValueRef get_rvalue(codegen_context_t *context, expression_result_t expr) {
    assert(context != NULL && expr.llvm_value != NULL);

    // TODO: what if the expression is a pointer?
    if (expr.is_lvalue) {
        return LLVMBuildLoad2(context->llvm_builder, expr.llvm_type, expr.llvm_value, "loadtmp");
    } else {
        return expr.llvm_value;
    }
}

LLVMTypeRef get_function_type(const type_t *function) {
    assert(function != NULL && function->kind == TYPE_FUNCTION);

    // Parameters
    bool has_varargs = function->function.parameter_list->variadic;
    size_t param_count = function->function.parameter_list->length;
    LLVMTypeRef *param_types = malloc(sizeof(LLVMTypeRef) * param_count);
    for (int i = 0; i < param_count; i += 1) {
        param_types[i] = llvm_type_for(function->function.parameter_list->parameters[i]->type);
    }

    LLVMTypeRef return_type = llvm_type_for(function->function.return_type);
    return LLVMFunctionType(return_type, param_types, param_count, has_varargs);
}

LLVMValueRef as_boolean(codegen_context_t *context, expression_result_t value) {
    assert(context != NULL);

    if (!is_scalar_type(value.type)) {
        const expression_t *expr = value.expression;
        fprintf(stderr, "%s:%d:%d: error: expected scalar type\n", expr->span.start.path, expr->span.start.line, expr->span.start.column);
        exit(1);
    }

    if (value.is_lvalue) {
        value.llvm_value = get_rvalue(context, value);
        value.is_lvalue = false;
    }

    LLVMValueRef condition;

    if (is_integer_type(value.type) || is_pointer_type(value.type)) {
        condition = LLVMBuildICmp(context->llvm_builder, LLVMIntNE, value.llvm_value, LLVMConstInt(value.llvm_type, 0, false), "");
    } else if (is_floating_type(value.type)) {
        condition = LLVMBuildFCmp(context->llvm_builder, LLVMRealONE, value.llvm_value, LLVMConstReal(value.llvm_type, 0.0), "");
    } else {
        assert(false && "Invalid type for boolean conversion");
    }

    return condition;
}

type_t *type_of_function_definition(const function_definition_t *function_definition) {
    if (function_definition == NULL) {
        return NULL;
    }

    type_t *type = malloc(sizeof(type_t));
    *type = (type_t) {
        .kind = TYPE_FUNCTION,
        .is_volatile = false,
        .is_const = false,
        .function = {
            .return_type = function_definition->return_type,
            .parameter_list = function_definition->parameter_list,
        },
    };
    return type;
}

bool function_types_equivalent(const type_t *a, const type_t* b) {
    if (a == NULL || b == NULL) {
        // A previous error occurred, so we don't need to report another error.
        return true;
    }

    assert(a->kind == TYPE_FUNCTION && b->kind == TYPE_FUNCTION);

    if (!types_equal(a->function.return_type, b->function.return_type)) {
        return false;
    }

    if (a->function.parameter_list->length != b->function.parameter_list->length) {
        return false;
    }

    if (a->function.parameter_list->variadic != b->function.parameter_list->variadic) {
        return false;
    }

    for (int i = 0; i < a->function.parameter_list->length; i += 1) {
        if (!types_equal(a->function.parameter_list->parameters[i]->type, b->function.parameter_list->parameters[i]->type)) {
            return false;
        }
    }

    return true;
}
