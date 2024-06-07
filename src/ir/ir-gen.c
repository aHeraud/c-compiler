/// A module for performing semantic analysis and IR generation from an input AST. Semantic analysis and IR generation
/// are combined into a single traversal of the AST.

#include <stdio.h>
#include <string.h>
#include "ast.h"
#include "ir/ir-gen.h"
#include "ir/ir.h"
#include "ir/ir-builder.h"
#include "ir/cfg.h"
#include "ir/fmt.h"
#include "numeric-constants.h"
#include "errors.h"
#include "util/strings.h"

typedef struct Scope scope_t;
typedef struct Symbol symbol_t;

typedef struct IrGenContext {
    ir_module_t *module;

    hash_table_t global_map;
    hash_table_t function_definition_map;

    // State for the current function being visited
    ir_function_definition_t *function;
    const function_definition_t *c_function;
    ir_function_builder_t *builder;
    ir_instruction_node_t *alloca_tail;

    // List of compilation errors encountered during semantic analysis
    compilation_error_vector_t errors;
    // The current lexical scope
    scope_t *current_scope;
    // Counter for generating unique global variable names
    // These should be unique over the entire module
    int global_id_counter;
    // Counter for generating unique local variable names
    // These are only unique within the current function
    int local_id_counter;
    // Counter for generating unique labels
    int label_counter;
} ir_gen_context_t;

typedef enum ExpressionResultKind {
    EXPR_RESULT_ERR,
    EXPR_RESULT_VALUE,
    EXPR_RESULT_INDIRECTION,
} expression_result_kind_t;

typedef struct ExpressionResult expression_result_t;
typedef struct ExpressionResult {
    expression_result_kind_t kind;
    const type_t *c_type;
    bool is_lvalue;
    bool addr_of;
    bool is_string_literal;
    union {
        ir_value_t value;
        expression_result_t *indirection_inner;
    };
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
    .kind = EXPR_RESULT_ERR,
    .c_type = NULL,
    .is_lvalue = false,
    .is_string_literal = false,
    .addr_of = false,
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

char *global_name(ir_gen_context_t *context);
char *temp_name(ir_gen_context_t *context);
ir_var_t temp_var(ir_gen_context_t *context, const ir_type_t *type);
char *gen_label(ir_gen_context_t *context);

ir_value_t ir_make_const_int(const ir_type_t *type, long long value);
ir_value_t ir_get_zero_value(ir_gen_context_t *context, const ir_type_t *type);

const ir_type_t *ir_get_type_of_value(const ir_value_t value);

/**
 * Helper to insert alloca instructions for local variables at the top of the function.
 */
void insert_alloca(ir_gen_context_t *context, const ir_type_t *ir_type, ir_var_t result);

/**
 * Get the C integer type that is the same width as a pointer.
 */
const type_t *c_ptr_int_type();

/**
 * Get the IR integer type that is the same width as a pointer.
 */
const ir_type_t *ir_ptr_int_type();

/**
 * Convert an IR value from one type to another.
 * Will generate conversion instructions if necessary, and store the result in a new variable,
 * with the exception of trivial conversions or constant values.
 * @param context The IR generation context
 * @param value   The value to convert
 * @param from_type The C type of the value
 * @param to_type  The C type to convert the value to
 * @return The resulting ir value and its corresponding c type
 */
expression_result_t convert_to_type(ir_gen_context_t *context, ir_value_t value, const type_t *from_type, const type_t *to_type);

expression_result_t get_boolean_value(
    ir_gen_context_t *context,
    ir_value_t value,
    const type_t *c_type,
    const expression_t *expr
);

/**
 * Get the IR type that corresponds to a specific C type.
 * @param c_type
 * @return corresponding IR type
 */
const ir_type_t* get_ir_type(ir_gen_context_t *context, const type_t *c_type);

/**
 * Get the IR type that is a pointer to the specified IR type
 * @param pointee The type that the pointer points to
 * @return The pointer type
 */
const ir_type_t *get_ir_ptr_type(const ir_type_t *pointee);

ir_value_t ir_value_for_var(ir_var_t var);

ir_value_t get_indirect_ptr(ir_gen_context_t *context, expression_result_t res);
expression_result_t get_rvalue(ir_gen_context_t *context, expression_result_t res);

void enter_scope(ir_gen_context_t *context) {
    assert(context != NULL);
    scope_t *scope = malloc(sizeof(scope_t));
    *scope = (scope_t) {
            .symbols = hash_table_create_string_keys(256),
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

void ir_append_function_ptr(ir_function_ptr_vector_t *vec, ir_function_definition_t *function) {
    assert(vec != NULL);
    assert(function != NULL);
    VEC_APPEND(vec, function);
}

void ir_append_global_ptr(ir_global_ptr_vector_t *vec, ir_global_t *global) {
    assert(vec != NULL);
    assert(global != NULL);
    VEC_APPEND(vec, global);
}

void ir_visit_translation_unit(ir_gen_context_t *context, const translation_unit_t *translation_unit);
void ir_visit_function(ir_gen_context_t *context, const function_definition_t *function);
void ir_visit_statement(ir_gen_context_t *context, const statement_t *statement);
void ir_visit_if_statement(ir_gen_context_t *context, const statement_t *statement);
void ir_visit_return_statement(ir_gen_context_t *context, const statement_t *statement);
void ir_visit_while_statement(ir_gen_context_t *context, const statement_t *statement);
void ir_visit_global_declaration(ir_gen_context_t *context, const declaration_t *declaration);
void ir_visit_declaration(ir_gen_context_t *context, const declaration_t *declaration);
expression_result_t ir_visit_expression(ir_gen_context_t *context, const expression_t *expression);
expression_result_t ir_visit_array_subscript_expression(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_primary_expression(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_constant(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_call_expression(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_binary_expression(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_additive_binexpr(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_assignment_binexpr(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_bitwise_binexpr(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_comparison_binexpr(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_multiplicative_binexpr(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_ternary_expression(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_unary_expression(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_bitwise_not_unexpr(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_address_of_unexpr(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_indirection_unexpr(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_logical_expression(ir_gen_context_t *context, const expression_t *expr);

ir_gen_result_t generate_ir(const translation_unit_t *translation_unit) {
    ir_gen_context_t context = {
        .module = malloc(sizeof(ir_module_t)),
        .global_map = hash_table_create_string_keys(256),
        .function_definition_map = hash_table_create_string_keys(256),
        .function = NULL,
        .builder = NULL,
        .errors = (compilation_error_vector_t) { .size = 0, .capacity = 0, .buffer = NULL },
        .current_scope = NULL,
        .global_id_counter = 0,
        .local_id_counter = 0,
    };

    *context.module = (ir_module_t) {
        .name = "module", // TODO: get the name of the input file?
        .functions = (ir_function_ptr_vector_t) { .size = 0, .capacity = 0, .buffer = NULL },
        .globals = (ir_global_ptr_vector_t) { .size = 0, .capacity = 0, .buffer = NULL },
    };

    ir_visit_translation_unit(&context, translation_unit);

    // Cleanup (note that this doesn't free the pointers stored in the hash tables).
    hash_table_destroy(&context.global_map);
    hash_table_destroy(&context.function_definition_map);

    ir_gen_result_t result = {
        .module = context.module,
        .errors = context.errors,
    };
    return result;
}

void ir_visit_translation_unit(ir_gen_context_t *context, const translation_unit_t *translation_unit) {
    enter_scope(context);

    for (size_t i = 0; i < translation_unit->length; i++) {
        external_declaration_t *external_declaration = (external_declaration_t*) translation_unit->external_declarations[i];
        switch (external_declaration->type) {
            case EXTERNAL_DECLARATION_FUNCTION_DEFINITION: {
                ir_visit_function(context, external_declaration->function_definition);
                break;
            }
            case EXTERNAL_DECLARATION_DECLARATION: {
                // A single declaration may declare multiple variables.
                for (int j = 0; j < external_declaration->declaration.length; j += 1) {
                    ir_visit_global_declaration(context, external_declaration->declaration.declarations[j]);
                }
                break;
            }
        }
    }

    leave_scope(context);
}

void ir_visit_function(ir_gen_context_t *context, const function_definition_t *function) {
    context->function = malloc(sizeof(ir_function_definition_t));
    *context->function = (ir_function_definition_t) {
        .name = function->identifier->value,
    };
    context->c_function = function;
    context->builder = ir_builder_create();
    context->alloca_tail = ir_builder_get_position(context->builder);

    const type_t function_c_type = {
        .kind = TYPE_FUNCTION,
        .function = {
            .return_type = function->return_type,
            .parameter_list = function->parameter_list,
        },
    };
    const ir_type_t *function_type = get_ir_type(context,&function_c_type);
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
        bool already_defined =
            hash_table_lookup(&context->function_definition_map, function->identifier->value, NULL);
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
        type_t *c_type = malloc(sizeof(type_t));
        *c_type = function_c_type;
        symbol_t *symbol = malloc(sizeof(symbol_t));
        *symbol = (symbol_t) {
            .kind = SYMBOL_FUNCTION,
            .identifier = function->identifier,
            .name = function->identifier->value,
            .c_type = c_type,
            .ir_type = function_type,
            // Not actually a pointer, but we use the ir_ptr field to store the function name
            .ir_ptr = (ir_var_t) {
                .name = function->identifier->value,
                .type = function_type,
            },
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
        const ir_type_t *ir_param_type = get_ir_type(context,param->type);
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
        insert_alloca(context, ir_param_type, param_ptr);

        // Store the parameter in the stack slot
        ir_build_store(context->builder, ir_value_for_var(param_ptr), ir_value_for_var(ir_param));

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

    ir_visit_statement(context, function->body);

    leave_scope(context);

    context->function->body = ir_builder_finalize(context->builder);

    hash_table_insert(&context->function_definition_map, function->identifier->value, context->function);
    ir_append_function_ptr(&context->module->functions, context->function);

    if (context->errors.size > 0) {
        // There were errors processing the function, skip IR validation.
        return;
    }

    // There were no semantic errors, so the generated IR should be valid.
    // Validate the IR to catch any bugs in the compiler.
    ir_validation_error_vector_t errors = ir_validate_function(context->function);
    if (errors.size > 0) {
        // We will just print the first error and exit for now.
        const char* error_message = errors.buffer[0].message;
        const char *instruction = ir_fmt_instr(alloca(512), 512, errors.buffer[0].instruction);
        const char *function_type_str = ir_fmt_type(alloca(512), 512, context->function->type);
        fprintf(stderr, "IR validation error in function %s %s\n", function->identifier->value, function_type_str);
        fprintf(stderr, "At instruction: %s\n", instruction);
        fprintf(stderr, "%s\n", error_message);
        exit(1);
    }

    // Create the control flow graph for the function, and prune unreachable blocks
    ir_control_flow_graph_t cfg = ir_create_control_flow_graph(context->function);
    ir_prune_control_flow_graph(&cfg);

    // Handle implicit return statements
    // The c99 standard specifies the following:
    // * 6.9.1 Function definitions - "If the } that terminates a function is reached, and the value of the function
    //                                 call is used by the caller, the behavior is undefined"
    // * 5.1.2.2.3 Program termination - "If the return type of the main function is a type compatible with int, ...
    //                                    reaching the } that terminates the main function returns a value of 0. If the
    //                                    return type is not compatible with int, the termination status returned to the
    //                                    host environment is unspecified."
    // To handle this: for any basic block that does not have a successor and which does not end in a return, we will
    // add a `return 0` instruction.
    // TODO: return undefined value for non-int main and non-main functions?
    for (size_t i = 0; i < cfg.basic_blocks.size; i += 1) {
        ir_basic_block_t *bb = cfg.basic_blocks.buffer[i];

        if (bb->successors.size > 0) continue;

        if (bb->instructions.size == 0 || bb->instructions.buffer[bb->instructions.size - 1]->opcode != IR_RET) {
            ir_instruction_t *ret = malloc(sizeof(ir_instruction_t));
            if (context->function->type->function.return_type->kind == IR_TYPE_VOID) {
                *ret = (ir_instruction_t) {
                    .opcode = IR_RET,
                    .ret = {
                        .has_value = false,
                    }
                };
            } else {
                *ret = (ir_instruction_t) {
                    .opcode = IR_RET,
                    .ret = {
                        .has_value = true,
                        .value = ir_get_zero_value(context, context->function->type->function.return_type),
                    },
                };
            }
            ir_instruction_ptr_vector_t *instructions = &bb->instructions;
            VEC_APPEND(instructions, ret);
        }
    }

    // Linearize the control flow graph
    // TODO: it's a bit awkward to operate on the cfg then return to the linearized result,
    //       may want to just store the cfg instead.
    ir_instruction_vector_t linearized = ir_linearize_cfg(&cfg);
    context->function->body = linearized;
}

void ir_visit_statement(ir_gen_context_t *context, const statement_t *statement) {
    assert(context != NULL && "Context must not be NULL");
    assert(statement != NULL && "Statement must not be NULL");

    switch (statement->type) {
        case STATEMENT_COMPOUND: {
            enter_scope(context);
            for (size_t i = 0; i < statement->compound.block_items.size; i++) {
                block_item_t *block_item = (block_item_t*) statement->compound.block_items.buffer[i];
                switch (block_item->type) {
                    case BLOCK_ITEM_STATEMENT: {
                        ir_visit_statement(context, block_item->statement);
                        break;
                    }
                    case BLOCK_ITEM_DECLARATION: {
                        ir_visit_declaration(context, block_item->declaration);
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
            ir_visit_expression(context, statement->expression);
            break;
        }
        case STATEMENT_IF: {
            ir_visit_if_statement(context, statement);
            break;
        }
        case STATEMENT_RETURN: {
            ir_visit_return_statement(context, statement);
            break;
        }
        case STATEMENT_WHILE: {
            ir_visit_while_statement(context, statement);
            break;
        }
        default:
            fprintf(stderr, "%s:%d: Invalid statement type\n", __FILE__, __LINE__);
            exit(1);
    }
}

void ir_visit_if_statement(ir_gen_context_t *context, const statement_t *statement) {
    assert(context != NULL && "Context must not be NULL");
    assert(statement != NULL && "Statement must not be NULL");
    assert(statement->type == STATEMENT_IF);

    // Evaluate the condition
    expression_result_t condition = ir_visit_expression(context, statement->if_.condition);

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
        condition = convert_to_type(context, condition.value, condition.c_type, c_ptr_int_type());
    }

    bool condition_is_floating = is_floating_type(condition.c_type);
    ir_value_t zero = condition_is_floating ?
          (ir_value_t) {
              .kind = IR_VALUE_CONST,
              .constant = (ir_const_t) {
                  .kind = IR_CONST_FLOAT,
                  .type = get_ir_type(context,condition.c_type),
                  .f = 0.0,
              },
          } :
          (ir_value_t) {
              .kind = IR_VALUE_CONST,
              .constant = (ir_const_t) {
                  .kind = IR_CONST_INT,
                  .type = get_ir_type(context,condition.c_type),
                  .i = 0,
              },
          };
    ir_var_t condition_var = (ir_var_t) {
        .name = temp_name(context),
        .type = &IR_BOOL,
    };
    ir_build_eq(context->builder, condition.value, zero, condition_var);
    ir_build_br_cond(context->builder, ir_value_for_var(condition_var), false_label != NULL ? false_label : end_label);

    // Generate code for the true branch
    ir_visit_statement(context, statement->if_.true_branch);

    if (statement->if_.false_branch != NULL) {
        // Jump to the end of the if statement
        ir_build_br(context->builder, end_label);

        // Label for the false branch
        ir_build_nop(context->builder, false_label);

        // Generate code for the false branch
        ir_visit_statement(context, statement->if_.false_branch);
    }

    ir_build_nop(context->builder, end_label);
}

void ir_visit_return_statement(ir_gen_context_t *context, const statement_t *statement) {
    assert(context != NULL && "Context must not be NULL");
    assert(statement != NULL && "Statement must not be NULL");
    assert(statement->type == STATEMENT_RETURN);

    const ir_type_t *return_type = context->function->type->function.return_type;
    const type_t *c_return_type = context->c_function->return_type;

    if (statement->return_.expression != NULL) {
        expression_result_t value = ir_visit_expression(context, statement->return_.expression);
        if (value.c_type == NULL) {
            // Error occurred while evaluating the return value
            return;
        }

        if (value.is_lvalue) {
            value = get_rvalue(context, value);
        }

        // Implicit conversion to the return type
        if (!ir_types_equal(ir_get_type_of_value(value.value), return_type)) {
            value = convert_to_type(context, value.value, value.c_type, c_return_type);
            if (value.c_type == NULL) {
                // Error occurred while converting the return value
                return;
            }
        }

        ir_build_ret(context->builder, value.value);
    } else {
        if (return_type->kind != TYPE_VOID) {
            append_compilation_error(&context->errors, (compilation_error_t) {
                // TODO
            });
        }
        ir_build_ret_void(context->builder);
    }
}

void ir_visit_while_statement(ir_gen_context_t *context, const statement_t *statement) {
    assert(context != NULL && "Context must not be NULL");
    assert(statement != NULL && "Statement must not be NULL");
    assert(statement->type == STATEMENT_WHILE);

    char *loop_label = gen_label(context);
    char *end_label = gen_label(context);

    // Label for the start of the loop
    ir_build_nop(context->builder, loop_label);

    // Evaluate the condition
    expression_result_t condition = ir_visit_expression(context, statement->while_.condition);

    // Constraint for all loops:
    // * The condition must have a scalar type
    // * The loop body executes until the condition evaluates to 0

    if (condition.is_lvalue) {
        condition = get_rvalue(context, condition);
    }

    if (!is_scalar_type(condition.c_type)) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_LOOP_CONDITION_TYPE,
            .location = statement->expression->span.start,
            .invalid_loop_condition_type = {
                .type = condition.c_type,
            },
        });
        return;
    }

    const ir_type_t *condition_ir_type = ir_get_type_of_value(condition.value);
    ir_value_t zero = ir_get_zero_value(context, condition_ir_type);
    ir_var_t condition_var = temp_var(context, &IR_BOOL);
    ir_build_eq(context->builder, condition.value, zero, condition_var);
    ir_build_br_cond(context->builder, ir_value_for_var(condition_var), end_label);

    // Execute the loop body
    ir_visit_statement(context, statement->while_.body);

    // Jump back to the start of the loop
    ir_build_br(context->builder, loop_label);

    // Label for the end of the loop
    ir_build_nop(context->builder, end_label);
}

void ir_visit_global_declaration(ir_gen_context_t *context, const declaration_t *declaration) {
    assert(context != NULL && "Context must not be NULL");
    assert(declaration != NULL && "Declaration must not be NULL");

    symbol_t *symbol = lookup_symbol_in_current_scope(context, declaration->identifier->value);
    ir_global_t *global = NULL;
    if (symbol != NULL) {
        // Global scope is a bit special. Re-declarations are allowed if the types match, however if
        // the global was previously given a value (e.g. has an initializer or is a function definition),
        // then it is a re-definition error.

        if (declaration->type->kind == TYPE_FUNCTION) {
            // Check if we've already processed a function definition with the same name
            if (hash_table_lookup(&context->function_definition_map, declaration->identifier->value, NULL)) {
                append_compilation_error(&context->errors, (compilation_error_t) {
                    .location = declaration->identifier->position,
                    .kind = ERR_REDEFINITION_OF_SYMBOL,
                    .redefinition_of_symbol = {
                        .redefinition = declaration->identifier,
                        .previous_definition = symbol->identifier,
                    },
                });
            }
            // Check if the types match. Re-declaration is allowed if the types match.
            if (!ir_types_equal(symbol->ir_type, get_ir_type(context,declaration->type))) {
                append_compilation_error(&context->errors, (compilation_error_t) {
                    .location = declaration->identifier->position,
                    .kind = ERR_REDEFINITION_OF_SYMBOL,
                    .redefinition_of_symbol = {
                        .redefinition = declaration->identifier,
                        .previous_definition = symbol->identifier,
                    },
                });
            }
            return;
        } else {
            // Look up the global in the module's global list.
            assert(hash_table_lookup(&context->global_map, declaration->identifier->value, (void**) &global));
            assert(global != NULL);
            // If the types are not equal, or the global has already been initialized, it is a redefinition error.
            if (!ir_types_equal(global->type, get_ir_type(context,declaration->type)) || global->initialized) {
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
        }
    } else {
        // Create a new global symbol
        symbol = malloc(sizeof(symbol_t));

        bool is_function = declaration->type->kind == TYPE_FUNCTION;

        char* name;
        if (is_function) {
            size_t len = strlen(declaration->identifier->value) + 2;
            name = malloc(len);
            snprintf(name, len, "%s", declaration->identifier->value);
        } else {
            name = global_name(context);
        }

        const ir_type_t *ir_type = get_ir_type(context,declaration->type);
        *symbol = (symbol_t) {
            .kind = is_function ? SYMBOL_FUNCTION : SYMBOL_GLOBAL_VARIABLE,
            .identifier = declaration->identifier,
            .name = declaration->identifier->value,
            .c_type = declaration->type,
            .ir_type = get_ir_type(context,declaration->type),
            .ir_ptr = (ir_var_t) {
                .name = name,
                .type = is_function ? ir_type : get_ir_ptr_type(ir_type),
            },
        };
        declare_symbol(context, symbol);

        // Add the global to the module's global list
        // *Function declarations are not IR globals*
        if (declaration->type->kind != TYPE_FUNCTION) {
            global = malloc(sizeof(ir_global_t));
            *global = (ir_global_t) {
                .name = symbol->ir_ptr.name,
                .type = symbol->ir_ptr.type,
                .initialized = declaration->initializer != NULL,
            };

            hash_table_insert(&context->global_map, symbol->name, global);
            ir_append_global_ptr(&context->module->globals, global);
        }
    }

    // Visit the initializer if present
    if (declaration->initializer != NULL) {
        assert(global != NULL);

        // Set up function builder state for the global initializer
        // (a valid initializer is a constant expression which will generate no instructions, but we use the same
        // code to generate IR for all expressions)
        context->function = & (ir_function_definition_t) {
            .name = "global_initializer",
            .type = NULL,
            .num_params = 0,
            .params = NULL,
            .is_variadic = false,
            .body = NULL,
        };
        context->builder = ir_builder_create();

        expression_result_t result = ir_visit_expression(context, declaration->initializer);
        if (result.c_type == NULL) return; // Invalid initializer

        // Typecheck/convert the initializer
        result = convert_to_type(context, result.value, result.c_type, declaration->type);

        // Delete the builder, throw away any generated instructions
        ir_builder_destroy(context->builder);
        context->function = NULL;

        if (result.value.kind != IR_VALUE_CONST) {
            // The initializer must be a constant expression
            append_compilation_error(&context->errors, (compilation_error_t) {
                .kind = ERR_GLOBAL_INITIALIZER_NOT_CONSTANT,
                .location = declaration->initializer->span.start,
                .global_initializer_not_constant = {
                    .declaration = declaration,
                },
            });
            return;
        }

        global->value = result.value.constant;
    } else if (global != NULL) {
        // Default value for uninitialized global variables
        if (is_floating_type(declaration->type)) {
            global->value = (ir_const_t) {
                .kind = IR_CONST_FLOAT,
                .type = symbol->ir_type,
                .f = 0.0,
            };
        } else {
            global->value = (ir_const_t) {
                .kind = IR_CONST_INT,
                .type = symbol->ir_type,
                .i = 0,
            };
        }
    }
}

void ir_visit_declaration(ir_gen_context_t *context, const declaration_t *declaration) {
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

    const ir_type_t *ir_type = get_ir_type(context,declaration->type);

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
    insert_alloca(context, ir_type, symbol->ir_ptr);

    // Evaluate the initializer if present, and store the result in the allocated storage
    if (declaration->initializer != NULL) {
        expression_result_t result = ir_visit_expression(context, declaration->initializer);

        // Error occurred while evaluating the initializer
        if (result.kind == EXPR_RESULT_ERR) return;

        // Incompatible types
        if (result.c_type == NULL) return;

        // If the initializer is an lvalue, load the value
        // TODO: not sure that this is correct
        if (result.is_lvalue) result = get_rvalue(context, result);

        // Verify that the types are compatible, convert if necessary
        result = convert_to_type(context, result.value, result.c_type, declaration->type);

        // Store the result in the allocated storage
        ir_build_store(context->builder, ir_value_for_var(symbol->ir_ptr), result.value);
    }
}

expression_result_t ir_visit_expression(ir_gen_context_t *context, const expression_t *expression) {
    assert(context != NULL && "Context must not be NULL");
    assert(expression != NULL && "Expression must not be NULL");

    switch (expression->type) {
        case EXPRESSION_ARRAY_SUBSCRIPT: {
            return ir_visit_array_subscript_expression(context, expression);
        }
        case EXPRESSION_BINARY:
            return ir_visit_binary_expression(context, expression);
        case EXPRESSION_CALL: {
            return ir_visit_call_expression(context, expression);
        }
        case EXPRESSION_CAST: {
            assert(false && "Cast not implemented");
        }
        case EXPRESSION_MEMBER_ACCESS: {
            assert(false && "Member access not implemented");
        }
        case EXPRESSION_PRIMARY:
            return ir_visit_primary_expression(context, expression);
        case EXPRESSION_SIZEOF: {
            assert(false && "sizeof operator not implemented");
        }
        case EXPRESSION_TERNARY: {
            return ir_visit_ternary_expression(context, expression);
        }
        case EXPRESSION_UNARY: {
            return ir_visit_unary_expression(context, expression);
        }
    }

    // TODO
    return EXPR_ERR;
}

expression_result_t ir_visit_array_subscript_expression(ir_gen_context_t *context, const expression_t *expr) {
    expression_result_t target = ir_visit_expression(context, expr->array_subscript.array);
    if (target.kind == EXPR_RESULT_ERR) return EXPR_ERR;

    // The target must be an array or a pointer
    if (target.c_type->kind != TYPE_ARRAY && target.c_type->kind != TYPE_POINTER) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_SUBSCRIPT_TARGET,
            .location = expr->array_subscript.array->span.start, // TODO, use the '[' token position?
        });
        return EXPR_ERR;
    }

    ir_value_t base_ptr;
    if (target.kind == IR_VALUE_VAR) {
        assert(ir_get_type_of_value(target.value)->kind == IR_TYPE_PTR);
        base_ptr = target.value;
    } else {
        base_ptr = get_indirect_ptr(context, target);
    }

    const ir_type_t *ptr_type = ir_get_type_of_value(base_ptr);
    const ir_type_t *element_type = ptr_type->ptr.pointee->kind == IR_TYPE_PTR
        ? ptr_type->ptr.pointee->ptr.pointee
        : ptr_type->ptr.pointee->array.element;

    expression_result_t index = ir_visit_expression(context, expr->array_subscript.index);
    if (index.kind == EXPR_RESULT_ERR) return EXPR_ERR;
    if (index.is_lvalue) index = get_rvalue(context, index);
    assert(index.kind == EXPR_RESULT_VALUE);

    // The subscript must have an integer type
    if (!is_integer_type(index.c_type)) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_SUBSCRIPT_TYPE,
            .location = expr->array_subscript.index->span.start,
        });
        return EXPR_ERR;
    }

    // Note:
    // Currently we directly calculate the pointer to the requested element. We could introduce
    // a new IR instruction that abstracts this, which may simplify optimizations later on.
    // As an example, consider the LLVM GetElementPtr instruction.

    // Calculate the offset from the base pointer to the requested element
    ssize_t stride = size_of_type_bytes(element_type);
    ir_value_t offset;
    if (index.value.kind == IR_VALUE_CONST) {
        // Constant folding
        offset = ir_make_const_int(ir_ptr_int_type(), index.value.constant.i * stride);
    } else {
        ir_value_t stride_val = ir_make_const_int(ir_ptr_int_type(), stride);
        ir_var_t index_ext = temp_var(context, ir_ptr_int_type());
        ir_build_ext(context->builder, index.value, index_ext);
        ir_var_t temp = temp_var(context, ir_ptr_int_type());
        ir_build_mul(context->builder, ir_value_for_var(index_ext), stride_val, temp);
        offset = ir_value_for_var(temp);
    }

    // Calculate the new pointer.
    ir_value_t result;
    if (offset.kind == IR_VALUE_CONST && offset.constant.i == 0) {
        ir_var_t converted_ptr = temp_var(context, get_ir_ptr_type(element_type));
        ir_build_bitcast(context->builder, base_ptr, converted_ptr);
        result = ir_value_for_var(converted_ptr);
    } else {
        // I know this looks like a lot of stuff, but everything except the add is just a pseudo-instruction.
        const ir_type_t *new_ptr_type = get_ir_ptr_type(element_type);
        ir_var_t base_addr = temp_var(context, ir_ptr_int_type());
        ir_build_ptoi(context->builder, base_ptr, base_addr);
        ir_var_t addr = temp_var(context, ir_ptr_int_type());
        ir_build_add(context->builder, ir_value_for_var(base_addr), offset, addr);
        ir_var_t new_ptr = temp_var(context, new_ptr_type);
        ir_build_itop(context->builder, ir_value_for_var(addr), new_ptr);
        result = ir_value_for_var(new_ptr);
    }

    const type_t *result_type = target.c_type->kind == TYPE_ARRAY
        ? target.c_type->array.element_type
        : target.c_type->pointer.base;
    return (expression_result_t) {
        .kind = EXPR_RESULT_VALUE,
        .c_type = result_type,
        .is_lvalue = true,
        .is_string_literal = false,
        .addr_of = false,
        .value = result,
    };
}

expression_result_t ir_visit_call_expression(ir_gen_context_t *context, const expression_t *expr) {
    expression_result_t function = ir_visit_expression(context, expr->call.callee);
    ptr_vector_t arguments = (ptr_vector_t) {
        .size = 0,
        .capacity = 0,
        .buffer = NULL,
    };

    if (function.kind == EXPR_RESULT_ERR) return EXPR_ERR;

    // Function can be a function, or a pointer to a function
    // TODO: handle function pointers
    if (function.c_type->kind != TYPE_FUNCTION) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_CALL_TARGET_NOT_FUNCTION,
            .location = expr->call.callee->span.start,
            .call_target_not_function = {
                .type = function.c_type,
            }
        });
        return EXPR_ERR;
    }

    // Check that the number of arguments matches function arity
    size_t expected_args_count = function.c_type->function.parameter_list->length;
    bool variadic = function.c_type->function.parameter_list->variadic;
    size_t actual_args_count = expr->call.arguments.size;
    if ((variadic && actual_args_count < expected_args_count) || (!variadic && actual_args_count != expected_args_count)) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_CALL_ARGUMENT_COUNT_MISMATCH,
            .location = expr->call.callee->span.start,
            .call_argument_count_mismatch = {
                .expected = expected_args_count,
                .actual = actual_args_count,
            }
        });
        return EXPR_ERR;
    }

    // Evaluate the arguments
    ir_value_t *args = malloc(sizeof(ir_value_t) * actual_args_count);
    for (size_t i = 0; i < actual_args_count; i += 1) {
        expression_result_t arg = ir_visit_expression(context, expr->call.arguments.buffer[i]);

        // Error occurred while evaluating the argument
        if (arg.kind == EXPR_RESULT_ERR) return EXPR_ERR;

        if (arg.is_lvalue) arg = get_rvalue(context, arg);

        // Implicit conversion to the parameter type
        // Variadic arguments are _NOT_ converted, they are just passed as is.
        if (i < function.c_type->function.parameter_list->length) {
            const type_t *param_type = function.c_type->function.parameter_list->parameters[i]->type;
            arg = convert_to_type(context, arg.value, arg.c_type, param_type);

            // Conversion was invalid
            if (arg.kind == EXPR_RESULT_ERR) return EXPR_ERR;
        }

        args[i] = arg.value;
    }

    // Emit the call instruction
    ir_var_t *result = NULL;
    if (function.c_type->function.return_type->kind != TYPE_VOID) {
        result = (ir_var_t*) malloc(sizeof(ir_var_t));
        *result = temp_var(context, get_ir_type(context,function.c_type->function.return_type));
    }
    assert(function.value.kind == IR_VALUE_VAR); // TODO: is it possible to directly call a constant?
    ir_build_call(context->builder, function.value.var, args, actual_args_count, result);

    ir_value_t result_value = result != NULL ?
        ir_value_for_var(*result) : (ir_value_t) {
        .kind = IR_VALUE_CONST,
        .constant = (ir_const_t) {
            .kind = IR_CONST_INT,
            .type = &IR_VOID,
        },
    };

    return (expression_result_t) {
        .kind = EXPR_RESULT_VALUE,
        .c_type = function.c_type->function.return_type,
        .is_lvalue = false,
        .is_string_literal = false,
        .addr_of = false,
        .value = result_value,
    };
}

expression_result_t ir_visit_binary_expression(ir_gen_context_t *context, const expression_t *expr) {
    assert(context != NULL && "Context must not be NULL");
    assert(expr != NULL && "Expression must not be NULL");
    assert(expr->type == EXPRESSION_BINARY);

    switch (expr->binary.type) {
        case BINARY_ARITHMETIC: {
            if (expr->binary.arithmetic_operator == BINARY_ARITHMETIC_ADD ||
                expr->binary.arithmetic_operator == BINARY_ARITHMETIC_SUBTRACT) {
                return ir_visit_additive_binexpr(context, expr);
            } else {
                return ir_visit_multiplicative_binexpr(context, expr);
            }
        }
        case BINARY_ASSIGNMENT: {
            return ir_visit_assignment_binexpr(context, expr);
        }
        case BINARY_BITWISE: {
            return ir_visit_bitwise_binexpr(context, expr);
        }
        case BINARY_COMMA: {
            // TODO
            assert(false && "comma operator not implemented");
        }
        case BINARY_COMPARISON: {
            return ir_visit_comparison_binexpr(context, expr);
        }
        case BINARY_LOGICAL: {
            return ir_visit_logical_expression(context, expr);
        }
    }
}

expression_result_t ir_visit_additive_binexpr(ir_gen_context_t *context, const expression_t *expr) {
    const type_t *result_type;
    const ir_type_t *ir_result_type;

    // Evaluate the left and right operands.
    expression_result_t left = ir_visit_expression(context, expr->binary.left);
    expression_result_t right = ir_visit_expression(context, expr->binary.right);

    // Bubble up errors if the operands are invalid.
    if (left.kind == EXPR_RESULT_ERR || right.kind == EXPR_RESULT_ERR) return EXPR_ERR;

    bool is_addition = expr->binary.operator->kind == TK_PLUS
                     || expr->binary.operator->kind == TK_PLUS_ASSIGN;

    if (left.is_lvalue) left = get_rvalue(context, left);
    if (right.is_lvalue) right = get_rvalue(context, right);

    // Both operands must have arithmetic type, or one operand must be a pointer and the other an integer.
    if (is_arithmetic_type(left.c_type) && is_arithmetic_type(right.c_type)) {
        // Integer/Float + Integer/Float
        result_type = get_common_type(left.c_type, right.c_type);
        ir_result_type = get_ir_type(context,result_type);

        left = convert_to_type(context, left.value, left.c_type, result_type);
        right = convert_to_type(context, right.value, right.c_type, result_type);

        ir_value_t result;
        if (left.value.kind == IR_VALUE_CONST && right.value.kind == IR_VALUE_CONST) {
            // constant folding
            result = (ir_value_t) {
                .kind = IR_VALUE_CONST,
                .constant = (ir_const_t) {
                    .kind = is_floating_type(result_type) ? IR_CONST_FLOAT : IR_CONST_INT,
                    .type = ir_result_type,
                    .i = 0,
                }
            };

            if (is_floating_type(result_type)) {
                result.constant.f = is_addition ? left.value.constant.f + right.value.constant.f
                                                : left.value.constant.f - right.value.constant.f;
            } else {
                result.constant.i = is_addition ? left.value.constant.i + right.value.constant.i
                                                : left.value.constant.i - right.value.constant.i;
            }
        } else {
            // Generate a temp var to store the result
            ir_var_t temp = temp_var(context, ir_result_type);
            is_addition ? ir_build_add(context->builder, left.value, right.value, temp)
                        : ir_build_sub(context->builder, left.value, right.value, temp);
            result = ir_value_for_var(temp);
        }

        return (expression_result_t) {
            .kind = EXPR_RESULT_VALUE,
            .c_type = result_type,
            .is_lvalue = false,
            .is_string_literal = false,
            .value = result,
        };
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

        ir_value_t result;

        // Extend/truncate the integer to the size of a pointer.
        integer_operand = convert_to_type(context, integer_operand.value, integer_operand.c_type, c_ptr_int_type());

        // Size of the pointee type
        ssize_t stride = size_of_type_bits(get_ir_type(context, pointer_operand.c_type->pointer.base));

        if (integer_operand.value.kind == IR_VALUE_CONST && pointer_operand.value.kind == IR_VALUE_CONST) {
            // constant folding
            long long val = is_addition ? pointer_operand.value.constant.i + (integer_operand.value.constant.i * stride)
                                           : pointer_operand.value.constant.i - (integer_operand.value.constant.i * stride);
            result = (ir_value_t) {
                .kind = IR_VALUE_CONST,
                .constant = (ir_const_t) {
                    .kind = IR_CONST_INT,
                    .type = ir_result_type,
                    .i = val,
                }
            };
        } else {
            // Multiply the integer by the size of the pointee type.
            ir_value_t size_constant = (ir_value_t) {
                .kind = IR_VALUE_CONST,
                .constant = (ir_const_t) {
                    .kind = IR_CONST_INT,
                    .type = get_ir_type(context,c_ptr_int_type()),
                    .i = stride
                }
            };
            ir_var_t temp = temp_var(context, get_ir_type(context,c_ptr_int_type()));
            ir_build_mul(context->builder, integer_operand.value, size_constant, temp);
            ir_value_t temp_val = (ir_value_t) { .kind = IR_VALUE_VAR, .var = temp };

            // Generate a temp variable to store the result.
            ir_var_t temp2 = temp_var(context, ir_result_type);

            // Add/sub the operands
            is_addition ? ir_build_add(context->builder, pointer_operand.value, temp_val, temp2)
                        : ir_build_sub(context->builder, pointer_operand.value, temp_val, temp2);

            result = ir_value_for_var(temp2);
        }


        return (expression_result_t) {
            .kind = EXPR_RESULT_VALUE,
            .c_type = result_type,
            .is_lvalue = false,
            .is_string_literal = false,
            .addr_of = false,
            .value = result,
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

expression_result_t ir_visit_multiplicative_binexpr(ir_gen_context_t *context, const expression_t *expr) {
    bool is_modulo = expr->binary.operator->kind == TK_PERCENT;
    bool is_division = expr->binary.operator->kind == TK_SLASH;

    // Evaluate the left and right operands.
    expression_result_t left = ir_visit_expression(context, expr->binary.left);
    expression_result_t right = ir_visit_expression(context, expr->binary.right);

    // Bubble up errors if the operands are invalid.
    if (left.kind == EXPR_RESULT_ERR || right.kind == EXPR_RESULT_ERR) return EXPR_ERR;

    if (left.is_lvalue) left = get_rvalue(context, left);
    if (right.is_lvalue) right = get_rvalue(context, right);

    // For multiplication/division both operands must have arithmetic type
    // For modulo both operands must have integer type
    if ((is_modulo && (!is_integer_type(left.c_type) || !is_integer_type(right.c_type))) ||
        (!is_modulo && (!is_arithmetic_type(left.c_type) || !is_arithmetic_type(right.c_type)))
    ) {
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

    // Type conversions
    const type_t *result_type = get_common_type(left.c_type, right.c_type);
    const ir_type_t *ir_result_type = get_ir_type(context,result_type);

    left = convert_to_type(context, left.value, left.c_type, result_type);
    right = convert_to_type(context, right.value, right.c_type, result_type);

    ir_value_t result;

    if (left.value.kind == IR_VALUE_CONST && right.value.kind == IR_VALUE_CONST) {
        // constant folding
        ir_const_t value = (ir_const_t) {
            .kind = is_floating_type(result_type) ? IR_CONST_FLOAT : IR_CONST_INT,
            .type = ir_result_type,
            .i = 0,
        };

        if (ir_is_integer_type(ir_result_type)) {
            // TODO: emit warning and set undefined value for division by zero
            // For now we will just set the value to 0 and move on
            if (is_division && right.value.constant.i == 0) {
                value.i = 0;
            } else {
                if (is_modulo) value.i = left.value.constant.i % right.value.constant.i;
                else if (is_division) value.i = left.value.constant.i / right.value.constant.i;
                else value.i = left.value.constant.i * right.value.constant.i;
            }
        } else {
            // no modulo operator for floating point
            if (is_division) value.f = left.value.constant.f / right.value.constant.f;
            else value.f = left.value.constant.f * right.value.constant.f;
        }

        result = (ir_value_t) {
            .kind = IR_VALUE_CONST,
            .constant = value,
        };
    } else {
        ir_var_t temp = temp_var(context, ir_result_type);
        if (is_modulo) {
            ir_build_mod(context->builder, left.value, right.value, temp);
        } else if (is_division) {
            ir_build_div(context->builder, left.value, right.value, temp);
        } else {
            ir_build_mul(context->builder, left.value, right.value, temp);
        }
        result = ir_value_for_var(temp);
    }

    return (expression_result_t) {
        .kind = EXPR_RESULT_VALUE,
        .c_type = result_type,
        .is_lvalue = false,
        .is_string_literal = false,
        .addr_of = false,
        .value = result,
    };
}

expression_result_t ir_visit_bitwise_binexpr(ir_gen_context_t *context, const expression_t *expr) {
    // Evaluate the left and right operands.
    expression_result_t left = ir_visit_expression(context, expr->binary.left);
    expression_result_t right = ir_visit_expression(context, expr->binary.right);

    // Bubble up errors if the operands are invalid.
    if (left.kind == EXPR_RESULT_ERR || right.kind == EXPR_RESULT_ERR) return EXPR_ERR;

    if (left.is_lvalue) left = get_rvalue(context, left);
    if (right.is_lvalue) right = get_rvalue(context, right);

    bool is_shift = expr->binary.operator->kind == TK_LSHIFT
                 || expr->binary.operator->kind == TK_RSHIFT;
    bool is_and = expr->binary.operator->kind == TK_AMPERSAND;
    bool is_or = expr->binary.operator->kind == TK_BITWISE_OR;

    // For bitwise operators, both operands must have integer type
    if (!is_integer_type(left.c_type) || !is_integer_type(right.c_type)) {
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

    const type_t *common_type = get_common_type(left.c_type, right.c_type);
    const ir_type_t *result_type = get_ir_type(context,common_type);

    left = convert_to_type(context, left.value, left.c_type, common_type);
    right = convert_to_type(context, right.value, right.c_type, common_type);

    ir_value_t result;

    if (left.value.kind == IR_VALUE_CONST && right.value.kind == IR_VALUE_CONST) {
        // constant folding
        ir_const_t value = {
            .kind = IR_CONST_INT,
            .type = result_type,
            .i = 0,
        };

        if (expr->binary.operator->kind == TK_LSHIFT) value.i = left.value.constant.i << right.value.constant.i;
        else if (expr->binary.operator->kind == TK_RSHIFT) value.i = left.value.constant.i >> right.value.constant.i;
        else if (is_and) value.i = left.value.constant.i & right.value.constant.i;
        else if (is_or) value.i = left.value.constant.i | right.value.constant.i;
        else value.i = left.value.constant.i ^ right.value.constant.i;

        result = (ir_value_t) {
            .kind = IR_VALUE_CONST,
            .constant = value,
        };
    } else {
        ir_var_t temp = temp_var(context, result_type);

        if (is_shift) {
            if (expr->binary.operator->kind == TK_LSHIFT) {
                ir_build_shl(context->builder, left.value, right.value, temp);
            } else {
                ir_build_shr(context->builder, left.value, right.value, temp);
            }
        } else if (is_and) {
            ir_build_and(context->builder, left.value, right.value, temp);
        } else if (is_or) {
            ir_build_or(context->builder, left.value, right.value, temp);
        } else {
            ir_build_xor(context->builder, left.value, right.value, temp);
        }

        result = ir_value_for_var(temp);
    }

    return (expression_result_t) {
        .kind = EXPR_RESULT_VALUE,
        .c_type = common_type,
        .is_lvalue = false,
        .is_string_literal = false,
        .addr_of = false,
        .value = result,
    };
}

expression_result_t ir_visit_assignment_binexpr(ir_gen_context_t *context, const expression_t *expr) {
    assert(context != NULL && "Context must not be NULL");
    assert(expr != NULL && "Expression must not be NULL");

    // Evaluate the left and right operands.
    expression_result_t left = ir_visit_expression(context, expr->binary.left);
    expression_result_t right = ir_visit_expression(context, expr->binary.right);

    // Bubble up errors if the operands are invalid.
    if (left.kind == EXPR_RESULT_ERR || right.kind == EXPR_RESULT_ERR) return EXPR_ERR;

    // The left operand must be a lvalue.
    if (!left.is_lvalue || left.c_type->is_const) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_ASSIGNMENT_TARGET,
            .location = expr->binary.operator->position,
        });
        return EXPR_ERR;
    }

    if (right.is_lvalue) right = get_rvalue(context, right);

    if (expr->binary.operator->kind != TK_ASSIGN) {
        // TODO
        assert(false && "Compound assignment not implemented");
    }

    // Generate an assignment instruction.
    ir_var_t result = (ir_var_t) {
        .name = temp_name(context),
        .type = get_ir_type(context,left.c_type),
    };

    if (!types_equal(left.c_type, right.c_type)) {
        // Convert the right operand to the type of the left operand.
        right = convert_to_type(context, right.value, right.c_type, left.c_type);
        if (right.c_type == NULL) return EXPR_ERR;
    }

    ir_build_assign(context->builder, right.value, result);

    ir_value_t ptr;
    if (left.kind == EXPR_RESULT_VALUE) {
        ptr = left.value;
    } else if (left.kind == EXPR_RESULT_INDIRECTION) {
        ptr = get_indirect_ptr(context, left);
    } else {
        return EXPR_ERR;
    }

    ir_build_store(context->builder, ptr, ir_value_for_var(result));

    // assignments can be chained, e.g. `a = b = c;`
    return left;
}

expression_result_t ir_visit_comparison_binexpr(ir_gen_context_t *context, const expression_t *expr) {
    assert(context != NULL && "Context must not be NULL");
    assert(expr != NULL && "Expression must not be NULL");
    assert(expr->type == EXPRESSION_BINARY && expr->binary.type == BINARY_COMPARISON);

    // Evaluate the left and right operands.
    expression_result_t left = ir_visit_expression(context, expr->binary.left);
    expression_result_t right = ir_visit_expression(context, expr->binary.right);

    // Bubble up errors if the operands are invalid.
    if (left.kind == EXPR_RESULT_ERR || right.kind == EXPR_RESULT_ERR) return EXPR_ERR;

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
        left = convert_to_type(context, left.value, left.c_type, common_type);
        right = convert_to_type(context, right.value, right.c_type, common_type);

        if (left.kind == EXPR_RESULT_ERR || right.kind == EXPR_RESULT_ERR) return EXPR_ERR;

        ir_value_t result;
        const binary_comparison_operator_t op = expr->binary.comparison_operator;

        if (left.value.kind == IR_VALUE_CONST && right.value.kind == IR_VALUE_CONST) {
            // constant folding
            ir_const_t value = {
                .kind = IR_CONST_INT,
                .type = &IR_BOOL,
                .i = 0,
            };
            bool floating = is_floating_type(common_type);
            long double leftf;
            long double rightf;
            long long lefti;
            long long righti;
            if (floating) {
                leftf = left.value.constant.kind == IR_CONST_INT ? left.value.constant.i : left.value.constant.f;
                rightf = right.value.constant.kind == IR_CONST_INT ? right.value.constant.i : right.value.constant.f;
            } else {
                lefti = left.value.constant.kind == IR_CONST_INT ? left.value.constant.i : left.value.constant.f;
                righti = right.value.constant.kind == IR_CONST_INT ? right.value.constant.i : right.value.constant.f;
            }
            switch (op) {
                case BINARY_COMPARISON_EQUAL:
                    value.i = floating ? leftf == rightf : lefti == righti;
                    break;
                case BINARY_COMPARISON_NOT_EQUAL:
                    value.i = floating ? leftf != rightf : lefti != righti;
                    break;
                case BINARY_COMPARISON_LESS_THAN:
                    value.i = floating ? leftf < rightf : lefti < righti;
                    break;
                case BINARY_COMPARISON_LESS_THAN_OR_EQUAL:
                    value.i = floating ? leftf <= rightf : lefti <= righti;
                    break;
                case BINARY_COMPARISON_GREATER_THAN:
                    value.i = floating ? leftf > rightf : lefti > righti;
                    break;
                case BINARY_COMPARISON_GREATER_THAN_OR_EQUAL:
                    value.i = floating ? leftf >= rightf : lefti >= righti;
                    break;
                default:
                    // Invalid comparison operator
                    // Should be unreachable
                    assert(false && "Invalid comparison operator");
            }
            result = (ir_value_t) {
                .kind = IR_VALUE_CONST,
                .constant = value,
            };
        } else {
            ir_var_t temp = temp_var(context, &IR_BOOL);
            switch (op) {
                case BINARY_COMPARISON_EQUAL:
                    ir_build_eq(context->builder, left.value, right.value, temp);
                    break;
                case BINARY_COMPARISON_NOT_EQUAL:
                    ir_build_ne(context->builder, left.value, right.value, temp);
                    break;
                case BINARY_COMPARISON_LESS_THAN:
                    ir_build_lt(context->builder, left.value, right.value, temp);
                    break;
                case BINARY_COMPARISON_LESS_THAN_OR_EQUAL:
                    ir_build_le(context->builder, left.value, right.value, temp);
                    break;
                case BINARY_COMPARISON_GREATER_THAN:
                    ir_build_gt(context->builder, left.value, right.value, temp);
                    break;
                case BINARY_COMPARISON_GREATER_THAN_OR_EQUAL:
                    ir_build_ge(context->builder, left.value, right.value, temp);
                    break;
                default:
                    // Invalid comparison operator
                    // Should be unreachable
                    assert(false && "Invalid comparison operator");
            }
            result = ir_value_for_var(temp);
        }

        return (expression_result_t) {
            .kind = EXPR_RESULT_VALUE,
            .c_type = &BOOL,
            .is_lvalue = false,
            .is_string_literal = false,
            .addr_of = false,
            .value = result,
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

expression_result_t ir_visit_logical_expression(ir_gen_context_t *context, const expression_t *expr) {
    assert(context != NULL && "Context must not be NULL");
    assert(expr != NULL && "Expression must not be NULL");
    assert(expr->type == EXPRESSION_BINARY && expr->binary.type == BINARY_LOGICAL);

    // Whether the operator is logical AND ('&&') or logical OR ('||')
    bool is_logical_and = expr->binary.logical_operator == BINARY_LOGICAL_AND;
    bool is_logical_or = !is_logical_and;

    // Evaluate the left operand
    // The logical && and || operators are short-circuiting, so if the left operand is false (for &&) or true (for ||),
    // then the right operand is not evaluated.
    expression_result_t left = ir_visit_expression(context, expr->binary.left);
    if (left.kind == EXPR_RESULT_ERR) return EXPR_ERR;
    if (left.is_lvalue) left = get_rvalue(context, left);

    // Both operands must have scalar type
    if (!is_scalar_type(left.c_type)) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_LOGICAL_BINARY_EXPRESSION_OPERAND_TYPE,
            .location = expr->binary.left->span.start,
            .invalid_logical_binary_expression_operand_type = {
                .type = left.c_type,
            },
        });
        return EXPR_ERR;
    }

    // Convert the left operand to a boolean value (if it is not already)
    // We already know that the left operand is a scalar type, so we don't need to check for errors since its a
    // valid conversion.
    ir_value_t left_bool = get_boolean_value(context, left.value, left.c_type, expr->binary.left).value;
    if (left_bool.kind == IR_VALUE_CONST) {
        // constant folding
        if ((is_logical_and && left_bool.constant.i == 0) || (is_logical_or && left_bool.constant.i != 0)) {
            // result is the value of the left operand (false for and, true for or)
            return (expression_result_t) {
                .kind = EXPR_RESULT_VALUE,
                .c_type = &BOOL,
                .is_lvalue = false,
                .is_string_literal = false,
                .addr_of = false,
                .value = left_bool,
            };
        } else {
            // result is the value of the right operand
            expression_result_t right = ir_visit_expression(context, expr->binary.right);
            if (right.kind == EXPR_RESULT_ERR) return EXPR_ERR;
            if (right.is_lvalue) right = get_rvalue(context, right);
            if (!is_scalar_type(right.c_type)) {
                append_compilation_error(&context->errors, (compilation_error_t) {
                    .kind = ERR_INVALID_LOGICAL_BINARY_EXPRESSION_OPERAND_TYPE,
                    .location = expr->binary.right->span.start,
                    .invalid_logical_binary_expression_operand_type = {
                        .type = right.c_type,
                    },
                });
                return EXPR_ERR;
            }
            ir_value_t right_bool = get_boolean_value(context, right.value, right.c_type, expr->binary.right).value;
            return (expression_result_t) {
                .kind = EXPR_RESULT_VALUE,
                .c_type = &BOOL,
                .is_lvalue = false,
                .is_string_literal = false,
                .addr_of = false,
                .value = right_bool,
            };
        }
    }

    // && - if the left operand is false, the result is false, otherwise the result is the value of the right operand
    // || - if the left operand is true, the result is true, otherwise the result is the value of the right operand
    ir_var_t result = temp_var(context, &IR_BOOL);
    ir_build_assign(context->builder, left_bool, result);
    const char* merge_label = gen_label(context);
    if (is_logical_and) {
        // if the left operand is false, the result is false
        // otherwise the result is the value of the right operand
        ir_var_t cond = temp_var(context, &IR_BOOL);
        ir_build_not(context->builder, left_bool, cond);
        ir_build_br_cond(context->builder, ir_value_for_var(cond), merge_label);
    } else {
        // if the left operand is true, the result is true
        // otherwise the result is the value of the right operand
        ir_build_br_cond(context->builder, left_bool, merge_label);
    }

    // Evaluate the right operand
    expression_result_t right = ir_visit_expression(context, expr->binary.right);
    if (right.kind == EXPR_RESULT_ERR) return EXPR_ERR;
    if (right.is_lvalue) right = get_rvalue(context, right);

    // Both operands must have scalar type
    if (!is_scalar_type(right.c_type)) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_LOGICAL_BINARY_EXPRESSION_OPERAND_TYPE,
            .location = expr->binary.left->span.start,
            .invalid_logical_binary_expression_operand_type = {
                .type = right.c_type,
            },
        });
        return EXPR_ERR;
    }

    // Convert the right operand to a boolean value (if it is not already)
    ir_value_t right_bool = right.value;
    if  (ir_get_type_of_value(right_bool)->kind != IR_TYPE_BOOL) {
        ir_var_t temp = temp_var(context, &IR_BOOL);
        ir_build_ne(context->builder, right.value, ir_get_zero_value(context, ir_get_type_of_value(right_bool)), temp);
        right_bool = ir_value_for_var(temp);
    }
    ir_build_assign(context->builder, right_bool, result);
    ir_build_nop(context->builder, merge_label);

    return (expression_result_t) {
        .kind = EXPR_RESULT_VALUE,
        .c_type = &BOOL,
        .is_lvalue = false,
        .is_string_literal = false,
        .addr_of = false,
        .value = ir_value_for_var(result),
    };
}

expression_result_t ir_visit_ternary_expression(ir_gen_context_t *context, const expression_t *expr) {
    assert(context != NULL && "Context must not be NULL");
    assert(expr != NULL && "Expression must not be NULL");
    assert(expr->type == EXPRESSION_TERNARY);

    expression_result_t condition = ir_visit_expression(context, expr->ternary.condition);
    if (condition.kind == EXPR_RESULT_ERR) return EXPR_ERR;
    if (condition.is_lvalue) condition = get_rvalue(context, condition);

    // The condition must have scalar type
    if (!is_scalar_type(condition.c_type)) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_TERNARY_CONDITION_TYPE,
            .location = expr->ternary.condition->span.start,
            .invalid_ternary_condition_type = {
                .type = condition.c_type,
            },
        });
        return EXPR_ERR;
    }

    const char* true_label = gen_label(context);
    const char* merge_label = gen_label(context);

    // Get the boolean value of the condition
    ir_value_t ir_condition = get_boolean_value(context, condition.value, condition.c_type, expr->ternary.condition).value;

    expression_result_t true_result;
    expression_result_t false_result;

    ir_instruction_node_t *true_branch_end = NULL;
    ir_instruction_node_t *false_branch_end = NULL;

    if (ir_condition.kind == IR_VALUE_CONST) {
        // Constant folding
        // Even though one of the branches will not be evaluated, we still need to visit it to perform semantic analysis
        // and to decide the type of the result. We will just throw away the generated code afterwards.

        if (ir_condition.constant.i != 0) {
            // Evaluate the true branch
            true_result = ir_visit_expression(context, expr->ternary.true_expression);
            if (true_result.kind == EXPR_RESULT_ERR) return EXPR_ERR;
            // Throw away the code for the false branch
            ir_instruction_node_t *position = ir_builder_get_position(context->builder);
            false_result = ir_visit_expression(context, expr->ternary.false_expression);
            ir_builder_clear_after(context->builder, position);
        } else {
            // Evaluate the false branch
            false_result = ir_visit_expression(context, expr->ternary.false_expression);
            if (false_result.kind == EXPR_RESULT_ERR) return EXPR_ERR;
            // Throw away the code for the true branch
            ir_instruction_node_t *position = ir_builder_get_position(context->builder);
            true_result = ir_visit_expression(context, expr->ternary.true_expression);
            ir_builder_clear_after(context->builder, position);
        }
    } else {
        // Branch based on the condition, falls through to the false branch
        ir_build_br_cond(context->builder, ir_condition, true_label);

        // False branch
        false_result = ir_visit_expression(context, expr->ternary.false_expression);
        if (false_result.kind == EXPR_RESULT_ERR) return EXPR_ERR;
        if (false_result.is_lvalue) false_result = get_rvalue(context, false_result);
        false_branch_end = ir_builder_get_position(context->builder);

        // True branch
        ir_build_nop(context->builder, true_label);
        true_result = ir_visit_expression(context, expr->ternary.true_expression);
        if (true_result.kind == EXPR_RESULT_ERR) return EXPR_ERR;
        if (true_result.is_lvalue) true_result = get_rvalue(context, true_result);
        true_branch_end = ir_builder_get_position(context->builder);
    }

    // One of the following must be true of the true and false operands:
    // 1. both have arithmetic type
    // 2. both have the same structure or union type (TODO)
    // 3. both operands have void type
    // 4. both operands are pointers to compatible types
    // 5. one operand is a pointer and the other is a null pointer constant
    // 6. one operand is a pointer to void, and the other is a pointer

    // This is a bit awkward, because we don't know the expected result type until after
    // generating code for the true and false branches.
    // After we know the result type, we have to generate conversion code (if necessary), then add an
    // assignment to the result variable in both branches (the IR is not in SSA form, so no phi node/block arguments).

    const type_t *result_type;
    const ir_type_t *ir_result_type;

    if (is_arithmetic_type(true_result.c_type) && is_arithmetic_type(false_result.c_type)) {
        result_type = get_common_type(true_result.c_type, false_result.c_type);
        ir_result_type = get_ir_type(context,result_type);
    } else if (true_result.c_type->kind == TYPE_VOID && false_result.c_type->kind == TYPE_VOID) {
        return (expression_result_t) {
            .kind = EXPR_RESULT_VALUE,
            .c_type = &VOID,
            .is_lvalue = false,
            .is_string_literal = false,
            .value = (ir_value_t) {
                .kind = IR_VALUE_CONST,
                .constant = (ir_const_t) {
                    .kind = IR_CONST_INT,
                    .type = &IR_VOID,
                    .i = 0,
                },
            }
        };
    } else if (is_pointer_type(true_result.c_type) && is_pointer_type(false_result.c_type)) {
        // TODO: pointer compatibility checks
        // For now, we will just use the type of the first non void* pointer branch
        result_type = true_result.c_type->pointer.base->kind == TYPE_VOID ? false_result.c_type : true_result.c_type;
        ir_result_type = get_ir_type(context,result_type);
    } else {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_TERNARY_EXPRESSION_OPERANDS,
            .location = expr->ternary.condition->span.start, // TODO: use the '?' token position
            .invalid_ternary_expression_operands = {
                .true_type = true_result.c_type,
                .false_type = false_result.c_type,
            },
        });
        return EXPR_ERR;
    }

    if (ir_condition.kind == IR_VALUE_CONST) {
        // Constant folding
        expression_result_t result_expr;
        return ir_condition.constant.i != 0
            ? convert_to_type(context, true_result.value, true_result.c_type, result_type)
            : convert_to_type(context, false_result.value, false_result.c_type, result_type);
    }

    ir_var_t result = temp_var(context, ir_result_type);

    ir_builder_position_after(context->builder, false_branch_end);
    if (!types_equal(false_result.c_type, result_type)) {
        false_result = convert_to_type(context, false_result.value, false_result.c_type, result_type);
        ir_build_assign(context->builder, false_result.value, result);
    } else {
        ir_build_assign(context->builder, false_result.value, result);
    }
    ir_build_br(context->builder, merge_label);

    ir_builder_position_after(context->builder, true_branch_end);
    if (!types_equal(true_result.c_type, result_type)) {
        true_result = convert_to_type(context, true_result.value, true_result.c_type, result_type);
        ir_build_assign(context->builder, true_result.value, result);
    } else {
        ir_build_assign(context->builder, true_result.value, result);
    }

    // Merge block
    ir_build_nop(context->builder, merge_label);

    return (expression_result_t) {
        .kind = EXPR_RESULT_VALUE,
        .c_type = result_type,
        .is_lvalue = false,
        .is_string_literal = false,
        .addr_of = false,
        .value = ir_value_for_var(result),
    };
}

expression_result_t ir_visit_unary_expression(ir_gen_context_t *context, const expression_t *expr) {
    assert(context != NULL && "Context must not be NULL");
    assert(expr != NULL && "Expression must not be NULL");
    assert(expr->type == EXPRESSION_UNARY);

    switch (expr->unary.operator) {
        case UNARY_BITWISE_NOT:
            return ir_visit_bitwise_not_unexpr(context, expr);
        case UNARY_ADDRESS_OF:
            return ir_visit_address_of_unexpr(context, expr);
        case UNARY_DEREFERENCE:
            return ir_visit_indirection_unexpr(context, expr);
        default:
            // TODO
            assert(false && "Unary operator not implemented");
    }
}

expression_result_t ir_visit_bitwise_not_unexpr(ir_gen_context_t *context, const expression_t *expr) {
    assert(context != NULL && "Context must not be NULL");
    assert(expr != NULL && "Expression must not be NULL");
    assert(expr->type == EXPRESSION_UNARY);

    expression_result_t operand = ir_visit_expression(context, expr->unary.operand);
    if (operand.kind == EXPR_RESULT_ERR) return EXPR_ERR;
    if (operand.is_lvalue) operand = get_rvalue(context, operand);

    if (!is_integer_type(operand.c_type)) {
        // The operand must have integer type
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_UNARY_NOT_OPERAND_TYPE,
            .location = expr->unary.operand->span.start,
            .invalid_unary_not_operand_type = {
                .type = operand.c_type,
            },
        });
        return EXPR_ERR;
    }

    if (operand.value.kind == IR_VALUE_CONST) {
        // Constant folding
        ir_value_t result = {
            .kind = IR_VALUE_CONST,
            .constant = {
                .kind = IR_CONST_INT,
                .type = ir_get_type_of_value(operand.value),
                .i = ~operand.value.constant.i,
            },
        };
        return (expression_result_t) {
            .kind = EXPR_RESULT_VALUE,
            .c_type = operand.c_type,
            .is_lvalue = false,
            .is_string_literal = false,
            .addr_of = false,
            .value = result,
        };
    }

    ir_var_t result = temp_var(context, ir_get_type_of_value(operand.value));
    ir_build_not(context->builder, operand.value, result);

    return (expression_result_t) {
        .kind = EXPR_RESULT_VALUE,
        .c_type = operand.c_type,
        .is_lvalue = false,
        .is_string_literal = false,
        .addr_of = false,
        .value = ir_value_for_var(result),
    };
}

expression_result_t ir_visit_address_of_unexpr(ir_gen_context_t *context, const expression_t *expr) {
    // The operand of the unary address of ('&') operator must be one of:
    // 1. A function designator
    // 2. The result of a [] or * operator
    // 3. A lvalue that designates an object that is not a bit-field and does not have the 'register' storage-class specifier

    expression_result_t operand = ir_visit_expression(context, expr->unary.operand);
    if (operand.kind == EXPR_RESULT_ERR) return EXPR_ERR;

    if (operand.is_lvalue) {

        return (expression_result_t) {
            .kind = EXPR_RESULT_VALUE,
            .value = operand.value,
            .c_type = operand.c_type,
            .is_lvalue = false,
            .is_string_literal = false,
            .addr_of = true,
        };
    } else {
        // TODO: handle result of [] or * operator, function designator
        assert(false && "Unimplemented");
    }
}

expression_result_t ir_visit_indirection_unexpr(ir_gen_context_t *context, const expression_t *expr) {
    expression_result_t operand = ir_visit_expression(context, expr->unary.operand);
    if (operand.kind == EXPR_RESULT_ERR) return EXPR_ERR;

    // The operand must be a pointer.
    if (!is_pointer_type(operand.c_type)) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_UNARY_INDIRECTION_OPERAND_NOT_PTR_TYPE,
            .location = expr->span.start,
        });
        return EXPR_ERR;
    }

    // If the operand points to a function, the result is a function designator.
    // Otherwise, the result is a lvalue designating the object or function designated by the operand.
    if (operand.c_type->pointer.base->kind == TYPE_FUNCTION) {
        // TODO: dereference function pointers
        assert(false && "De-referencing function pointers not implemented");
    } else {
        expression_result_t *inner = malloc(sizeof(expression_result_t));
        *inner = operand;

        return (expression_result_t) {
            .kind = EXPR_RESULT_INDIRECTION,
            .c_type = operand.c_type->pointer.base,
            .is_lvalue = true,
            .is_string_literal = false,
            .addr_of = false,
            .indirection_inner = inner,
        };
    }
}

expression_result_t ir_visit_primary_expression(ir_gen_context_t *context, const expression_t *expr) {
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
                .kind = EXPR_RESULT_VALUE,
                .c_type = symbol->c_type,
                .is_lvalue = true,
                .is_string_literal = false,
                .addr_of = false,
                .value = (ir_value_t) {
                    .kind = IR_VALUE_VAR,
                    .var = symbol->ir_ptr,
                },
            };
        }
        case PE_CONSTANT: {
            return ir_visit_constant(context, expr);
        }
        case PE_STRING_LITERAL: {
            // String literal semantics:
            // - A string literal is an array of characters with static storage duration.
            // - Whether identical string literals are distinct or share a single storage location
            //   is implementation-defined.
            // - Modifying a string literal results in undefined behavior.

            // First we need to replace escape sequences in the string literal
            char *literal = replace_escape_sequences(expr->primary.token.value);

            // Maybe there should be a special expression node type for static lengths?
            expression_t *array_length_expr = malloc(sizeof(expression_t));
            *array_length_expr = (expression_t) {
                .type = EXPRESSION_PRIMARY,
                .primary = {
                    .type = PE_CONSTANT,
                    .token = {
                        .kind = TK_INTEGER_CONSTANT,
                        .value = malloc(32),
                        .position = expr->primary.token.position,
                    },
                },
            };
            snprintf(array_length_expr->primary.token.value, 32, "%zu", strlen(literal) + 1);

            // The C type is an array of characters
            type_t *c_type = malloc(sizeof(type_t));
            *c_type = (type_t) {
                .kind = TYPE_ARRAY,
                .array = {
                    .element_type = &CHAR,
                    .size = array_length_expr,
                },
            };

            ir_type_t *ir_type = malloc(sizeof(ir_type_t));
            *ir_type = (ir_type_t) {
                .kind = IR_TYPE_ARRAY,
                .array = {
                    .element = &IR_I8,
                    .length = strlen(literal) + 1,
                },
            };

            ir_global_t *global = malloc(sizeof(ir_global_t));
            *global = (ir_global_t) {
                .name = global_name(context),
                .type = get_ir_ptr_type(ir_type),
                .initialized = true,
                .value = (ir_const_t) {
                    .type = ir_type,
                    .kind = IR_CONST_STRING,
                    .s = literal,
                },
            };
            ir_append_global_ptr(&context->module->globals, global);

            return (expression_result_t) {
                .kind = EXPR_RESULT_VALUE,
                .c_type = c_type,
                .is_lvalue = false,
                .is_string_literal = true,
                .addr_of = false,
                .value = ir_value_for_var((ir_var_t) {
                    .type = get_ir_ptr_type(ir_type),
                    .name = global->name,
                })
            };
        }
        case PE_EXPRESSION: {
            return ir_visit_expression(context, expr->primary.expression);
        }
        default: {
            // Unreachable
            fprintf(stderr, "Invalid primary expression\n");
            exit(1);
        }
    }
}

expression_result_t ir_visit_constant(ir_gen_context_t *context, const expression_t *expr) {
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
                .kind = EXPR_RESULT_VALUE,
                .c_type = &INT,
                .is_lvalue = false,
                .is_string_literal = false,
                .addr_of = false,
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
                .kind = EXPR_RESULT_VALUE,
                .c_type = c_type,
                .is_lvalue = false,
                .addr_of = false,
                .is_string_literal = false,
                .value = (ir_value_t) {
                    .kind = IR_VALUE_CONST,
                    .constant = (ir_const_t) {
                            .kind = IR_CONST_INT,
                            .type = get_ir_type(context,c_type),
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
                .kind = EXPR_RESULT_VALUE,
                .c_type = c_type,
                .is_lvalue = false,
                .is_string_literal = false,
                .addr_of = false,
                .value = (ir_value_t) {
                    .kind = IR_VALUE_CONST,
                    .constant = (ir_const_t) {
                        .kind = IR_CONST_FLOAT,
                        .type = get_ir_type(context,c_type),
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

char *global_name(ir_gen_context_t *context) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "@%d", context->global_id_counter++);
    return strdup(buffer);
}

char *temp_name(ir_gen_context_t *context) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%%%d", context->local_id_counter++);
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

const ir_type_t* get_ir_type(ir_gen_context_t *context, const type_t *c_type) {
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
            const ir_type_t *pointee = get_ir_type(context,c_type->pointer.base);
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
            const ir_type_t *ir_return_type = get_ir_type(context,c_type->function.return_type);
            const ir_type_t **ir_param_types = malloc(c_type->function.parameter_list->length * sizeof(ir_type_t*));
            for (size_t i = 0; i < c_type->function.parameter_list->length; i++) {
                const parameter_declaration_t *param = c_type->function.parameter_list->parameters[i];
                ir_param_types[i] = get_ir_type(context,param->type);
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
            const ir_type_t *element_type = get_ir_type(context,c_type->array.element_type);
            expression_result_t array_len = ir_visit_expression(context, c_type->array.size);
            if (array_len.kind == EXPR_RESULT_ERR) assert(false && "Invalid array size"); // TODO: handle error
            if (array_len.is_lvalue) array_len = get_rvalue(context, array_len);
            ir_value_t length = array_len.value;
            if (length.kind != IR_VALUE_CONST) {
                // TODO: handle non-constant array sizes
                assert(false && "Non-constant array sizes not implemented");
            }
            ir_type_t *ir_type = malloc(sizeof(ir_type_t));
            *ir_type = (ir_type_t) {
                .kind = IR_TYPE_ARRAY,
                .array = {
                    .element = element_type,
                    .length = length.constant.i,
                }
            };
            return ir_type;
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

ir_value_t ir_get_zero_value(ir_gen_context_t *context, const ir_type_t *type) {
    if (ir_is_integer_type(type)) {
        return (ir_value_t) {
            .kind = IR_VALUE_CONST,
            .constant = (ir_const_t) {
                .kind = IR_CONST_INT,
                .type = type,
                .i = 0,
            }
        };
    } else if (ir_is_float_type(type)) {
        return (ir_value_t) {
            .kind = IR_VALUE_CONST,
            .constant = (ir_const_t) {
                .kind = IR_CONST_FLOAT,
                .type = type,
                .f = 0.0,
            }
        };
    } else if (type->kind == IR_TYPE_PTR) {
        ir_value_t zero = ir_get_zero_value(context, get_ir_type(context,c_ptr_int_type()));
        ir_var_t result = temp_var(context, type);
        ir_build_ptoi(context->builder, zero, result);
        return ir_value_for_var(result);
    } else {
        // TODO: struct, arrays, enums, etc...
        fprintf(stderr, "Unimplemented default value for type %s\n", ir_fmt_type(alloca(256), 256, type));
        exit(1);
    }
}

expression_result_t get_boolean_value(
    ir_gen_context_t *context,
    ir_value_t value,
    const type_t *c_type,
    const expression_t *expr
) {
    const ir_type_t *ir_type = ir_get_type_of_value(value);
    if (ir_type->kind == IR_TYPE_BOOL) {
        return (expression_result_t) {
            .c_type = &BOOL,
            .is_lvalue = false,
            .value = value,
        };
    }

    if (!ir_is_scalar_type(ir_type)) {
        // The value must have scalar type
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_CONVERSION_TO_BOOLEAN,
            .location = expr->span.start,
            .invalid_conversion_to_boolean = {
                .type = c_type,
            },
        });
        return EXPR_ERR;
    }

    ir_value_t result;
    if (value.kind == IR_VALUE_CONST) {
        // constant folding
        ir_const_t constant = {
            .kind = IR_CONST_INT,
            .type = &IR_BOOL,
            .i =  ir_is_float_type(ir_type) ? value.constant.f != 0.0 : value.constant.i != 0,
        };
        result = (ir_value_t) {
            .kind = IR_VALUE_CONST,
            .constant = constant,
        };
    } else {
        ir_var_t temp = temp_var(context, &IR_BOOL);
        ir_build_ne(context->builder, value, ir_get_zero_value(context, ir_type), temp);
        result = ir_value_for_var(temp);
    }

    return (expression_result_t) {
        .c_type = &BOOL,
        .is_lvalue = false,
        .value = result,
    };
}

expression_result_t convert_to_type(
        ir_gen_context_t *context,
        ir_value_t value,
        const type_t *from_type,
        const type_t *to_type
) {
    const ir_type_t *result_type = get_ir_type(context,to_type);
    const ir_type_t *source_type;
    if (value.kind == IR_VALUE_CONST) {
        source_type = value.constant.type;
    } else {
        source_type = value.var.type;
    }

    if (ir_types_equal(source_type, result_type)) {
        // No conversion necessary
        return (expression_result_t) {
            .kind = EXPR_RESULT_VALUE,
            .c_type = to_type,
            .is_lvalue = false,
            .value = value,
        };
    }

    ir_var_t result = {
        .name = temp_name(context),
        .type = result_type,
    };

    if (ir_is_integer_type(result_type)) {
        if (ir_is_integer_type(source_type)) {
            if (value.kind == IR_VALUE_CONST) {
                // constant -> constant conversion
                ir_value_t constant = (ir_value_t) {
                    .kind = IR_VALUE_CONST,
                    .constant = (ir_const_t) {
                        .kind = IR_CONST_INT,
                        .type = result_type,
                        .i = value.constant.i,
                    }
                };
                return (expression_result_t) {
                    .kind = EXPR_RESULT_VALUE,
                    .c_type = to_type,
                    .is_lvalue = false,
                    .value = constant,
                };
            }

            // int -> int conversion
            if (size_of_type_bits(source_type) > size_of_type_bits(result_type)) {
                // Truncate
                ir_build_trunc(context->builder, value, result);
            } else if (size_of_type_bits(source_type) < size_of_type_bits(result_type)) {
                // Extend
                ir_build_ext(context->builder, value, result);
            } else {
                // Sign/unsigned integer conversion
                ir_build_bitcast(context->builder, value, result);
            }
        } else if (ir_is_float_type(source_type)) {
            // constant conversion
            if (value.kind == IR_VALUE_CONST) {
                ir_value_t constant = (ir_value_t) {
                    .kind = IR_VALUE_CONST,
                    .constant = (ir_const_t) {
                        .kind = IR_CONST_INT,
                        .type = result_type,
                        .i = (long long)value.constant.f,
                    },
                };
                return (expression_result_t) {
                    .kind = EXPR_RESULT_VALUE,
                    .c_type = to_type,
                    .is_lvalue = false,
                    .value = constant,
                };
            }

            // float -> int
            ir_build_ftoi(context->builder, value, result);
        } else if (source_type->kind == IR_TYPE_PTR) {
            if (value.kind == IR_VALUE_CONST) {
                ir_value_t constant = (ir_value_t) {
                    .kind = IR_VALUE_CONST,
                    .constant = (ir_const_t) {
                        .kind = IR_CONST_INT,
                        .type = result_type,
                        .i = value.constant.i,
                    }
                };
                return (expression_result_t) {
                    .kind = EXPR_RESULT_VALUE,
                    .c_type = to_type,
                    .is_lvalue = false,
                    .value = constant,
                };
            }

            // ptr -> int
            ir_build_ptoi(context->builder, value, result);
        } else {
            // TODO, other conversions, proper error handling
            fprintf(stderr, "Unimplemented type conversion from %s to %s\n",
                    ir_fmt_type(alloca(256), 256, source_type),
                    ir_fmt_type(alloca(256), 256, result_type));
            return EXPR_ERR;
        }
    } else if (ir_is_float_type(result_type)) {
        if (ir_is_float_type(source_type)) {
            // constant conversion
            if (value.kind == IR_VALUE_CONST) {
                ir_value_t constant = (ir_value_t) {
                    .kind = IR_VALUE_CONST,
                    .constant = (ir_const_t) {
                        .kind = IR_CONST_FLOAT,
                        .type = result_type,
                        .f = value.constant.f,
                    }
                };
                return (expression_result_t) {
                    .kind = EXPR_RESULT_VALUE,
                    .c_type = to_type,
                    .is_lvalue = false,
                    .value = constant,
                };
            }

            // float -> float conversion
            if (size_of_type_bits(source_type) > size_of_type_bits(result_type)) {
                // Truncate
                ir_build_trunc(context->builder, value, result);
            } else if (size_of_type_bits(source_type) < size_of_type_bits(result_type)) {
                // Extend
                ir_build_ext(context->builder, value, result);
            } else {
                // No conversion necessary
                ir_build_assign(context->builder, value, result);
            }
        } else if (ir_is_integer_type(source_type)) {
            // constant conversion
            if (value.kind == IR_VALUE_CONST) {
                ir_value_t constant = (ir_value_t) {
                    .kind = IR_VALUE_CONST,
                    .constant = (ir_const_t) {
                        .kind = IR_CONST_FLOAT,
                        .type = result_type,
                        .f = (double)value.constant.i,
                    }
                };
                return (expression_result_t) {
                    .kind = EXPR_RESULT_VALUE,
                    .c_type = to_type,
                    .is_lvalue = false,
                    .value = constant,
                };
            }

            // int -> float
            ir_build_itof(context->builder, value, result);
        } else {
            // TODO: proper error handling
            fprintf(stderr, "Unimplemented type conversion from %s to %s\n",
                    ir_fmt_type(alloca(256), 256, source_type),
                    ir_fmt_type(alloca(256), 256, result_type));
            return EXPR_ERR;
        }
    } else if (result_type->kind == IR_TYPE_PTR) {
        if (source_type->kind == IR_TYPE_PTR) {
            // constant conversion
            if (value.kind == IR_VALUE_CONST) {
                ir_value_t constant = (ir_value_t) {
                    .kind = IR_VALUE_CONST,
                    .constant = (ir_const_t) {
                        .kind = IR_CONST_INT,
                        .type = result_type,
                        .i = value.constant.i,
                    }
                };
                return (expression_result_t) {
                    .kind = EXPR_RESULT_VALUE,
                    .c_type = to_type,
                    .is_lvalue = false,
                    .value = constant,
                };
            }

            // ptr -> ptr conversion
            ir_build_bitcast(context->builder, value, result);
        } else if (ir_is_integer_type(source_type)) {
            // constant conversion
            if (value.kind == IR_VALUE_CONST) {
                ir_value_t constant = (ir_value_t) {
                    .kind = IR_VALUE_CONST,
                    .constant = (ir_const_t) {
                        .kind = IR_CONST_INT,
                        .type = result_type,
                        .i = value.constant.i,
                    }
                };
                return (expression_result_t) {
                    .kind = EXPR_RESULT_VALUE,
                    .c_type = to_type,
                    .is_lvalue = false,
                    .value = constant,
                };
            }

            // int -> ptr
            // If the source is smaller than the target, we need to extend it
            if (size_of_type_bits(source_type) < size_of_type_bits(get_ir_type(context, c_ptr_int_type()))) {
                ir_var_t temp = temp_var(context, get_ir_type(context,c_ptr_int_type()));
                ir_build_ext(context->builder, value, temp);
                value = ir_value_for_var(temp);
            }
            ir_build_itop(context->builder, value, result);
        } else if (ir_is_float_type(source_type)) {
            // float -> ptr
            // TODO: is this allowed? Seems like it's an invalid conversion
            const ir_type_t* int_type = source_type->kind == IR_TYPE_F64 ? &IR_I64 : &IR_I32;
            ir_var_t temp = temp_var(context, int_type);
            ir_build_bitcast(context->builder, value, temp);
            ir_build_itop(context->builder, ir_value_for_var(temp), result);
        } else if (source_type->kind == IR_TYPE_ARRAY) {
            // TODO
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
        .kind = EXPR_RESULT_VALUE,
        .c_type = to_type,
        .is_lvalue = false,
        .value = ir_value_for_var(result),
    };
}

ir_value_t ir_value_for_var(ir_var_t var) {
    return (ir_value_t) {
        .kind = IR_VALUE_VAR,
        .var = var,
    };
}

ir_value_t get_indirect_ptr(ir_gen_context_t *context, expression_result_t res) {
    assert(res.kind == EXPR_RESULT_INDIRECTION && "Expected indirection expression");

    // We need to load the value from a pointer.
    // However, there may be multiple levels of indirection, each requiring a load.
    expression_result_t *e = &res;
    int indirection_level = 0;
    do {
        assert(e->indirection_inner != NULL);
        e = e->indirection_inner;
        indirection_level += 1;
    } while (e->kind == EXPR_RESULT_INDIRECTION);

    // Starting at the base pointer, repeatedly load the new pointer
    ir_value_t ptr = e->value;
    for (int i = 0; i < indirection_level; i += 1) {
        ir_var_t temp = temp_var(context, ir_get_type_of_value(ptr)->ptr.pointee);
        ir_build_load(context->builder, ptr, temp);
        ptr = ir_value_for_var(temp);
    }

    return ptr;
}

expression_result_t get_rvalue(ir_gen_context_t *context, expression_result_t res) {
    assert(res.is_lvalue && "Expected lvalue");
    if (res.kind == EXPR_RESULT_VALUE) {
        assert(ir_get_type_of_value(res.value)->kind == IR_TYPE_PTR && "Expected pointer type");
        ir_var_t temp = temp_var(context, ir_get_type_of_value(res.value)->ptr.pointee);
        ir_var_t ptr = (ir_var_t) {
            .name = res.value.var.name,
            .type = res.value.var.type,
        };
        ir_build_load(context->builder, ir_value_for_var(ptr), temp);
        return (expression_result_t) {
            .kind = EXPR_RESULT_VALUE,
            .c_type = res.c_type,
            .is_lvalue = false,
            .value = ir_value_for_var(temp),
        };
    } else if(res.kind == EXPR_RESULT_INDIRECTION) {
        ir_value_t ptr = get_indirect_ptr(context, res);

        // Then finally, load the result
        ir_var_t result = temp_var(context, ir_get_type_of_value(ptr)->ptr.pointee);
        ir_build_load(context->builder, ptr, result);
        return (expression_result_t) {
            .kind = EXPR_RESULT_VALUE,
            .c_type = res.c_type,
            .is_lvalue = false,
            .addr_of = false,
            .is_string_literal = false,
            .value = ir_value_for_var(result)
        };
    } else {
        return EXPR_ERR;
    }
}

void insert_alloca(ir_gen_context_t *context, const ir_type_t *ir_type, ir_var_t result) {
    // save the current position of the builder
    ir_instruction_node_t *position = ir_builder_get_position(context->builder);
    bool should_restore = position != NULL && position != context->alloca_tail;

    ir_builder_position_after(context->builder, context->alloca_tail);
    context->alloca_tail = ir_build_alloca(context->builder, ir_type, result);

    // restore the builder position
    if (should_restore) {
        ir_builder_position_after(context->builder, position);
    }
}

const ir_type_t *ir_ptr_int_type() {
    // TODO: arch dependent
    return &IR_I64;
}

ir_value_t ir_make_const_int(const ir_type_t *type, long long value) {
    return (ir_value_t) {
        .kind = IR_VALUE_CONST,
        .constant = {
            .kind = IR_CONST_INT,
            .type = type,
            .i = value,
        }
    };
}
