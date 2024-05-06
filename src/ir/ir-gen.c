/// A module for performing semantic analysis and IR generation from an input AST. Semantic analysis and IR generation
/// are combined into a single traversal of the AST.
///

#include <stdio.h>
#include <string.h>
#include "ast.h"
#include "ir/ir-gen.h"
#include "ir/ir.h"
#include "ir/ir-builder.h"
#include "numeric-constants.h"
#include "errors.h"
#include "util/strings.h"

typedef struct Scope scope_t;
typedef struct Symbol symbol_t;

typedef struct IrGenContext {
    ir_module_t *module;
    ir_function_definition_t *function;
    ir_function_builder_t *builder;
    compilation_error_vector_t errors;
    scope_t *current_scope;
    // Counter for generating unique local variable names
    int local_id_counter;
    // Counter for generating unique labels
    int label_counter;
} ir_gen_context_t;

typedef struct ExpressionResult {
    ir_value_t value;
    bool is_lvalue;
    const type_t *c_type;
} expression_result_t;

typedef struct Scope {
    hash_table_t symbols;
    scope_t *parent;
} scope_t;

enum SymbolKind {
    SYMBOL_LOCAL_VARIABLE,
    SYMBOL_GLOBAL_VARIABLE,
    SYMBOL_FUNCTION,
};

typedef struct Symbol {
    enum SymbolKind kind;
    // The token containing the name of the symbol as it appears in the source.
    const token_t *identifier;
    // The name of the symbol as it appears in the IR.
    const char *name;
    // The C type of this symbol.
    const type_t *c_type;
    // The IR type of this symbol.
    const ir_type_t *ir_type;
    // Pointer to the memory location where this symbol is stored (variables only).
    ir_var_t ir_ptr;
} symbol_t;

const expression_result_t EXPR_ERR = {
    .c_type = NULL,
    .is_lvalue = false,
    .value = {
        .kind = IR_VALUE_VAR,
        .var = {
            .name = NULL,
            .type = NULL,
        },
    },
};

symbol_t *lookup_symbol(const ir_gen_context_t *context, const char *name) {
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

symbol_t *lookup_symbol_in_current_scope(const ir_gen_context_t *context, const char *name) {
    symbol_t *symbol = NULL;
    if (hash_table_lookup(&context->current_scope->symbols, name, (void**) &symbol)) {
        return symbol;
    }
    return NULL;
}

void declare_symbol(ir_gen_context_t *context, symbol_t *symbol) {
    assert(context != NULL && symbol != NULL);
    bool inserted = hash_table_insert(&context->current_scope->symbols, symbol->identifier->value, (void*) symbol);
    assert(inserted);
}

char *temp_name(ir_gen_context_t *context);
ir_var_t temp_var(ir_gen_context_t *context, const ir_type_t *type);
char *gen_label(ir_gen_context_t *context);

const ir_type_t *get_ir_val_type(ir_value_t value);

/**
 * Get the C integer type that is the same width as a pointer.
 * @return
 */
const type_t *c_ptr_int_type();

/**
 * Convert an IR value from one type to another.
 * @param context The IR generation context
 * @param value   The value to convert
 * @param from_type The C type of the value
 * @param to_type  The C type to convert the value to
 * @param result  Variable to store the result in. If NULL, a temporary variable will be created.
 * @return The resulting ir value and its corresponding c type
 */
expression_result_t convert_to_type(ir_gen_context_t *context, ir_value_t value, const type_t *from_type, const type_t *to_type, ir_var_t *result);

/**
 * Get the IR type that corresponds to a specific C type.
 * @param c_type
 * @return corresponding IR type
 */
const ir_type_t* get_ir_type(const type_t *c_type);

/**
 * Get the IR type that is a pointer to the specified IR type
 * @param pointee The type that the pointer points to
 * @return The pointer type
 */
const ir_type_t *get_ir_ptr_type(const ir_type_t *pointee);

ir_value_t ir_value_for_var(ir_var_t var);

expression_result_t get_rvalue(ir_gen_context_t *context, expression_result_t res);

void enter_scope(ir_gen_context_t *context) {
    assert(context != NULL);
    scope_t *scope = malloc(sizeof(scope_t));
    *scope = (scope_t) {
            .symbols = hash_table_create(1000),
            .parent = context->current_scope,
    };
    context->current_scope = scope;
}

void leave_scope(ir_gen_context_t *context) {
    assert(context != NULL);
    scope_t *scope = context->current_scope;
    context->current_scope = scope->parent;
    // TODO: free symbols
    free(scope);
}

void visit_translation_unit(ir_gen_context_t *context, const translation_unit_t *translation_unit);
void visit_function(ir_gen_context_t *context, const function_definition_t *function);
void visit_statement(ir_gen_context_t *context, const statement_t *statement);
void visit_if_statement(ir_gen_context_t *context, const statement_t *statement);
void visit_return_statement(ir_gen_context_t *context, const statement_t *statement);
void visit_declaration(ir_gen_context_t *context, const declaration_t *declaration);
expression_result_t visit_expression(ir_gen_context_t *context, const expression_t *expression);
expression_result_t visit_primary_expression(ir_gen_context_t *context, const expression_t *expr);
expression_result_t visit_constant(ir_gen_context_t *context, const expression_t *expr);
expression_result_t visit_call_expression(ir_gen_context_t *context, const expression_t *expr);
expression_result_t visit_binary_expression(ir_gen_context_t *context, const expression_t *expr);
expression_result_t visit_additive_binexpr(ir_gen_context_t *context, const expression_t *expr, expression_result_t left, expression_result_t right, ir_var_t *result);
expression_result_t visit_multiplicative_binexpr(ir_gen_context_t *context, const expression_t *expr, expression_result_t left, expression_result_t right, ir_var_t *result);
expression_result_t visit_assignment_binexpr(ir_gen_context_t *context, const expression_t *expr, expression_result_t left, expression_result_t right);
expression_result_t visit_comparison_binexpr(ir_gen_context_t *context, const expression_t *expr, expression_result_t left, expression_result_t right);

ir_gen_result_t generate_ir(const translation_unit_t *translation_unit) {
    ir_gen_context_t context = {
        .module = malloc(sizeof(ir_module_t)),
        .function = NULL,
        .builder = NULL,
        .errors = (compilation_error_vector_t) { .size = 0, .capacity = 0, .buffer = NULL },
        .current_scope = NULL,
        .local_id_counter = 0,
    };

    *context.module = (ir_module_t) {
        .functions = (ir_function_ptr_vector_t) { .size = 0, .capacity = 0, .buffer = NULL },
    };

    visit_translation_unit(&context, translation_unit);

    ir_gen_result_t result = {
        .module = context.module,
        .errors = context.errors,
    };
    return result;
}

void visit_translation_unit(ir_gen_context_t *context, const translation_unit_t *translation_unit) {
    enter_scope(context);

    for (size_t i = 0; i < translation_unit->length; i++) {
        external_declaration_t *external_declaration = (external_declaration_t*) translation_unit->external_declarations[i];
        switch (external_declaration->type) {
            case EXTERNAL_DECLARATION_FUNCTION_DEFINITION: {
                visit_function(context, external_declaration->function_definition);
                break;
            }
            case EXTERNAL_DECLARATION_DECLARATION: {
                // TODO: handle global/static variables
                // visit_declaration(context, external_declaration->declaration);
                break;
            }
        }
    }

    leave_scope(context);
}

void visit_function(ir_gen_context_t *context, const function_definition_t *function) {
    context->function = malloc(sizeof(ir_function_definition_t));
    *context->function = (ir_function_definition_t) {
        .name = function->identifier->value,
    };
    context->builder = IrCreateFunctionBuilder();

    // TODO: Add the function to the global scope (if not already present)
    //       If the function is already present, check that the signatures match.

    const type_t function_c_type = {
        .kind = TYPE_FUNCTION,
        .function = {
            .return_type = function->return_type,
            .parameter_list = function->parameter_list,
        },
    };
    const ir_type_t *function_type = get_ir_type(&function_c_type);
    context->function->type = function_type;

    // Verify that the function was not previously defined with a different signature.
    symbol_t *entry = lookup_symbol(context, function->identifier->value);
    if (entry != NULL) {
        if (entry->kind != SYMBOL_FUNCTION) {
            // A symbol with the same name exists, but it is not a function.
            append_compilation_error(&context->errors, (compilation_error_t) {
                .kind = ERR_REDEFINITION_OF_SYMBOL,
                .location = function->identifier->position,
                .redefinition_of_symbol = {
                    .redefinition = function->identifier,
                    .previous_definition = entry->identifier,
                },
            });
        } else if (!ir_types_equal(entry->ir_type, function_type)) {
            // The function was previously declared with a different signature
            append_compilation_error(&context->errors, (compilation_error_t) {
                .kind = ERR_REDEFINITION_OF_SYMBOL,
                .location = function->identifier->position,
                .redefinition_of_symbol = {
                    .redefinition = function->identifier,
                    .previous_definition = entry->identifier,
                },
            });
        }

        // Error if function was defined more than once
        bool already_defined = false;
        for (size_t i = 0; i < context->module->functions.size; i += 1) {
            ir_function_definition_t *existing_function = context->module->functions.buffer[i];
            if (strcmp(existing_function->name, function->identifier->value) == 0) {
                already_defined = true;
                break;
            }
        }
        if (already_defined) {
            append_compilation_error(&context->errors, (compilation_error_t) {
                .kind = ERR_REDEFINITION_OF_SYMBOL,
                .location = function->identifier->position,
                .redefinition_of_symbol = {
                    .redefinition = function->identifier,
                    .previous_definition = entry->identifier,
                },
            });
        }
    } else {
        // Insert the function into the symbol table
        symbol_t *symbol = malloc(sizeof(symbol_t));
        *symbol = (symbol_t) {
            .kind = SYMBOL_FUNCTION,
            .identifier = function->identifier,
            .name = function->identifier->value,
            .c_type = &function_c_type,
            .ir_type = function_type,
        };
        declare_symbol(context, symbol);
    }

    enter_scope(context); // Enter the function scope

    // Declare the function parameters, and add them to the symbol table
    context->function->num_params = function->parameter_list->length;
    context->function->params = malloc(sizeof(ir_var_t) * context->function->num_params);
    context->function->is_variadic = function->parameter_list->variadic;
    for (size_t i = 0; i < function->parameter_list->length; i += 1) {
        parameter_declaration_t *param = function->parameter_list->parameters[i];
        const ir_type_t *ir_param_type = get_ir_type(param->type);
        ir_var_t ir_param = {
            .name = param->identifier->value,
            .type = ir_param_type,
        };
        context->function->params[i] = ir_param;

        // Allocate a stack slot for the parameter
        ir_var_t param_ptr = {
            .name = temp_name(context),
            .type = get_ir_ptr_type(ir_param_type),
        };
        IrBuildAlloca(context->builder, ir_param_type, param_ptr);

        // Store the parameter in the stack slot
        IrBuildStore(context->builder, ir_value_for_var(param_ptr), ir_value_for_var(ir_param));

        // Create a symbol for the parameter and add it to the symbol table
        symbol_t *symbol = malloc(sizeof(symbol_t));
        *symbol = (symbol_t) {
            .kind = SYMBOL_LOCAL_VARIABLE,
            .identifier = param->identifier,
            .name = param->identifier->value,
            .c_type = param->type,
            .ir_type = ir_param_type,
            .ir_ptr = param_ptr,
        };
        declare_symbol(context, symbol);
    }

    visit_statement(context, function->body);

    leave_scope(context);

    IrFinalizeFunctionBuilder(context->builder, context->function);
    ir_function_ptr_vector_t *functions = &context->module->functions;
    VEC_APPEND(functions, context->function);
}

void visit_statement(ir_gen_context_t *context, const statement_t *statement) {
    assert(context != NULL && "Context must not be NULL");
    assert(statement != NULL && "Statement must not be NULL");

    switch (statement->type) {
        case STATEMENT_COMPOUND: {
            enter_scope(context);
            for (size_t i = 0; i < statement->compound.block_items.size; i++) {
                block_item_t *block_item = (block_item_t*) statement->compound.block_items.buffer[i];
                switch (block_item->type) {
                    case BLOCK_ITEM_STATEMENT: {
                        visit_statement(context, block_item->statement);
                        break;
                    }
                    case BLOCK_ITEM_DECLARATION: {
                        visit_declaration(context, block_item->declaration);
                    }
                }
            }
            leave_scope(context);
        }
        case STATEMENT_EMPTY: {
            // no-op
            break;
        }
        case STATEMENT_EXPRESSION: {
            visit_expression(context, statement->expression);
            break;
        }
        case STATEMENT_IF: {
            visit_if_statement(context, statement);
            break;
        }
        case STATEMENT_RETURN: {
            visit_return_statement(context, statement);
            break;
        }
        default:
            fprintf(stderr, "%s:%d: Invalid statement type\n", __FILE__, __LINE__);
            exit(1);
    }
}

void visit_if_statement(ir_gen_context_t *context, const statement_t *statement) {
    assert(context != NULL && "Context must not be NULL");
    assert(statement != NULL && "Statement must not be NULL");
    assert(statement->type == STATEMENT_IF);

    // Evaluate the condition
    expression_result_t condition = visit_expression(context, statement->if_.condition);

    if (condition.is_lvalue) {
        condition = get_rvalue(context, condition);
    }

    // The condition must have a scalar type
    if (!is_scalar_type(condition.c_type)) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_IF_CONDITION_TYPE,
            .location = statement->if_.keyword->position,
        });
        return;
    }

    // Create labels for the false branch and the end of the if statement
    char *false_label = NULL;
    if (statement->if_.false_branch != NULL) {
        false_label = gen_label(context);
    }
    char *end_label = gen_label(context);

    // Compare the condition to zero
    if (is_pointer_type(condition.c_type)) {
        // Convert to an integer type
        condition = convert_to_type(context, condition.value, condition.c_type, c_ptr_int_type(), NULL);
    }

    bool condition_is_floating = is_floating_type(condition.c_type);
    ir_value_t zero = condition_is_floating ?
          (ir_value_t) {
              .kind = IR_VALUE_CONST,
              .constant = (ir_const_t) {
                  .kind = IR_CONST_FLOAT,
                  .type = get_ir_type(condition.c_type),
                  .f = 0.0,
              },
          } :
          (ir_value_t) {
              .kind = IR_VALUE_CONST,
              .constant = (ir_const_t) {
                  .kind = IR_CONST_INT,
                  .type = get_ir_type(condition.c_type),
                  .i = 0,
              },
          };
    ir_var_t condition_var = (ir_var_t) {
        .name = temp_name(context),
        .type = &IR_BOOL,
    };
    IrBuildEq(context->builder, condition.value, zero, condition_var);
    IrBuildBrCond(context->builder, ir_value_for_var(condition_var), false_label != NULL ? false_label : end_label);

    // Generate code for the true branch
    visit_statement(context, statement->if_.true_branch);

    if (statement->if_.false_branch != NULL) {
        // Jump to the end of the if statement
        IrBuildBr(context->builder, end_label);

        // Label for the false branch
        IrBuildNop(context->builder, false_label);

        // Generate code for the false branch
        visit_statement(context, statement->if_.false_branch);
    }

    IrBuildNop(context->builder, end_label);
}

void visit_return_statement(ir_gen_context_t *context, const statement_t *statement) {
    assert(context != NULL && "Context must not be NULL");
    assert(statement != NULL && "Statement must not be NULL");
    assert(statement->type == STATEMENT_RETURN);

    // TODO: Verify that the return type matches the function signature
    if (statement->return_.expression != NULL) {
        expression_result_t value = visit_expression(context, statement->return_.expression);
        // TODO: apply implicit conversion to the return type
        IrBuildReturnValue(context->builder, value.value);
    } else {
        IrBuildReturnVoid(context->builder);
    }
}

void visit_declaration(ir_gen_context_t *context, const declaration_t *declaration) {
    assert(context != NULL && "Context must not be NULL");
    assert(declaration != NULL && "Declaration must not be NULL");

    // Verify that this declaration is not a redeclaration of an existing symbol
    symbol_t *symbol = lookup_symbol_in_current_scope(context, declaration->identifier->value);
    if (symbol != NULL) {
        // Symbols in the same scope must have unique names, redefinition is not allowed.
        append_compilation_error(&context->errors, (compilation_error_t) {
            .location = declaration->identifier->position,
            .kind = ERR_REDEFINITION_OF_SYMBOL,
            .redefinition_of_symbol = {
                .redefinition = declaration->identifier,
                .previous_definition = symbol->identifier,
            },
        });
        return;
    }

    const ir_type_t *ir_type = get_ir_type(declaration->type);

    // Create a new symbol for this declaration and add it to the current scope
    symbol = malloc(sizeof(symbol_t));
    *symbol = (symbol_t) {
        .kind = SYMBOL_LOCAL_VARIABLE, // TODO: handle global/static variables
        .identifier = declaration->identifier,
        .name = declaration->identifier->value,
        .c_type = declaration->type,
        .ir_type = ir_type,
        .ir_ptr = (ir_var_t) {
            .name = temp_name(context),
            .type = get_ir_ptr_type(ir_type),
        }
    };
    declare_symbol(context, symbol);

    // Allocate storage space for the variable
    IrBuildAlloca(context->builder, get_ir_type(declaration->type), symbol->ir_ptr);

    // Evaluate the initializer if present, and store the result in the allocated storage
    if (declaration->initializer != NULL) {
        expression_result_t result = visit_expression(context, declaration->initializer);
        if (result.c_type == NULL) {
            // Error occurred while evaluating the initializer
            return;
        }

        // Verify that the types are compatible, convert if necessary
        result = convert_to_type(context, result.value, result.c_type, declaration->type, NULL);
        if (result.c_type == NULL) {
            // Incompatible types
            return;
        }

        // If the initializer is an lvalue, load the value
        // TODO: not sure that this is correct
        if (result.is_lvalue) {
            result = get_rvalue(context, result);
        }

        // Store the result in the allocated storage
        IrBuildStore(context->builder, ir_value_for_var(symbol->ir_ptr), result.value);
    }
}

expression_result_t visit_expression(ir_gen_context_t *context, const expression_t *expression) {
    assert(context != NULL && "Context must not be NULL");
    assert(expression != NULL && "Expression must not be NULL");

    switch (expression->type) {
        case EXPRESSION_ARRAY_SUBSCRIPT: {
            assert(false && "Array subscript not implemented");
        }
        case EXPRESSION_BINARY:
            return visit_binary_expression(context, expression);
        case EXPRESSION_CALL: {
            return visit_call_expression(context, expression);
        }
        case EXPRESSION_CAST: {
            assert(false && "Cast not implemented");
        }
        case EXPRESSION_MEMBER_ACCESS: {
            assert(false && "Member access not implemented");
        }
        case EXPRESSION_PRIMARY:
            return visit_primary_expression(context, expression);
        case EXPRESSION_SIZEOF: {
            assert(false && "sizeof operator not implemented");
        }
        case EXPRESSION_TERNARY: {
            assert(false && "Ternary operator not implemented");
        }
        case EXPRESSION_UNARY: {
            assert(false && "Unary operators not implemented");
        }
    }

    // TODO
    return EXPR_ERR;
}

expression_result_t visit_call_expression(ir_gen_context_t *context, const expression_t *expr) {
    // TODO: implement call expression
    IrBuildNop(context->builder, NULL);
    return EXPR_ERR;
}

expression_result_t visit_binary_expression(ir_gen_context_t *context, const expression_t *expr) {
    assert(context != NULL && "Context must not be NULL");
    assert(expr != NULL && "Expression must not be NULL");
    assert(expr->type == EXPRESSION_BINARY);

    // Evaluate the left and right operands.
    expression_result_t left = visit_expression(context, expr->binary.left);
    expression_result_t right = visit_expression(context, expr->binary.right);

    // Bubble up errors if the operands are invalid.
    if (left.c_type == NULL || right.c_type == NULL) {
        return EXPR_ERR;
    }

    switch (expr->binary.type) {
        case BINARY_ARITHMETIC: {
            if (expr->binary.arithmetic_operator == BINARY_ARITHMETIC_ADD ||
                expr->binary.arithmetic_operator == BINARY_ARITHMETIC_SUBTRACT) {
                return visit_additive_binexpr(context, expr, left, right, NULL);
            } else {
                return visit_multiplicative_binexpr(context, expr, left, right, NULL);
            }
        }
        case BINARY_ASSIGNMENT: {
            return visit_assignment_binexpr(context, expr, left, right);
        }
        case BINARY_BITWISE: {
            // TODO
            assert(false && "bitwise binary operators not implemented");
        }
        case BINARY_COMMA: {
            // TODO
            assert(false && "comma operator not implemented");
        }
        case BINARY_COMPARISON: {
            return visit_comparison_binexpr(context, expr, left, right);
        }
        case BINARY_LOGICAL: {
            // TODO
            assert(false && "logical operators not implemented");
        }
    }
}

expression_result_t visit_additive_binexpr(ir_gen_context_t *context, const expression_t *expr, expression_result_t left, expression_result_t right, ir_var_t *result) {
    const type_t *result_type;
    const ir_type_t *ir_result_type;

    bool is_addition = expr->binary.operator->kind == TK_PLUS
                     || expr->binary.operator->kind == TK_PLUS_ASSIGN;

    // Both operands must have arithmetic type, or one operand must be a pointer and the other an integer.
    if (is_arithmetic_type(left.c_type) && is_arithmetic_type(right.c_type)) {
        // Integer/Float + Integer/Float
        result_type = get_common_type(left.c_type, right.c_type);
        ir_result_type = get_ir_type(result_type);
        if (result == NULL) {
            // Generate a temp variable to store the result.
            result = (ir_var_t *) alloca(sizeof(ir_var_t));
            *result = temp_var(context, ir_result_type);
        }

        left = convert_to_type(context, left.value, left.c_type, result_type, NULL);
        right = convert_to_type(context, right.value, right.c_type, result_type, NULL);

        if (ir_types_equal(result->type, ir_result_type)) {
            // Can assign directly to the result variable
            is_addition ? IrBuildAdd(context->builder, left.value, right.value, *result)
                        : IrBuildSub(context->builder, left.value, right.value, *result);
            return (expression_result_t) {
                .c_type = result_type,
                .is_lvalue = false,
                .value = ir_value_for_var(*result),
            };
        } else {
            // Need to store the result in a temp variable and then convert it to the result type.
            ir_var_t temp = temp_var(context, ir_result_type);
            is_addition ? IrBuildAdd(context->builder, left.value, right.value, temp)
                        : IrBuildSub(context->builder, left.value, right.value, temp);
            return convert_to_type(context, ir_value_for_var(temp), NULL, result_type, result);
        }
    } else if ((is_pointer_type(left.c_type) && is_integer_type(right.c_type)) ||
               (is_integer_type(left.c_type) && is_pointer_type(right.c_type))) {
        // Pointer +/ integer.
        expression_result_t pointer_operand = is_pointer_type(left.c_type) ? left : right;
        expression_result_t integer_operand = is_pointer_type(left.c_type) ? right : left;

        if (!is_addition && is_pointer_type(right.c_type)) {
            // For subtraction the lhs must be the pointer
            append_compilation_error(&context->errors, (compilation_error_t) {
                .kind = ERR_INVALID_BINARY_EXPRESSION_OPERANDS,
                .location = expr->binary.operator->position,
                .invalid_binary_expression_operands = {
                    .operator = expr->binary.operator->value,
                    .left_type = left.c_type,
                    .right_type = right.c_type,
                },
            });
            return EXPR_ERR;
        }

        // The result type is the same as the pointer type.
        result_type = pointer_operand.c_type;

        if (result == NULL) {
            // Generate a temp variable to store the result.
            result = (ir_var_t *) alloca(sizeof(ir_var_t));
            *result = temp_var(context, ir_result_type);
        }

        // Extend/truncate the integer to the size of a pointer.
        integer_operand = convert_to_type(context, integer_operand.value, integer_operand.c_type, c_ptr_int_type(), NULL);

        // Multiply the integer by the size of the pointee type.
        ir_value_t size_constant = (ir_value_t) {
            .kind = IR_VALUE_CONST,
            .constant = (ir_const_t) {
                .kind = IR_CONST_INT,
                .type = get_ir_type(c_ptr_int_type()),
                .i = size_of_type(get_ir_type(pointer_operand.c_type->pointer.base))
            }
        };
        ir_var_t temp = temp_var(context, get_ir_type(c_ptr_int_type()));
        IrBuildMul(context->builder, integer_operand.value, size_constant, temp);
        ir_value_t temp_val = (ir_value_t) { .kind = IR_VALUE_VAR, .var = temp };
        // Add/sub the operands
        is_addition ? IrBuildAdd(context->builder, pointer_operand.value, temp_val, *result)
                    : IrBuildSub(context->builder, pointer_operand.value, temp_val, *result);

        return (expression_result_t) {
            .c_type = result_type,
            .is_lvalue = false,
            .value = (ir_value_t) {
                .kind = IR_VALUE_VAR,
                .var = *result,
            },
        };
    } else {
        // Invalid operand types.
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_BINARY_EXPRESSION_OPERANDS,
            .location = expr->binary.operator->position,
            .invalid_binary_expression_operands = {
                .operator = expr->binary.operator->value,
                .left_type = left.c_type,
                .right_type = right.c_type,
            },
        });
        return EXPR_ERR;
    }
}

expression_result_t visit_multiplicative_binexpr(ir_gen_context_t *context, const expression_t *expr, expression_result_t left, expression_result_t right, ir_var_t *result) {
    assert(false && "Multiplicative binary expressions not implemented");
    return EXPR_ERR;
}

expression_result_t visit_assignment_binexpr(ir_gen_context_t *context, const expression_t *expr, expression_result_t left, expression_result_t right) {
    assert(context != NULL && "Context must not be NULL");
    assert(expr != NULL && "Expression must not be NULL");

    // The left operand must be a lvalue.
    if (!left.is_lvalue || left.c_type->is_const) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_ASSIGNMENT_TARGET,
            .location = expr->binary.operator->position,
        });
        return EXPR_ERR;
    }

    if (expr->binary.operator->kind != TK_ASSIGN) {
        // TODO
        assert(false && "Compound assignment not implemented");
    }

    // Generate an assignment instruction.
    ir_var_t result = (ir_var_t) {
        .name = temp_name(context),
        .type = get_ir_type(left.c_type),
    };

    if (!types_equal(left.c_type, right.c_type)) {
        // Convert the right operand to the type of the left operand.
        right = convert_to_type(context, right.value, right.c_type, left.c_type, &result);
        if (right.c_type == NULL) {
            return EXPR_ERR;
        }
    }

    IrBuildAssign(context->builder, right.value, result);
    IrBuildStore(context->builder, left.value, ir_value_for_var(result));

    // assignments can be chained, e.g. `a = b = c;`
    return left;
}

expression_result_t visit_comparison_binexpr(ir_gen_context_t *context, const expression_t *expr, expression_result_t left, expression_result_t right) {
    assert(context != NULL && "Context must not be NULL");
    assert(expr != NULL && "Expression must not be NULL");
    assert(expr->type == EXPRESSION_BINARY && expr->binary.type == BINARY_COMPARISON);

    if (left.is_lvalue) left = get_rvalue(context, left);
    if (right.is_lvalue) right = get_rvalue(context, right);

    // One of the following must be true:
    // 1. both operands have arithmetic type
    // 2. both operands are pointers to compatible types
    // 3. both operands are pointers, and one is a pointer to void
    // 4. one operand is a pointer and the other is a null pointer constant

    // We will lazily relax this to allow comparisons between two arithmetic types, or two pointer types.
    // TODO: Implement the correct type restrictions for pointer comparisons.

    if (is_arithmetic_type(left.c_type) && is_arithmetic_type(right.c_type)) {
        const type_t *common_type = get_common_type(left.c_type, right.c_type);
        left = convert_to_type(context, left.value, left.c_type, common_type, NULL);
        right = convert_to_type(context, right.value, right.c_type, common_type, NULL);
        if (left.c_type == NULL || right.c_type == NULL) {
            return EXPR_ERR;
        }

        ir_var_t result = temp_var(context, &IR_BOOL);

        binary_comparison_operator_t op = expr->binary.comparison_operator;
        switch (op) {
            case BINARY_COMPARISON_EQUAL:
                IrBuildEq(context->builder, left.value, right.value, result);
                break;
            default:
                // TODO: implement the other comparison operators
                assert(false && "Comparison operator not implemented");
        }

        return (expression_result_t) {
            .c_type = &BOOL,
            .is_lvalue = false,
            .value = ir_value_for_var(result),
        };
    } else if (is_pointer_type(left.c_type) && is_pointer_type(right.c_type)) {
        // TODO: Implement pointer comparisons
        assert(false && "Pointer comparisons not implemented");
    } else {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_BINARY_EXPRESSION_OPERANDS,
            .location = expr->binary.operator->position,
            .invalid_binary_expression_operands = {
                .operator = expr->binary.operator->value,
                .left_type = left.c_type,
                .right_type = right.c_type,
            },
        });
        return EXPR_ERR;
    }
}

expression_result_t visit_primary_expression(ir_gen_context_t *context, const expression_t *expr) {
    assert(context != NULL && "Context must not be NULL");
    assert(expr != NULL && "Primary expression must not be NULL");
    assert(expr->type == EXPRESSION_PRIMARY);

    switch (expr->primary.type) {
        case PE_IDENTIFIER: {
            symbol_t *symbol = lookup_symbol(context, expr->primary.token.value);
            if (symbol == NULL) {
                source_position_t pos = expr->primary.token.position;
                append_compilation_error(&context->errors, (compilation_error_t) {
                    .kind = ERR_USE_OF_UNDECLARED_IDENTIFIER,
                    .location = pos,
                    .use_of_undeclared_identifier = {
                        .identifier = expr->primary.token.value,
                    },
                });
                return EXPR_ERR;
            }
            return (expression_result_t) {
                .c_type = symbol->c_type,
                .is_lvalue = true,
                .value = (ir_value_t) {
                    .kind = IR_VALUE_VAR,
                    .var = symbol->ir_ptr,
                },
            };
        }
        case PE_CONSTANT: {
            return visit_constant(context, expr);
        }
        case PE_STRING_LITERAL: {
            char *literal = replace_escape_sequences(expr->primary.token.value);
            return (expression_result_t) {
                .c_type = &CONST_CHAR_PTR,
                .is_lvalue = false,
                .value = (ir_value_t) {
                    .kind = IR_VALUE_CONST,
                    .constant = (ir_const_t) {
                        .kind = IR_CONST_STRING,
                        .type = &IR_PTR_CHAR,
                        .s = literal,
                    }
                }
            };
        }
        case PE_EXPRESSION: {
            return visit_expression(context, expr->primary.expression);
        }
        default: {
            // Unreachable
            fprintf(stderr, "Invalid primary expression\n");
            exit(1);
        }
    }
}

expression_result_t visit_constant(ir_gen_context_t *context, const expression_t *expr) {
    assert(context != NULL && "Context must not be NULL");
    assert(expr != NULL && "Expression must not be NULL");
    assert(expr->type == EXPRESSION_PRIMARY && expr->primary.type == PE_CONSTANT);
    assert(expr->primary.token.value != NULL && "Token value must not be NULL");

    switch (expr->primary.token.kind) {
        case TK_CHAR_LITERAL: {
            // TODO: Handle escape sequences, wide character literals.
            char c = expr->primary.token.value[0];
            // In C char literals are ints
            return (expression_result_t) {
                .c_type = &INT,
                .is_lvalue = false,
                .value = (ir_value_t) {
                    .kind = IR_VALUE_CONST,
                    .constant = (ir_const_t) {
                        .kind = IR_CONST_INT,
                        .type = &IR_I32,
                        .i = (int)c,
                    }
                }
            };
        }
        case TK_INTEGER_CONSTANT: {
            unsigned long long value;
            const type_t *c_type;
            decode_integer_constant(&expr->primary.token, &value, &c_type);
            return (expression_result_t) {
                .c_type = c_type,
                .is_lvalue = false,
                .value = (ir_value_t) {
                    .kind = IR_VALUE_CONST,
                    .constant = (ir_const_t) {
                            .kind = IR_CONST_INT,
                            .type = get_ir_type(c_type),
                            .i = value,
                    }
                }
            };
        }
        case TK_FLOATING_CONSTANT: {
            long double value;
            const type_t *c_type;
            decode_float_constant(&expr->primary.token, &value, &c_type);
            return (expression_result_t) {
                .c_type = c_type,
                .is_lvalue = false,
                .value = (ir_value_t) {
                    .kind = IR_VALUE_CONST,
                    .constant = (ir_const_t) {
                        .kind = IR_CONST_FLOAT,
                        .type = get_ir_type(c_type),
                        .f = value,
                    }
                }
            };
        }
        default: {
            // Unreachable
            fprintf(stderr, "Invalid constant expression\n");
            exit(1);
        }
    }
}

char *temp_name(ir_gen_context_t *context) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "t%d", context->local_id_counter++);
    return strdup(buffer);
}

char *gen_label(ir_gen_context_t *context) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "l%d", context->label_counter++);
    return strdup(buffer);
}

ir_var_t temp_var(ir_gen_context_t *context, const ir_type_t *type) {
    return (ir_var_t) {
        .type = type,
        .name = temp_name(context),
    };
}

const type_t *c_ptr_int_type() {
    return &UNSIGNED_LONG;
}

const ir_type_t* get_ir_type(const type_t *c_type) {
    assert(c_type != NULL && "C type must not be NULL");

    switch (c_type->kind) {
        case TYPE_INTEGER: {
            if (c_type->integer.is_signed) {
                switch (c_type->integer.size) {
                    case INTEGER_TYPE_BOOL:
                        return &IR_BOOL;
                    case INTEGER_TYPE_CHAR:
                        return &IR_I8;
                    case INTEGER_TYPE_SHORT:
                        return &IR_I16;
                    case INTEGER_TYPE_INT:
                        return &IR_I32;
                    default:
                        // long, long long
                        return &IR_I64;
                }
            } else {
                switch (c_type->integer.size) {
                    case INTEGER_TYPE_BOOL:
                        return &IR_BOOL;
                    case INTEGER_TYPE_CHAR:
                        return &IR_U8;
                    case INTEGER_TYPE_SHORT:
                        return &IR_U16;
                    case INTEGER_TYPE_INT:
                        return &IR_U32;
                    default:
                        // long, long long
                        return &IR_U64;
                }
            }
        }
        case TYPE_FLOATING: {
            switch (c_type->floating) {
                case FLOAT_TYPE_FLOAT:
                    return &IR_F32;
                default:
                    // double, long double
                    return &IR_F64;
            }
        }
        case TYPE_POINTER: {
            const ir_type_t *pointee = get_ir_type(c_type->pointer.base);
            ir_type_t *ir_type = malloc(sizeof(ir_type_t));
            *ir_type = (ir_type_t) {
                .kind = IR_TYPE_PTR,
                .ptr = {
                    .pointee = pointee,
                },
            };
            return ir_type;
        }
        case TYPE_FUNCTION: {
            const ir_type_t *ir_return_type = get_ir_type(c_type->function.return_type);
            const ir_type_t **ir_param_types = malloc(c_type->function.parameter_list->length * sizeof(ir_type_t*));
            for (size_t i = 0; i < c_type->function.parameter_list->length; i++) {
                const parameter_declaration_t *param = c_type->function.parameter_list->parameters[i];
                ir_param_types[i] = get_ir_type(param->type);
            }
            ir_type_t *ir_type = malloc(sizeof(ir_type_t));
            *ir_type = (ir_type_t) {
                .kind = IR_TYPE_FUNCTION,
                .function = {
                    .return_type = ir_return_type,
                    .params = ir_param_types,
                    .num_params = c_type->function.parameter_list->length,
                    .is_variadic = c_type->function.parameter_list->variadic
                },
            };
            return ir_type;
        }
        case TYPE_ARRAY: {
            // TODO
            assert(false && "Unimplemented");
        }
        default:
            return &IR_VOID;
    }
}

const ir_type_t *get_ir_ptr_type(const ir_type_t *pointee) {
    // TODO: cache these?
    ir_type_t *ir_type = malloc(sizeof(ir_type_t));
    *ir_type = (ir_type_t) {
        .kind = IR_TYPE_PTR,
        .ptr = {
            .pointee = pointee,
        },
    };
    return ir_type;
}

bool is_ir_integer_type(const ir_type_t *type) {
    switch (type->kind) {
        case IR_TYPE_I8:
        case IR_TYPE_I16:
        case IR_TYPE_I32:
        case IR_TYPE_I64:
        case IR_TYPE_U8:
        case IR_TYPE_U16:
        case IR_TYPE_U32:
        case IR_TYPE_U64:
            return true;
        default:
            return false;
    }
}

bool is_ir_float_type(const ir_type_t *type) {
    return type->kind == IR_TYPE_F32 || type->kind == IR_TYPE_F64;
}

expression_result_t convert_to_type(
        ir_gen_context_t *context,
        ir_value_t value,
        const type_t *from_type,
        const type_t *to_type,
        ir_var_t *result
) {
    const ir_type_t *result_type = get_ir_type(to_type);
    const ir_type_t *source_type;
    if (value.kind == IR_VALUE_CONST) {
        source_type = value.constant.type;
    } else {
        source_type = value.var.type;
    }

    if (result == NULL) {
        result = (ir_var_t *) alloca(sizeof(ir_var_t));
        *result = (ir_var_t) {
            .name = temp_name(context),
            .type = result_type,
        };
    } else {
        // Ensure that the result variable has the correct type
        assert(ir_types_equal(result->type, result_type));
    }

    if (is_ir_integer_type(result_type)) {
        if (is_ir_integer_type(source_type)) {
            // int -> int conversion
            if (size_of_type(source_type) > size_of_type(result_type)) {
                // Truncate
                IrBuildTrunc(context->builder, value, *result);
            } else if (size_of_type(source_type) < size_of_type(result_type)) {
                // Extend
                IrBuildExt(context->builder, value, *result);
            } else {
                // No conversion necessary
                IrBuildAssign(context->builder, value, *result);
            }
        } else if (is_ir_float_type(source_type)) {
            // float -> int
            IrBuildFtoI(context->builder, value, *result);
        } else if (source_type->kind == IR_TYPE_PTR) {
            // ptr -> int
            IrBuildPtoI(context->builder, value, *result);
        } else {
            // TODO, other conversions, proper error handling
            fprintf(stderr, "Unimplemented type conversion from %s to %s\n",
                    ir_fmt_type(alloca(256), 256, source_type),
                    ir_fmt_type(alloca(256), 256, result_type));
            return EXPR_ERR;
        }
    } else if (is_ir_float_type(result_type)) {
        if (is_ir_float_type(source_type)) {
            // float -> float conversion
            if (size_of_type(source_type) > size_of_type(result_type)) {
                // Truncate
                IrBuildTrunc(context->builder, value, *result);
            } else if (size_of_type(source_type) < size_of_type(result_type)) {
                // Extend
                IrBuildExt(context->builder, value, *result);
            } else {
                // No conversion necessary
                IrBuildAssign(context->builder, value, *result);
            }
        } else if (is_ir_integer_type(source_type)) {
            // int -> float
            IrBuildItoF(context->builder, value, *result);
        } else {
            // TODO: proper error handling
            fprintf(stderr, "Unimplemented type conversion from %s to %s\n",
                    ir_fmt_type(alloca(256), 256, source_type),
                    ir_fmt_type(alloca(256), 256, result_type));
            return EXPR_ERR;
        }
    } else {
        fprintf(stderr, "Unimplemented type conversion from %s to %s\n",
                ir_fmt_type(alloca(256), 256, source_type),
                ir_fmt_type(alloca(256), 256, result_type));
        return EXPR_ERR;
    }

    return (expression_result_t) {
        .c_type = to_type,
        .is_lvalue = false,
        .value = ir_value_for_var(*result),
    };
}

ir_value_t ir_value_for_var(ir_var_t var) {
    return (ir_value_t) {
        .kind = IR_VALUE_VAR,
        .var = var,
    };
}

const ir_type_t *get_ir_val_type(const ir_value_t value) {
    if (value.kind == IR_VALUE_VAR) {
        return value.var.type;
    } else {
        return value.constant.type;
    }
}

expression_result_t get_rvalue(ir_gen_context_t *context, expression_result_t res) {
    assert(res.is_lvalue && "Expected lvalue");
    assert(get_ir_val_type(res.value)->kind == IR_TYPE_PTR && "Expected pointer type");

    ir_var_t temp = temp_var(context, get_ir_val_type(res.value)->ptr.pointee);
    ir_var_t ptr = (ir_var_t) {
            .name = res.value.var.name,
            .type = res.value.var.type,
    };
    IrBuildLoad(context->builder, ir_value_for_var(ptr), temp);
    return (expression_result_t) {
        .c_type = res.c_type,
        .is_lvalue = false,
        .value = ir_value_for_var(temp),
    };
}
