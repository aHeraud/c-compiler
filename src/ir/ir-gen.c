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
#include "parser/numeric-constants.h"
#include "errors.h"
#include "util/strings.h"

struct Scope;
struct Symbol;

VEC_DEFINE(StatementPtrVector, statement_ptr_vector_t, statement_t*)

typedef struct IrGenContext {
    ir_module_t *module;
    const ir_arch_t *arch;

    hash_table_t global_map;
    hash_table_t function_definition_map;
    hash_table_t tag_uid_map;

    // State for the current function being visited
    ir_function_definition_t *function;
    const function_definition_t *c_function;
    ir_function_builder_t *builder;
    ir_instruction_node_t *alloca_tail;
    hash_table_t label_map; // map of c label -> ir label
    hash_table_t label_exists; // set of c label that actually exist, for validating the goto statements
    statement_ptr_vector_t goto_statements; // goto statements that need to be validated at the end of the fn

    // Switch instruction node (in in a switch statement)
    ir_instruction_node_t *switch_node;
    // Break label (if in a loop/switch case statement)
    char* break_label;
    // Continue label (if in a loop)
    char* continue_label;

    // List of compilation errors encountered during semantic analysis
    compilation_error_vector_t errors;
    // The current lexical scope
    struct Scope *current_scope;
    // Counter for generating unique global variable names
    // These should be unique over the entire module
    unsigned short global_id_counter;
    // Counter for generating unique local variable names
    // These are only unique within the current function
    unsigned short local_id_counter;
    // Counter for generating unique labels
    unsigned short label_counter;
    // Counter for generating unique tag suffixes
    unsigned short tag_id_counter;
} ir_gen_context_t;

typedef enum ExpressionResultKind {
    EXPR_RESULT_ERR,
    EXPR_RESULT_VALUE,
    EXPR_RESULT_INDIRECTION,
} expression_result_kind_t;

struct Symbol;
struct ExpressionResult;
typedef struct ExpressionResult {
    expression_result_kind_t kind;
    const type_t *c_type;
    bool is_lvalue;
    bool addr_of;
    bool is_string_literal;
    // Non-null if this was the result of a primary expression which was an identifier
    const struct Symbol *symbol;
    // only 1 of these is initialized, depending on the value of kind
    ir_value_t value;
    struct ExpressionResult *indirection_inner;
} expression_result_t;

typedef struct Scope {
    hash_table_t symbols;
    hash_table_t tags; // separate namespace for struct/union/enum declarations
    struct Scope *parent;
} scope_t;

enum SymbolKind {
    SYMBOL_ENUMERATION_CONSTANT,
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
    // True if this has a constant value (e.g. constant storage class)
    bool has_const_value;
    // Constant value for this symbol, only valid if has_constant_value == true
    ir_const_t const_value;
} symbol_t;

typedef struct Tag {
    const token_t *identifier;
    const char* uid; // unique-id (module) for the tag
    const type_t *c_type;
    const ir_type_t *ir_type;
} tag_t;

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

tag_t *lookup_tag(const ir_gen_context_t *context, const char *name) {
    const scope_t *scope = context->current_scope;
    while (scope != NULL) {
        tag_t *tag = NULL;
        if (hash_table_lookup(&scope->tags, name, (void**) &tag)) return tag;
        scope = scope->parent;
    }
    return NULL;
}

tag_t *lookup_tag_in_current_scope(const ir_gen_context_t *context, const char *name) {
    tag_t *tag = NULL;
    if (hash_table_lookup(&context->current_scope->tags, name, (void**) &tag)) return tag;
    return NULL;
}

tag_t *lookup_tag_by_uid(const ir_gen_context_t *context, const char *uid) {
    tag_t *tag = NULL;
    hash_table_lookup(&context->tag_uid_map, uid, (void**) &tag);
    return tag;
}

void declare_symbol(ir_gen_context_t *context, symbol_t *symbol) {
    assert(context != NULL && symbol != NULL);
    bool inserted = hash_table_insert(&context->current_scope->symbols, symbol->identifier->value, (void*) symbol);
    assert(inserted);
}

void declare_tag(ir_gen_context_t *context, const tag_t *tag) {
    assert(context != NULL && tag != NULL);
    assert(hash_table_insert(&context->current_scope->tags, tag->identifier->value, (void*) tag));
    assert(hash_table_insert(&context->tag_uid_map, tag->uid, (void*) tag));

    // also add the type to the module
    assert(!hash_table_lookup(&context->module->type_map, tag->uid, NULL)); // should be unique
    assert(hash_table_insert(&context->module->type_map, tag->uid, (void*) tag->ir_type));
}

char *global_name(ir_gen_context_t *context);
char *temp_name(ir_gen_context_t *context);
ir_var_t temp_var(ir_gen_context_t *context, const ir_type_t *type);
char *gen_label(ir_gen_context_t *context);

ir_value_t ir_make_const_int(const ir_type_t *type, long long value);
ir_value_t ir_make_const_float(const ir_type_t *type, double value);
ir_value_t ir_get_zero_value(ir_gen_context_t *context, const ir_type_t *type);

/**
 * Helper to insert alloca instructions for local variables at the top of the function.
 */
ir_instruction_node_t *insert_alloca(ir_gen_context_t *context, const ir_type_t *ir_type, ir_var_t result);

/**
 * Get the C integer type that is the same width as a pointer.
 */
const type_t *c_ptr_uint_type(void);

/**
 * Get the IR integer type that is the same width as a pointer.
 */
const ir_type_t *ir_ptr_int_type(const ir_gen_context_t *context);

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
 * Get the IR type that corresponds to a C struct/union type.
 * @param context
 * @param c_type
 * @param id
 * @return New struct/union type
 */
const ir_type_t *get_ir_struct_type(ir_gen_context_t *context, const type_t *c_type, const char *id);

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
            .tags = hash_table_create_string_keys(256),
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

typedef struct LoopContext {
    char *break_label;
    char *continue_label;
} loop_context_t;

/**
* Enter a loop context, which will set the loop break and continue labels
* Also saves and returns the previous context
*/
loop_context_t enter_loop_context(ir_gen_context_t *context, char *break_label, char *continue_label) {
    const loop_context_t prev = {
        .break_label = context->break_label,
        .continue_label = context->continue_label,
    };
    context->break_label = break_label;
    context->continue_label = continue_label;
    return prev;
}

/**
 * Restore the previous loop context
 */
void leave_loop_context(ir_gen_context_t *context, loop_context_t prev) {
    context->break_label = prev.break_label;
    context->continue_label = prev.continue_label;
}

void ir_visit_translation_unit(ir_gen_context_t *context, const translation_unit_t *translation_unit);
void ir_visit_function(ir_gen_context_t *context, const function_definition_t *function);
void ir_visit_statement(ir_gen_context_t *context, const statement_t *statement);
void ir_visit_labeled_statement(ir_gen_context_t *context, const statement_t *statement);
void ir_visit_if_statement(ir_gen_context_t *context, const statement_t *statement);
void ir_visit_return_statement(ir_gen_context_t *context, const statement_t *statement);
void ir_visit_loop_statement(ir_gen_context_t *context, const statement_t *statement);
void ir_visit_break_statement(ir_gen_context_t *context, const statement_t *statement);
void ir_visit_continue_statement(ir_gen_context_t *context, const statement_t *statement);
void ir_visit_goto_statement(ir_gen_context_t *context, const statement_t *statement);
void ir_visit_switch_statement(ir_gen_context_t *context, const statement_t *statement);
void ir_visit_case_statement(ir_gen_context_t *context, const statement_t *statement);
void ir_visit_global_declaration(ir_gen_context_t *context, const declaration_t *declaration);
void ir_visit_declaration(ir_gen_context_t *context, const declaration_t *declaration);
expression_result_t ir_visit_expression(ir_gen_context_t *context, const expression_t *expression);
expression_result_t ir_visit_constant_expression(ir_gen_context_t *context, const expression_t *expression);
expression_result_t ir_visit_array_subscript_expression(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_member_access_expression(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_primary_expression(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_constant(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_call_expression(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_cast_expression(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_binary_expression(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_additive_binexpr(ir_gen_context_t *context, const expression_t *expr, expression_result_t lhs, expression_result_t rhs);
expression_result_t ir_visit_assignment_binexpr(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_bitwise_binexpr(ir_gen_context_t *context, const expression_t *expr, expression_result_t lhs, expression_result_t rhs);
expression_result_t ir_visit_comparison_binexpr(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_multiplicative_binexpr(ir_gen_context_t *context, const expression_t *expr, expression_result_t lhs, expression_result_t rhs);
expression_result_t ir_visit_sizeof_expression(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_ternary_expression(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_unary_expression(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_bitwise_not_unexpr(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_logical_not_unexpr(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_address_of_unexpr(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_indirection_unexpr(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_sizeof_unexpr(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_increment_decrement(ir_gen_context_t *context, const expression_t *expr, bool pre, bool incr);
expression_result_t ir_visit_logical_expression(ir_gen_context_t *context, const expression_t *expr);
expression_result_t ir_visit_compound_literal(ir_gen_context_t *context, const expression_t *expr);

ir_gen_result_t generate_ir(const translation_unit_t *translation_unit, const ir_arch_t *arch) {
    ir_gen_context_t context = {
        .module = malloc(sizeof(ir_module_t)),
        .arch = arch,
        .global_map = hash_table_create_string_keys(256),
        .function_definition_map = hash_table_create_string_keys(256),
        .tag_uid_map = hash_table_create_string_keys(256),
        .function = NULL,
        .builder = NULL,
        .errors = (compilation_error_vector_t) { .size = 0, .capacity = 0, .buffer = NULL },
        .current_scope = NULL,
        .break_label = NULL,
        .continue_label = NULL,
        .global_id_counter = 0,
        .local_id_counter = 0,
    };

    *context.module = (ir_module_t) {
        .name = "module", // TODO: get the name of the input file?
        .arch = arch,
        .functions = (ir_function_ptr_vector_t) { .size = 0, .capacity = 0, .buffer = NULL },
        .type_map = hash_table_create_string_keys(128),
        .globals = (ir_global_ptr_vector_t) { .size = 0, .capacity = 0, .buffer = NULL },
    };

    ir_visit_translation_unit(&context, translation_unit);

    // Cleanup (note that this doesn't free the pointers stored in the hash tables).
    hash_table_destroy(&context.global_map);
    hash_table_destroy(&context.function_definition_map);
    hash_table_destroy(&context.tag_uid_map);

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
        switch (external_declaration->kind) {
            case EXTERNAL_DECLARATION_FUNCTION_DEFINITION: {
                ir_visit_function(context, external_declaration->value.function_definition);
                break;
            }
            case EXTERNAL_DECLARATION_DECLARATION: {
                // A single declaration may declare multiple variables.
                for (int j = 0; j < external_declaration->value.declaration.length; j += 1) {
                    ir_visit_global_declaration(context, external_declaration->value.declaration.declarations[j]);
                }
                break;
            }
        }
    }

    leave_scope(context);
}

void ir_visit_function(ir_gen_context_t *context, const function_definition_t *function) {
    context->local_id_counter = 0;
    context->function = malloc(sizeof(ir_function_definition_t));
    *context->function = (ir_function_definition_t) {
        .name = function->identifier->value,
    };
    context->c_function = function;
    context->builder = ir_builder_create();
    context->alloca_tail = ir_builder_get_position(context->builder);
    context->label_map = hash_table_create_string_keys(64);
    context->label_exists = hash_table_create_string_keys(64);
    context->goto_statements = (statement_ptr_vector_t) VEC_INIT;

    const type_t function_c_type = {
        .kind = TYPE_FUNCTION,
        .value.function = {
            .return_type = function->return_type,
            .parameter_list = function->parameter_list,
        },
    };
    const ir_type_t *function_type = get_ir_type(context, &function_c_type);
    context->function->type = function_type;

    // Verify that the function was not previously defined with a different signature.
    symbol_t *entry = lookup_symbol(context, function->identifier->value);
    if (entry != NULL) {
        if (entry->kind != SYMBOL_FUNCTION || !ir_types_equal(entry->ir_type, function_type)) {
            // A symbol with the same name exists, but it is not a function, or the type signature doesn't match
            // what was previously declared.
            append_compilation_error(&context->errors, (compilation_error_t) {
                .kind = ERR_REDEFINITION_OF_SYMBOL,
                .location = function->identifier->position,
                .value.redefinition_of_symbol = {
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
                .value.redefinition_of_symbol = {
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
            .has_const_value = false,
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
        const type_t *c_type = param->type;
        const ir_type_t *ir_param_type = get_ir_type(context, c_type);

        // Array to pointer decay
        if (c_type->kind == TYPE_ARRAY) {
            c_type = get_ptr_type(c_type->value.array.element_type);
            ir_param_type = get_ir_ptr_type(ir_param_type->value.array.element);
        }

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
            .c_type = c_type,
            .ir_type = ir_param_type,
            .ir_ptr = param_ptr,
            .has_const_value = false,
        };
        declare_symbol(context, symbol);
    }

    ir_visit_statement(context, function->body);

    leave_scope(context);

    context->function->body = ir_builder_finalize(context->builder);

    hash_table_insert(&context->function_definition_map, function->identifier->value, context->function);
    ir_append_function_ptr(&context->module->functions, context->function);

    // Validate the goto statements
    // We deferred the validation until the end of the function body, as you can goto a label defined later in
    // the function.
    // For every goto statement, there should be an entry in the label_exists map.
    for (int i = 0; i < context->goto_statements.size; i += 1) {
        const statement_t *goto_statement = context->goto_statements.buffer[i];
        assert(goto_statement != NULL && goto_statement->kind == STATEMENT_GOTO);
        bool valid_label = hash_table_lookup(&context->label_exists, goto_statement->value.goto_.identifier->value, NULL);
        if (!valid_label) {
            append_compilation_error(&context->errors, (compilation_error_t) {
                .kind = ERR_USE_OF_UNDECLARED_LABEL,
                .location = goto_statement->value.label_.identifier->position,
                .value.use_of_undeclared_label = {
                    .label = *goto_statement->value.label_.identifier
                },
            });
        }
    }

    if (context->errors.size > 0) {
        // There were errors processing the function, skip IR validation.
        return;
    }

    // There were no semantic errors, so the generated IR should be valid.
    // Validate the IR to catch any bugs in the compiler.
    ir_validation_error_vector_t errors = ir_validate_function(context->module, context->function);
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
            if (context->function->type->value.function.return_type->kind == IR_TYPE_VOID) {
                *ret = (ir_instruction_t) {
                    .opcode = IR_RET,
                    .value.ret = {
                        .has_value = false,
                    }
                };
            } else {
                *ret = (ir_instruction_t) {
                    .opcode = IR_RET,
                    .value.ret = {
                        .has_value = true,
                        .value = ir_get_zero_value(context, context->function->type->value.function.return_type),
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

    // cleanup
    hash_table_destroy(&context->label_map);
    hash_table_destroy(&context->label_exists);
    VEC_DESTROY(&context->goto_statements);
}

void ir_visit_statement(ir_gen_context_t *context, const statement_t *statement) {
    assert(context != NULL && "Context must not be NULL");
    assert(statement != NULL && "Statement must not be NULL");

    switch (statement->kind) {
        case STATEMENT_COMPOUND: {
            enter_scope(context);
            for (size_t i = 0; i < statement->value.compound.block_items.size; i++) {
                block_item_t *block_item = (block_item_t*) statement->value.compound.block_items.buffer[i];
                switch (block_item->kind) {
                    case BLOCK_ITEM_STATEMENT: {
                        ir_visit_statement(context, block_item->value.statement);
                        break;
                    }
                    case BLOCK_ITEM_DECLARATION: {
                        ir_visit_declaration(context, block_item->value.declaration);
                    }
                }
            }
            leave_scope(context);
        }
        case STATEMENT_EMPTY:
            // no-op
            break;
        case STATEMENT_EXPRESSION:
            ir_visit_expression(context, statement->value.expression);
            break;
        case STATEMENT_IF:
            ir_visit_if_statement(context, statement);
            break;
        case STATEMENT_RETURN:
            ir_visit_return_statement(context, statement);
            break;
        case STATEMENT_WHILE:
        case STATEMENT_DO_WHILE:
        case STATEMENT_FOR:
            ir_visit_loop_statement(context, statement);
            break;
        case STATEMENT_BREAK:
            ir_visit_break_statement(context, statement);
            break;
        case STATEMENT_CONTINUE:
            ir_visit_continue_statement(context, statement);
            break;
        case STATEMENT_LABEL:
            ir_visit_labeled_statement(context, statement);
            break;
        case STATEMENT_GOTO:
            ir_visit_goto_statement(context, statement);
            break;
        case STATEMENT_SWITCH:
            ir_visit_switch_statement(context, statement);
            break;
        case STATEMENT_CASE:
            ir_visit_case_statement(context, statement);\
            break;
        default:
            fprintf(stderr, "%s:%d: Invalid statement type\n", __FILE__, __LINE__);
            exit(1);
    }
}

void ir_visit_if_statement(ir_gen_context_t *context, const statement_t *statement) {
    assert(context != NULL && "Context must not be NULL");
    assert(statement != NULL && "Statement must not be NULL");
    assert(statement->kind == STATEMENT_IF);

    // Evaluate the condition
    expression_result_t condition = ir_visit_expression(context, statement->value.if_.condition);

    if (condition.is_lvalue) {
        condition = get_rvalue(context, condition);
    }

    // The condition must have a scalar type
    if (!is_scalar_type(condition.c_type)) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_IF_CONDITION_TYPE,
            .location = statement->value.if_.keyword->position,
        });
        return;
    }

    // Create labels for the false branch and the end of the if statement
    char *false_label = NULL;
    if (statement->value.if_.false_branch != NULL) {
        false_label = gen_label(context);
    }
    char *end_label = gen_label(context);

    // Compare the condition to zero
    if (is_pointer_type(condition.c_type)) {
        // Convert to an integer type
        condition = convert_to_type(context, condition.value, condition.c_type, c_ptr_uint_type());
    }

    bool condition_is_floating = is_floating_type(condition.c_type);
    ir_value_t zero = condition_is_floating ?
          (ir_value_t) {
              .kind = IR_VALUE_CONST,
              .constant = (ir_const_t) {
                  .kind = IR_CONST_FLOAT,
                  .type = get_ir_type(context,condition.c_type),
                  .value.f = 0.0,
              },
          } :
          (ir_value_t) {
              .kind = IR_VALUE_CONST,
              .constant = (ir_const_t) {
                  .kind = IR_CONST_INT,
                  .type = get_ir_type(context,condition.c_type),
                  .value.i = 0,
              },
          };
    ir_var_t condition_var = (ir_var_t) {
        .name = temp_name(context),
        .type = &IR_BOOL,
    };
    ir_build_eq(context->builder, condition.value, zero, condition_var);
    ir_build_br_cond(context->builder, ir_value_for_var(condition_var), false_label != NULL ? false_label : end_label);

    // Generate code for the true branch
    ir_visit_statement(context, statement->value.if_.true_branch);

    if (statement->value.if_.false_branch != NULL) {
        // Jump to the end of the if statement
        ir_build_br(context->builder, end_label);

        // Label for the false branch
        ir_build_nop(context->builder, false_label);

        // Generate code for the false branch
        ir_visit_statement(context, statement->value.if_.false_branch);
    }

    ir_build_nop(context->builder, end_label);
}

void ir_visit_return_statement(ir_gen_context_t *context, const statement_t *statement) {
    assert(context != NULL && "Context must not be NULL");
    assert(statement != NULL && "Statement must not be NULL");
    assert(statement->kind == STATEMENT_RETURN);

    const ir_type_t *return_type = context->function->type->value.function.return_type;
    const type_t *c_return_type = context->c_function->return_type;

    if (statement->value.return_.expression != NULL) {
        expression_result_t value = ir_visit_expression(context, statement->value.return_.expression);
        // Error occurred while evaluating the return value
        if (value.kind == EXPR_RESULT_ERR) return;

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
            // attempting to return void from a function that returns a value
            append_compilation_error(&context->errors, (compilation_error_t) {
                .kind = ERR_NON_VOID_FUNCTION_RETURNS_VOID,
                .location = statement->value.return_.keyword->position,
                .value.non_void_function_returns_void = {
                    .ret = statement->value.return_.keyword,
                    .fn = context->c_function,
                },
            });
        }
        ir_build_ret_void(context->builder);
    }
}

void ir_visit_loop_statement(ir_gen_context_t *context, const statement_t *statement) {
    assert(context != NULL && statement != NULL);
    assert(statement->kind == STATEMENT_WHILE || statement->kind == STATEMENT_DO_WHILE || statement->kind == STATEMENT_FOR);

    bool post_test = false;
    expression_t *condition_expr = NULL;
    statement_t *body = NULL;

    char *loop_start_label = gen_label(context);
    char *loop_end_label = gen_label(context);
    char *loop_exit_label = gen_label(context);

    switch (statement->kind) {
        case STATEMENT_WHILE:
            body = statement->value.while_.body;
            condition_expr = statement->value.while_.condition;
            break;
        case STATEMENT_DO_WHILE:
            body = statement->value.do_while.body;
            condition_expr = statement->value.do_while.condition;
            post_test = true;
            break;
        case STATEMENT_FOR: {
            // The for statement gets its own scope, so that variables declared in the initializer are not visible
            // outside the loop.
            enter_scope(context);
            // Visit the initializer(s)
            if (statement->value.for_.initializer.kind == FOR_INIT_DECLARATION) {
                assert(statement->value.for_.initializer.declarations != NULL);
                for (size_t i = 0; i < statement->value.for_.initializer.declarations->size; i += 1) {
                    ir_visit_declaration(context, statement->value.for_.initializer.declarations->buffer[i]);
                }
            } else if (statement->value.for_.initializer.kind == FOR_INIT_EXPRESSION) {
                assert(statement->value.for_.initializer.expression != NULL);
                ir_visit_expression(context, statement->value.for_.initializer.expression);
            }
            body = statement->value.for_.body;
            condition_expr = statement->value.for_.condition;
            break;
        }
        default:
            fprintf(stderr, "%s:%d (%s) Error unrecognized statement type\n", __FILE__, __LINE__, __func__);
            exit(1);
    }

    // Label for the start of the loop
    ir_build_nop(context->builder, loop_start_label);

    if (!post_test && condition_expr != NULL) {
        // Evaluate the condition
        expression_result_t condition = ir_visit_expression(context, condition_expr);
        if (condition.is_lvalue) condition = get_rvalue(context, condition);

        // The condition must have a scalar type.
        if (!is_scalar_type(condition.c_type)) {
            append_compilation_error(&context->errors, (compilation_error_t) {
                .kind = ERR_INVALID_LOOP_CONDITION_TYPE,
                .location = condition_expr->span.start,
                .value.invalid_loop_condition_type = {
                    .type = condition.c_type,
                },
            });
            return;
        }

        // If the condition is false (0), then jump to the exit label. Otherwise continue to the loop body.
        const ir_type_t *condition_ir_type = ir_get_type_of_value(condition.value);
        ir_value_t zero = ir_get_zero_value(context, condition_ir_type);
        ir_var_t condition_var = temp_var(context, &IR_BOOL);
        ir_build_eq(context->builder, condition.value, zero, condition_var);
        ir_build_br_cond(context->builder, ir_value_for_var(condition_var), loop_exit_label);
    }

    // set the loop context while in the body (for break/continue)
    loop_context_t loop_context = enter_loop_context(context, loop_exit_label, loop_end_label);

    // Execute the loop body
    if (body != NULL) ir_visit_statement(context, body);

    // restore the loop context
    leave_loop_context(context, loop_context);

    // Label for the end of the loop body
    ir_build_nop(context->builder, loop_end_label);

    if (post_test && condition_expr != NULL) {
        // Evaluate the condition
        expression_result_t condition = ir_visit_expression(context, condition_expr);
        if (condition.is_lvalue) condition = get_rvalue(context, condition);

        // The condition must have a scalar type.
        if (!is_scalar_type(condition.c_type)) {
            append_compilation_error(&context->errors, (compilation_error_t) {
                .kind = ERR_INVALID_LOOP_CONDITION_TYPE,
                .location = condition_expr->span.start,
                .value.invalid_loop_condition_type = {
                    .type = condition.c_type,
                },
            });
            return;
        }

        // Evaluate the condition. If it evaluates to 0 (false), then jump to the exit label.
        const ir_type_t *condition_ir_type = ir_get_type_of_value(condition.value);
        ir_value_t zero = ir_get_zero_value(context, condition_ir_type);
        ir_var_t condition_var = temp_var(context, &IR_BOOL);
        ir_build_eq(context->builder, condition.value, zero, condition_var);
        ir_build_br_cond(context->builder, ir_value_for_var(condition_var), loop_exit_label);
    }

    if (statement->kind == STATEMENT_FOR && statement->value.for_.post != NULL)
        ir_visit_expression(context, statement->value.for_.post);

    // Jump back to the start of the loop
    ir_build_br(context->builder, loop_start_label);

    // Label to exit the loop
    ir_build_nop(context->builder, loop_exit_label);

    if (statement->kind == STATEMENT_FOR) leave_scope(context);
}

void ir_visit_break_statement(ir_gen_context_t *context, const statement_t *statement) {
    assert(statement != NULL && statement->kind == STATEMENT_BREAK);
    if (context->break_label == NULL) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_BREAK_OUTSIDE_OF_LOOP_OR_SWITCH,
            .location = statement->value.break_.keyword->position,
            .value.break_outside_of_loop_or_switch_case = {
                .keyword = *statement->value.break_.keyword,
            },
        });
        return;
    }

    ir_build_br(context->builder, context->break_label);
}

void ir_visit_continue_statement(ir_gen_context_t *context, const statement_t *statement) {
    assert(statement != NULL && statement->kind == STATEMENT_CONTINUE);
    if (context->continue_label == NULL) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_CONTINUE_OUTSIDE_OF_LOOP,
            .location = statement->value.continue_.keyword->position,
            .value.continue_outside_of_loop = {
                .keyword = *statement->value.continue_.keyword,
            },
        });
    }

    ir_build_br(context->builder, context->continue_label);
}

void ir_visit_labeled_statement(ir_gen_context_t *context, const statement_t *statement) {
    assert(statement != NULL && statement->kind == STATEMENT_LABEL);

    const token_t *source_label = statement->value.label_.identifier;

    // check if this is a duplicate label
    statement_t *previous_definition = NULL;
    bool exists = hash_table_lookup(&context->label_exists, source_label->value, (void**) &previous_definition);
    if (exists) {
        // label redefinition
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_REDEFINITION_OF_LABEL,
            .location = source_label->position,
            .value.redefinition_of_label = {
                .label = *source_label,
                .previous_definition = *previous_definition->value.label_.identifier,
            },
        });
    }

    // check if we've already mapped this label to an ir label
    char *ir_label = NULL;
    if (!hash_table_lookup(&context->label_map, source_label->value, (void**) &ir_label)) {
        // nope, need to generate the ir label and create the mapping
        ir_label = gen_label(context);
        hash_table_insert(&context->label_map, source_label->value, ir_label);
    }

    // add the definition of the label to the label_exists map so we can detect duplicate definitions, and so we can
    // validate that any goto instruction that references it is valid later
    hash_table_insert(&context->label_exists, source_label->value, (void *) statement);

    // insert the label into the ir
    ir_build_nop(context->builder, ir_label);

    // visit the inner statement
    if (statement->value.label_.statement != NULL) ir_visit_statement(context, statement->value.label_.statement);
}

void ir_visit_goto_statement(ir_gen_context_t *context, const statement_t *statement) {
    assert(statement != NULL && statement->kind == STATEMENT_GOTO);

    // add to the function goto statement list so we can validate it later (it may reference a label that hasn't been
    // visited yet)
    VEC_APPEND(&context->goto_statements, statement);

    const token_t *source_label =  statement->value.goto_.identifier;

    // check if we've already mapped this label to an ir label
    char *ir_label = NULL;
    if (!hash_table_lookup(&context->label_map, source_label->value, (void**) &ir_label)) {
        // nope, need to generate the ir label and create the mapping
        ir_label = gen_label(context);
        hash_table_insert(&context->label_map, source_label->value, ir_label);
    }

    // jump to the label
    ir_build_br(context->builder, ir_label);
}

void ir_visit_switch_statement(ir_gen_context_t *context, const statement_t *statement) {
    // get the value for the controlling expression
    expression_result_t expr = ir_visit_expression(context, statement->value.switch_.expression);
    if (expr.kind == EXPR_RESULT_ERR) return;
    if (expr.is_lvalue) expr = get_rvalue(context, expr);
    if (expr.c_type->kind != TYPE_INTEGER) {
        // must be an integer
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_SWITCH_EXPRESSION_TYPE,
            .location = statement->value.switch_.keyword->position,
            .value.invalid_switch_expression_type = {
                .keyword = statement->value.switch_.keyword,
                .type = expr.c_type,
            }
        });
        return;
    }

    // generate the label that will be used to jump to the end of the switch statement
    // this will also initially be the label for the default case, unless one is specified
    char *exit_label = gen_label(context);

    // create the switch instruction, it will be updated to add the case statements as we visit them
    ir_instruction_node_t *switch_node = ir_build_switch(context->builder, expr.value, NULL);

    // insert the switch instruction into the context, so we can add the cases as we find them
    ir_instruction_node_t *prev_switch_node = context->switch_node;
    context->switch_node = switch_node;

    // insert the exit label into the context, so we can jump to it if we encounter a break statement
    const char *prev_break_label = context->break_label;
    context->break_label = exit_label;

    // visit the switch statement body
    ir_visit_statement(context, statement->value.case_.statement);

    // restore the previous switch node (if this is a nested switch statement)
    context->switch_node = prev_switch_node;

    // restore the previous break label (if this is a nested switch statement, or inside of a loop)
    context->break_label = prev_break_label;

    // if the switch instruction doesn't contain a default case, add the exit label as the default
    ir_instruction_t *instruction = ir_builder_get_instruction(switch_node);
    if (instruction->value.switch_.default_label == NULL) instruction->value.switch_.default_label = exit_label;

    ir_build_nop(context->builder, exit_label);
}

bool ir_switch_contains_case(ir_instruction_t *instruction, ir_const_t const_value) {
    ir_switch_case_vector_t *cases = &instruction->value.switch_.cases;
    // TODO: this could be painfully slow for switch statements with lots (tens of thousands of cases?)
    //       consider a hashmap or bst instead of a vector to store the cases?
    for (int i = 0; i < cases->size; i += 1) {
        if (cases->buffer[i].const_val.value.i == const_value.value.i) return true;
    }
    return false;
}

void ir_visit_case_statement(ir_gen_context_t *context, const statement_t *statement) {
    assert(statement->kind == STATEMENT_CASE);

    ir_instruction_t *switch_instruction = NULL;

    // a case statement can only appear in the body of a switch statement
    if (context->switch_node == NULL) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_CASE_STATEMENT_OUTSIDE_OF_SWITCH,
            .location = statement->value.case_.keyword->position,
            .value.case_statement_outside_of_switch = {
                .keyword = statement->value.case_.keyword,
            },
        });
        // recoverable, continue
    } else {
        switch_instruction = ir_builder_get_instruction(context->switch_node);
    }

    // label for the case statement
    const char *case_label = gen_label(context);

    // add the case to the switch instruction (if its valid)
    if (statement->value.case_.expression != NULL) {
        // get the value of the case statement
        // must be a constant integer value
        expression_result_t expr = ir_visit_expression(context, statement->value.case_.expression);
        // the errors here are recoverable (in that we can continue semantic analysis), continue analysis but don't add
        // the case to the switch instruction
        if (expr.kind != EXPR_RESULT_ERR) {
            if (expr.c_type->kind != TYPE_INTEGER || expr.value.kind != IR_VALUE_CONST) {
                append_compilation_error(&context->errors, (compilation_error_t) {
                    .kind = ERR_INVALID_CASE_EXPRESSION,
                    .location = statement->value.case_.keyword->position,
                    .value.invalid_case_expression = {
                        .keyword = statement->value.case_.keyword,
                        .type = expr.c_type,
                    },
                });
            } else if (switch_instruction != NULL && ir_switch_contains_case(switch_instruction, expr.value.constant)) {
                // duplicate cases are not allowed
                append_compilation_error(&context->errors, (compilation_error_t) {
                    .kind = ERR_DUPLICATE_SWITCH_CASE,
                    .location = statement->value.case_.keyword->position,
                    .value.duplicate_switch_case = {
                        .keyword = statement->value.case_.keyword,
                        .value = expr.value.constant.value.i,
                    },
                });
            } else if (switch_instruction != NULL) {
                // add the case to the switch statement
                ir_switch_case_t switch_case = {
                    .const_val = expr.value.constant,
                    .label = case_label,
                };
                VEC_APPEND(&switch_instruction->value.switch_.cases, switch_case);
            }
        }
    } else {
        // default case
        if (switch_instruction != NULL) {
            // a switch statement can only contain one default case
            if (switch_instruction->value.switch_.default_label != NULL) {
                // error, duplicate default case
                append_compilation_error(&context->errors, (compilation_error_t) {
                    .kind = ERR_DUPLICATE_SWITCH_CASE,
                    .location = statement->value.case_.keyword->position,
                    .value.duplicate_switch_case = {
                        .keyword = statement->value.case_.keyword,
                    },
                });
            } else {
                switch_instruction->value.switch_.default_label = case_label;
            }
        }
    }

    // Add the label
    ir_build_nop(context->builder, case_label);

    // visit the case body
    ir_visit_statement(context, statement->value.case_.statement);

    // We don't jump out of the switch statement here, that only happens if and when we visit a break statement
    // inside the case statement.
}

typedef struct InitializerResult {
    const ir_type_t *type;
    bool has_const_value;
    ir_const_t const_value;
} ir_initializer_result_t;

const ir_initializer_result_t INITIALIZER_RESULT_ERR = {
    .type = NULL,
    .has_const_value = false,
};

ir_initializer_result_t ir_visit_initializer(ir_gen_context_t *context, ir_value_t ptr, const type_t *var_ctype, const initializer_t *initializer);

ir_initializer_result_t ir_visit_array_initializer(ir_gen_context_t *context, ir_value_t ptr, const type_t *c_type, const initializer_list_t *initializer) {
    const ir_type_t *type = ir_get_type_of_value(ptr);
    assert(type->kind == IR_TYPE_PTR);
    assert(type->value.ptr.pointee->kind == IR_TYPE_ARRAY);
    const ir_type_t *element_type = type->value.ptr.pointee->value.array.element;
    const ir_type_t *element_ptr_type = get_ir_ptr_type(element_type);
    assert(c_type->kind == TYPE_ARRAY);
    const type_t *c_element_type = c_type->value.array.element_type;

    bool known_size = c_type->value.array.size != NULL;
    if (!known_size) {
        //if size is not known, treat the ir type as a raw pointer
        ir_var_t tmp = temp_var(context, element_ptr_type);
        ir_build_bitcast(context->builder, ptr, tmp);
        ptr = ir_value_for_var(tmp);
    }

    // TODO: if the initializer is constant and contiguous, we can declare it as a global constant and memcpy it
    // For now this just lazily generates code to initialize the array
    int index = 0; // a designator could change the current index, so we track the array index separately than the index
                   // into the list of initializer list elements
    int inferred_array_length = 0;
    for (size_t i = 0; i < initializer->size; i += 1) {
        initializer_list_element_t element = initializer->buffer[i];
        if (element.designation != NULL && element.designation->size > 0) {
            // Handle the designator top level designator, then recursively visit the initializer
            designator_t designator = element.designation->buffer[0];
            if (designator.kind != DESIGNATOR_INDEX) {
                // TODO: error for invalid designator
                // TODO: source position for designators
                fprintf(stderr, "field designator can only be used to initialize a struct or union type\n");
                exit(1);
            }
            expression_result_t index_expr = ir_visit_constant_expression(context, designator.value.index);
            if (index_expr.kind == EXPR_RESULT_ERR) continue;
            if (index_expr.is_lvalue) index_expr = get_rvalue(context, index_expr);
            if (index_expr.value.constant.kind != IR_CONST_INT) {
                // TODO: array index designator constant must have integer type
                // TODO: source position for designators
                fprintf(stderr, "array index designator must have integer type\n");
                exit(1);
            }

            // the designators modifies the current array index, the next element in the array will start after the new
            // index of the current element
            index = index_expr.value.constant.value.i;
            ir_var_t element_ptr = temp_var(context, element_ptr_type);
            ir_build_get_array_element_ptr(context->builder, ptr, ir_make_const_int(ir_ptr_int_type(context), index), element_ptr);

            if (element.designation->size > 1) {
                // create a new initializer list that just includes this element, with the first designator removed
                designator_list_t nested_designators = VEC_INIT;
                for (int j = 1; j < element.designation->size; j += 1) {
                    VEC_APPEND(&nested_designators, element.designation->buffer[j]);
                }
                initializer_list_t nested_initializer_list = VEC_INIT;
                initializer_list_element_t nested_element = {
                    .designation = &nested_designators,
                    .initializer = element.initializer,
                };
                VEC_APPEND(&nested_initializer_list, nested_element);
                initializer_t nested_initializer = {
                    .kind = INITIALIZER_LIST,
                    .span = element.initializer->span, // TODO: this should include the designator
                    .value.list = &nested_initializer_list,
                };
                // recursively visit it
                ir_visit_initializer(context, ir_value_for_var(element_ptr), c_element_type, &nested_initializer);
                VEC_DESTROY(&nested_initializer_list);
                VEC_DESTROY(&nested_designators);
            } else {
                // visit the initializer
                ir_visit_initializer(context, ir_value_for_var(element_ptr), c_element_type, element.initializer);
            }
        } else {
            // For fixed size arrays, we will ignore elements in the initializer list that exceed the length of the
            // array. Arrays without a specified size (e.g. `int a[] = {1, 2, 3};`) are a special case, where we ignore
            // the ir array type length.
            if (known_size && i >= type->value.ptr.pointee->value.array.length) {
                // TODO: warn that initializer is longer than array
                continue;
            }
            ir_value_t index_val = ir_make_const_int(ir_ptr_int_type(context), index);
            ir_var_t element_ptr = temp_var(context, element_ptr_type);
            ir_build_get_array_element_ptr(context->builder, ptr, index_val, element_ptr);
            ir_visit_initializer(context, ir_value_for_var(element_ptr), c_type->value.array.element_type, element.initializer);
        }

        // bookkeeping for the array index
        index += 1;
        if (index > inferred_array_length) inferred_array_length = index;
    }

    // Return the type of the array
    // We need to do this to handle arrays without a specified size (e.g. `int a[] = {1, 2, 3};`), as the size is
    // inferred from the initializer, which we hadn't visited yet when we created the symbol.
    if (known_size) {
        // known size, can just return the type
        return (ir_initializer_result_t) {
            .type = ir_get_type_of_value(ptr)->value.ptr.pointee,
            .has_const_value = false, // TODO: constant arrays?
        };
    } else {
        // create a new type with the inferred length
        ir_type_t *new_type = malloc(sizeof(ir_type_t));
        *new_type = (ir_type_t) {
            .kind = IR_TYPE_ARRAY,
            .value.array = {
                .element = element_type,
                .length = inferred_array_length,
            },
        };
        return (ir_initializer_result_t) {
            .type = new_type,
            .has_const_value = false, // TODO: constant arrays?
        };
    }
}

ir_initializer_result_t ir_visit_struct_initializer(ir_gen_context_t *context, ir_value_t ptr, const type_t *c_type, const initializer_list_t *initializer_list) {
    assert(c_type->kind == TYPE_STRUCT_OR_UNION);
    const ir_type_t *ir_ptr_type = ir_get_type_of_value(ptr);
    assert(ir_ptr_type->kind == IR_TYPE_PTR);
    const ir_type_t *ir_struct_type = ir_ptr_type->value.ptr.pointee;
    assert(ir_struct_type->kind == IR_TYPE_STRUCT_OR_UNION);

    const field_ptr_vector_t *fields = &c_type->value.struct_or_union.fields;
    const hash_table_t *field_map = &c_type->value.struct_or_union.field_map;

    const ir_struct_field_ptr_vector_t *ir_fields = &ir_struct_type->value.struct_or_union.fields;
    const hash_table_t *ir_field_map = &ir_struct_type->value.struct_or_union.field_map;

    int field_index = 0;
    for (int i = 0; i < initializer_list->size; i += 1) {
        initializer_list_element_t element = initializer_list->buffer[i];
        if (element.designation != NULL && element.designation->size > 0) {
            // Handle the designator top level designator, then recursively visit the initializer
            designator_t designator = element.designation->buffer[0];
            if (designator.kind != DESIGNATOR_FIELD) {
                // TODO: error for invalid designator
                // TODO: source position for designators
                fprintf(stderr, "Index designator can only be used to initialize an array element\n");
                exit(1);
            }

            // Look up the field
            const token_t *field_name = designator.value.field;
            struct_field_t *field = NULL;
            if (!hash_table_lookup(field_map, field_name->value, (void**) &field)) {
                append_compilation_error(&context->errors, (compilation_error_t) {
                    .kind = ERR_INVALID_STRUCT_FIELD_REFERENCE,
                    .location = field_name->position,
                    .value.invalid_struct_field_reference = {
                        .field = *field_name,
                        .type = c_type,
                    },
                });

                // TODO: how to handle errors processing the initializer list?
                continue;
            }
            field_index = field->index;

            // Get the IR field (may have a different index due to padding)
            ir_struct_field_t *ir_field = NULL;
            assert(hash_table_lookup(ir_field_map, field_name->value, (void**) &ir_field));

            // Get a pointer to the field
            ir_var_t element_ptr = temp_var(context, get_ir_ptr_type(ir_field->type));
            ir_build_get_struct_member_ptr(context->builder, ptr, ir_field->index, element_ptr);

            if (element.designation->size > 1) {
                // create a new initializer list that just includes this element, with the first designator removed
                designator_list_t nested_designators = VEC_INIT;
                for (int j = 1; j < element.designation->size; j += 1) {
                    VEC_APPEND(&nested_designators, element.designation->buffer[j]);
                }
                initializer_list_t nested_initializer_list = VEC_INIT;
                initializer_list_element_t nested_element = {
                        .designation = &nested_designators,
                        .initializer = element.initializer,
                };
                VEC_APPEND(&nested_initializer_list, nested_element);
                initializer_t nested_initializer = {
                        .kind = INITIALIZER_LIST,
                        .span = element.initializer->span, // TODO: this should include the designator
                        .value.list = &nested_initializer_list,
                };
                // recursively visit it
                ir_visit_initializer(context, ir_value_for_var(element_ptr), field->type, &nested_initializer);
                VEC_DESTROY(&nested_initializer_list);
                VEC_DESTROY(&nested_designators);
            } else {
                // visit the initializer
                ir_visit_initializer(context, ir_value_for_var(element_ptr), field->type, element.initializer);
            }
        } else {
            // No designator, this just refers to the current field index (either the first field, or the field following
            // the last field we visited in the list).
            // e.g. `struct Foo foo = { 1, 2, 3 };`
            if (field_index >= fields->size) {
                // TODO: warn about too many elements in struct initializer
                continue;
            }

            const struct_field_t *field = fields->buffer[field_index];

            // Get the IR field, it may have a different index after adding padding, so look it up by name.
            const ir_struct_field_t *ir_field = NULL;
            assert(hash_table_lookup(ir_field_map, field->identifier->value, (void**) &ir_field));

            // Get a pointer to the field
            ir_var_t element_ptr = temp_var(context, get_ir_ptr_type(ir_field->type));
            ir_build_get_struct_member_ptr(context->builder, ptr, ir_field->index, element_ptr);

            // visit the initializer
            ir_visit_initializer(context, ir_value_for_var(element_ptr), field->type, element.initializer);
        }
        field_index += 1;
    }

    return (ir_initializer_result_t) {
        .type = ir_struct_type,
        .has_const_value = false,
    };
}

ir_initializer_result_t ir_visit_initializer_list(ir_gen_context_t *context, ir_value_t ptr, const type_t *c_type, const initializer_list_t *initializer_list) {
    const ir_type_t *ir_type = ir_get_type_of_value(ptr);
    assert(ir_type->kind == IR_TYPE_PTR);
    switch (ir_type->value.ptr.pointee->kind) {
        case IR_TYPE_ARRAY:
            return ir_visit_array_initializer(context, ptr, c_type, initializer_list);
        case IR_TYPE_STRUCT_OR_UNION: {
            return ir_visit_struct_initializer(context, ptr, c_type, initializer_list);
        }
        default: {
            fprintf(stderr, "%s:%d: Invalid type for initializer list\n", __FILE__, __LINE__);
            exit(1);
        }
    }
}

ir_initializer_result_t ir_visit_initializer(ir_gen_context_t *context, ir_value_t ptr, const type_t *var_ctype, const initializer_t *initializer) {
    switch (initializer->kind) {
        case INITIALIZER_EXPRESSION: {
            expression_result_t result =  ir_visit_expression(context, initializer->value.expression);

            // Error occurred while evaluating the initializer
            if (result.kind == EXPR_RESULT_ERR) return INITIALIZER_RESULT_ERR;

            // If the initializer is an lvalue, load the value
            // TODO: not sure that this is correct
            if (result.is_lvalue) result = get_rvalue(context, result);

            // Verify that the types are compatible, convert if necessary
            result = convert_to_type(context, result.value, result.c_type, var_ctype);
            if (result.kind == EXPR_RESULT_ERR) return INITIALIZER_RESULT_ERR;

            // Store the result in the allocated storage
            ir_build_store(context->builder, ptr, result.value);

            return (ir_initializer_result_t) {
                .type = ir_get_type_of_value(result.value),
                .has_const_value = result.value.kind == IR_VALUE_CONST,
                .const_value = result.value.constant,
            };
        }
        case INITIALIZER_LIST: {
            return ir_visit_initializer_list(context, ptr, var_ctype, initializer->value.list);
        }
        default: {
            assert(false && "Invalid initializer kind");
            exit(1);
        }
    }
}

bool is_tag_incomplete_type(const tag_t *tag) {
    assert(tag != NULL);
    return tag->ir_type == NULL;
}

const tag_t *tag_for_declaration(ir_gen_context_t *context, const type_t *c_type);

/**
 * Recursively resolve a struct type.
 * Needed to avoid incorrectly resolving the types of fields if a new struct or enum type with the same name as one
 * referenced by a field has been declared between the struct definition and its use.
 * Example:
 * ```
 * struct Bar { float a; float b; };
 * enum Baz { A, B, C };
 * struct Foo { struct Bar a; enum Baz b; };
 * if (c) {
 *     struct Bar { int a; int b; };
 *     struct Foo foo;               // <--- foo.a should have the type struct { float, float }
 *                                   //      but if we wait to look up what the type of tag Bar is at this point,
 *                                   //      we will choose the wrong one (struct { int, int })
 * }
 * ```
 * @param context Codegen context
 * @param c_type Type to resolve (must be a struct)
 * @return Resolved C type
 */
const type_t *resolve_struct_type(ir_gen_context_t *context, const type_t *c_type) {
    assert(c_type->kind == TYPE_STRUCT_OR_UNION);

    // TODO: this needlessly makes copies of every struct type

    type_t *resolved = malloc(sizeof(type_t));
    *resolved = *c_type;
    resolved->value.struct_or_union.field_map = hash_table_create_string_keys(64);
    resolved->value.struct_or_union.fields = (field_ptr_vector_t) VEC_INIT;

    for (int i = 0; i < c_type->value.struct_or_union.fields.size; i += 1) {
        const struct_field_t *field = c_type->value.struct_or_union.fields.buffer[i];
        const type_t *field_type = field->type;
        // TODO: this should also apply to enums?
        if (field->type->kind == TYPE_STRUCT_OR_UNION) {
            if (!field->type->value.struct_or_union.has_body || lookup_tag_in_current_scope(context, field->type->value.struct_or_union.identifier->value) == NULL) {
                // incomplete type we should try to resolve, or tag we haven't created yet
                const tag_t *tag = tag_for_declaration(context, field_type);
                field_type = tag->c_type;
            }
            field_type = resolve_struct_type(context, field_type);
            struct_field_t *new_field = malloc(sizeof(struct_field_t));
            *new_field = *field;
            new_field->type = field_type;
            field = new_field;
        }
        VEC_APPEND(&resolved->value.struct_or_union.fields, (struct_field_t*) field);
        hash_table_insert(&resolved->value.struct_or_union.field_map, field->identifier->value, (void*) field);
    }

    return resolved;
}

expression_result_t ir_visit_constant_expression(ir_gen_context_t *context, const expression_t *expression) {
    // Create a dummy fake instruction context and a function builder (visit_expression will attempt to generate
    // instructions if this expression isn't actually a compile time constant).
    ir_function_definition_t *cur_fn = context->function;
    ir_function_builder_t *cur_builder = context->builder;
    context->function = & (ir_function_definition_t) {
            .name = "__gen_constexpr",
            .type = NULL,
            .num_params = 0,
            .params = NULL,
            .is_variadic = false,
            .body = NULL,
    };
    context->builder = ir_builder_create();

    expression_result_t result = ir_visit_expression(context, expression);
    if (result.kind != EXPR_RESULT_VALUE) result = EXPR_ERR;

    if (result.kind == EXPR_RESULT_VALUE && result.value.kind != IR_VALUE_CONST) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_EXPECTED_CONSTANT_EXPRESSION,
            .location = expression->span.start,
        });
        result = EXPR_ERR;
    }

    // Delete the builder, throw away any generated instructions, and restore whatever the previous values were
    ir_builder_destroy(context->builder);
    context->function = NULL;

    context->function = cur_fn;
    context->builder = cur_builder;

    return result;
}

void visit_enumeration_constants(ir_gen_context_t *context, const enum_specifier_t *enum_specifier) {
    long value = 0;
    for (int i = 0; i < enum_specifier->enumerators.size; i += 1) {
        enumerator_t el = enum_specifier->enumerators.buffer[i];
        if (el.value != NULL) {
            expression_result_t res = ir_visit_constant_expression(context, el.value);
            if (res.kind == EXPR_RESULT_VALUE) {
                if (res.value.constant.kind != IR_CONST_INT) {
                    append_compilation_error(&context->errors, (compilation_error_t) {
                        .kind = ERR_ENUMERATION_CONSTANT_MUST_HAVE_INTEGER_TYPE,
                        .location = el.value->span.start,
                    });
                } else {
                    value = res.value.constant.value.i;
                }
            }
        }

        bool is_global = context->function == NULL;
        const char *name = is_global ? global_name(context) : temp_name(context);

        symbol_t *symbol = malloc(sizeof(symbol_t));
        *symbol = (symbol_t) {
            .identifier = el.identifier,
            .has_const_value = true,
            .ir_type = context->arch->sint,
            .c_type = &INT,
            .ir_ptr = {
                .type = NULL,
                .name = NULL,
            },
            .kind = SYMBOL_ENUMERATION_CONSTANT,
            .const_value = ir_make_const_int(context->arch->sint, value++).constant,
            .name = name,
        };
        declare_symbol(context, symbol);

        if (is_global) {
            ir_global_t *global = malloc(sizeof(ir_global_t));
            *global = (ir_global_t) {
                .initialized = true,
                .value = symbol->const_value,
                .type = symbol->ir_type,
                .name = symbol->name,
            };
            VEC_APPEND(&context->module->globals, global);
        }
    }
}

const tag_t *tag_for_declaration(ir_gen_context_t *context, const type_t *c_type) {
    assert(c_type->kind == TYPE_STRUCT_OR_UNION || c_type->kind == TYPE_ENUM);

    // From section 6.7.2.2 of C99 standard
    // Is this declaring a new tag, modifying a forward declaration, or just referencing an existing one?

    bool incomplete_type;
    const token_t *identifier;
    if (c_type->kind == TYPE_STRUCT_OR_UNION) {
        incomplete_type = !c_type->value.struct_or_union.has_body;
        identifier = c_type->value.struct_or_union.identifier;
    } else {
        assert(c_type->kind == TYPE_ENUM);
        incomplete_type = c_type->value.enum_specifier.enumerators.size == 0;
        identifier = c_type->value.enum_specifier.identifier;
    }

    if (identifier == NULL) {
        // anonymous tag, generate a unique identifier
        char *name = malloc(24);
        snprintf(name, 24, "__anon_tag_%d", context->tag_id_counter++);
        token_t * new_identifier = malloc(sizeof(token_t));
        *new_identifier = (token_t) {
            .kind = TK_IDENTIFIER,
            .value = name,
            // TODO: get position of declaration?
            .position = {
                .path = "",
                .column = 0,
                .line = 0,
            }
        };
        identifier = new_identifier;
    }

    tag_t *tag = lookup_tag_in_current_scope(context, identifier->value);
    // If there was already a tag with this name declared in the current scope. If one or both are incomplete types
    // (e.g. forward declarations, such as `struct Foo;`), then it's ok, otherwise it is a redefinition error.
    if (tag != NULL && !is_tag_incomplete_type(tag) && !incomplete_type) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_REDEFINITION_OF_TAG,
            .value.redefinition_of_tag = {
                .redefinition = identifier,
                .previous_definition = tag->identifier,
            },
        });
    }

    if (incomplete_type) {
        // Could be a forward declaration, also could be a reference to an existing tag.
        if (tag == NULL) tag = lookup_tag(context, identifier->value);
        if (tag == NULL) {
            // Declare a new tag
            size_t id_len = strlen(identifier->value) + 7; // max of 5 chars for id (unsigned short), plus 1 for _ and 1 for null terminator
            char *id = malloc(id_len);
            snprintf(id, id_len, "%s_%u", identifier->value, context->global_id_counter++);
            tag = malloc(sizeof(tag_t));
            *tag = (tag_t) {
                .identifier = identifier,
                .uid = id,
                .ir_type = NULL,
                .c_type = NULL, // null = incomplete
            };
            declare_tag(context, tag);
        }
        return tag;
    } else {
        // Defines a new tag
        // First declare an incomplete tag to allow for recursive references (e.g. `struct Foo { struct Foo *next; };`)
        size_t id_len = strlen(identifier->value) + 7; // 5 chars for id counter (unsigned short), 1 for _ and 1 for null terminator
        char *id = malloc(id_len);
        snprintf(id, id_len, "%s_%u", identifier->value, context->global_id_counter++);
        tag = malloc(sizeof(tag_t));
        *tag = (tag_t) {
            .identifier = identifier,
            .uid = id,
            .ir_type = NULL,
            .c_type = NULL, // null = incomplete
        };
        declare_tag(context, tag);

        if (c_type->kind == TYPE_STRUCT_OR_UNION) {
            // Resolve the struct c type
            const type_t *resolved_type = resolve_struct_type(context, c_type);

            // Visit the struct/union body to build the IR type, and update the tag
            const ir_type_t *ir_type = get_ir_struct_type(context, resolved_type, id);
            tag->ir_type = ir_type;
            tag->c_type = resolved_type;
        } else {
            // TODO: smaller enumeration constants based on max value?
            tag->ir_type = context->arch->sint;
            tag->c_type = &INT;

            // Declare identifiers in the current scope for the enumeration constants
            visit_enumeration_constants(context, &c_type->value.enum_specifier);
        }

        return tag;
    }
}

// TODO: This is a bit of a mess, and duplicates a lot of code from ir_visit_declaration.
//       They should probably be combined into a single function.
void ir_visit_global_declaration(ir_gen_context_t *context, const declaration_t *declaration) {
    assert(context != NULL && "Context must not be NULL");
    assert(declaration != NULL && "Declaration must not be NULL");

    // Typedef-name resolution is handled by the parser. This is a no-op.
    if (declaration->type->storage_class == STORAGE_CLASS_TYPEDEF) {
        return;
    }

    // Does this declare or reference a tag? (TODO: also support enums)
    const tag_t *tag = NULL;
    if (declaration->type->kind == TYPE_STRUCT_OR_UNION) {
        tag = tag_for_declaration(context, declaration->type);
    }

    if (declaration->identifier == NULL) {
        // this only declares a tag
        return;
    }

    // Create the symbol for the variable declared by this declaration
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
                    .value.redefinition_of_symbol = {
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
                    .value.redefinition_of_symbol = {
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
                    .value.redefinition_of_symbol = {
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
            .has_const_value = false,
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

        expression_result_t result;
        if (declaration->initializer->kind == INITIALIZER_EXPRESSION) {
            result = ir_visit_constant_expression(context, declaration->initializer->value.expression);
        } else {
            fprintf(stderr, "%s:%d: Codegen for initializer lists unimplemented\n", __FILE__, __LINE__);
            exit(1);
        }
        if (result.c_type == NULL) return; // Invalid initializer

        // Typecheck/convert the initializer
        result = convert_to_type(context, result.value, result.c_type, declaration->type);

        if (result.value.kind != IR_VALUE_CONST) {
            // The initializer must be a constant expression
            append_compilation_error(&context->errors, (compilation_error_t) {
                .kind = ERR_GLOBAL_INITIALIZER_NOT_CONSTANT,
                .location = declaration->initializer->span.start,
                .value.global_initializer_not_constant = {
                    .declaration = declaration,
                },
            });
            return;
        }
        global->value = result.value.constant;
        symbol->has_const_value = true;
        symbol->const_value = result.value.constant;
    } else if (global != NULL) {
        // Default value for uninitialized global variables
        if (is_floating_type(declaration->type)) {
            global->value = (ir_const_t) {
                .kind = IR_CONST_FLOAT,
                .type = symbol->ir_type,
                .value.f = 0.0,
            };
        } else {
            global->value = (ir_const_t) {
                .kind = IR_CONST_INT,
                .type = symbol->ir_type,
                .value.i = 0,
            };
        }
    }
}

void ir_visit_declaration(ir_gen_context_t *context, const declaration_t *declaration) {
    assert(context != NULL && "Context must not be NULL");
    assert(declaration != NULL && "Declaration must not be NULL");

    if (declaration->type->storage_class == STORAGE_CLASS_TYPEDEF) {
        // Typedefs are a no-op
        // The actual typedef-name resolution happens in the parser.
        return;
    }

    // Does this declare or reference a tag?
    const tag_t *tag = NULL;
    if (declaration->type->kind == TYPE_STRUCT_OR_UNION || declaration->type->kind == TYPE_ENUM) {
        tag = tag_for_declaration(context, declaration->type);
    }

    if (declaration->identifier == NULL) {
        // this only declares a tag
        return;
    }

    // Verify that this declaration is not a redeclaration of an existing symbol
    symbol_t *symbol = lookup_symbol_in_current_scope(context, declaration->identifier->value);
    if (symbol != NULL) {
        // Symbols in the same scope must have unique names, redefinition is not allowed.
        append_compilation_error(&context->errors, (compilation_error_t) {
            .location = declaration->identifier->position,
            .kind = ERR_REDEFINITION_OF_SYMBOL,
            .value.redefinition_of_symbol = {
                .redefinition = declaration->identifier,
                .previous_definition = symbol->identifier,
            },
        });
        return;
    }

    const type_t *c_type;
    const ir_type_t *ir_type;
    if (tag == NULL) {
        c_type = declaration->type;
        ir_type = get_ir_type(context, declaration->type);
    } else {
        c_type = tag->c_type;
        ir_type = tag->ir_type;
    }

    // Create a new symbol for this declaration and add it to the current scope
    symbol = malloc(sizeof(symbol_t));
    *symbol = (symbol_t) {
        .kind = SYMBOL_LOCAL_VARIABLE, // TODO: handle global/static variables
        .identifier = declaration->identifier,
        .name = declaration->identifier->value,
        .c_type = c_type,
        .ir_type = ir_type,
        .ir_ptr = (ir_var_t) {
            .name = temp_name(context),
            .type = get_ir_ptr_type(ir_type),
        },
        .has_const_value = false,
    };
    declare_symbol(context, symbol);

    // Allocate storage space for the variable
    ir_instruction_node_t *alloca_node = insert_alloca(context, ir_type, symbol->ir_ptr);

    // Evaluate the initializer if present, and store the result in the allocated storage
    if (declaration->initializer != NULL) {
        ir_initializer_result_t initializer_result =
                ir_visit_initializer(context, ir_value_for_var(symbol->ir_ptr), symbol->c_type, declaration->initializer);
        const ir_type_t *value_type = initializer_result.type;

        // If the variable was an array with a length inferred from the initializer list (e.g. `int a[] = {1, 2, 3};`),
        // we need to go back and update the symbol type and alloca parameter now that we know the size.
        // TODO: this seems like a hack, maybe there's a better way to do this?
        if (value_type != NULL && c_type->kind == TYPE_ARRAY && c_type->value.array.size == NULL) {
            // update the symbol
            symbol->ir_type = value_type;
            symbol->ir_ptr.type = get_ir_ptr_type(value_type);
            // update the alloca instruction
            ir_instruction_t *alloca_instr = ir_builder_get_instruction(alloca_node);
            alloca_instr->value.alloca.type = value_type;
            alloca_instr->value.alloca.result = symbol->ir_ptr;
        }

        // If this variable has a constant type, and the initializer is a constant, then we can treat this as a compile
        // time constant (e.g. for constant propagation, or for use in something requiring a constant expression).
        // TODO: does this work correctly for pointers (e.g. `const int *foo = &bar`)?
        if (symbol->c_type->is_const && initializer_result.has_const_value) {
            symbol->has_const_value = true;
            symbol->const_value = initializer_result.const_value;
        }
    }
}

expression_result_t ir_visit_expression(ir_gen_context_t *context, const expression_t *expression) {
    assert(context != NULL && "Context must not be NULL");
    assert(expression != NULL && "Expression must not be NULL");

    switch (expression->kind) {
        case EXPRESSION_ARRAY_SUBSCRIPT:
            return ir_visit_array_subscript_expression(context, expression);
        case EXPRESSION_BINARY:
            return ir_visit_binary_expression(context, expression);
        case EXPRESSION_CALL:
            return ir_visit_call_expression(context, expression);
        case EXPRESSION_CAST:
            return ir_visit_cast_expression(context, expression);
        case EXPRESSION_MEMBER_ACCESS:
            return ir_visit_member_access_expression(context, expression);
        case EXPRESSION_PRIMARY:
            return ir_visit_primary_expression(context, expression);
        case EXPRESSION_SIZEOF:
            return ir_visit_sizeof_expression(context, expression);
        case EXPRESSION_TERNARY:
            return ir_visit_ternary_expression(context, expression);
        case EXPRESSION_UNARY:
            return ir_visit_unary_expression(context, expression);
        case EXPRESSION_COMPOUND_LITERAL:
            return ir_visit_compound_literal(context, expression);
    }

    // TODO
    return EXPR_ERR;
}

expression_result_t ir_visit_array_subscript_expression(ir_gen_context_t *context, const expression_t *expr) {
    expression_result_t target = ir_visit_expression(context, expr->value.array_subscript.array);
    if (target.kind == EXPR_RESULT_ERR) return EXPR_ERR;

    // The target must be an array or a pointer
    if (target.c_type->kind != TYPE_ARRAY && target.c_type->kind != TYPE_POINTER) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_SUBSCRIPT_TARGET,
            .location = expr->value.array_subscript.array->span.start, // TODO, use the '[' token position?
        });
        return EXPR_ERR;
    }

    // If the target is a pointer, we need to dereference it to get the base pointer
    if (target.c_type->kind == TYPE_POINTER) {
        target = get_rvalue(context, target);
    }

    ir_value_t base_ptr;
    if (target.kind == EXPR_RESULT_VALUE) {
        assert(ir_get_type_of_value(target.value)->kind == IR_TYPE_PTR);
        base_ptr = target.value;
    } else {
        base_ptr = get_indirect_ptr(context, target);
    }

    const ir_type_t *ptr_type = ir_get_type_of_value(base_ptr);
    const ir_type_t *element_type = ptr_type->value.ptr.pointee->kind == IR_TYPE_ARRAY
        ? ptr_type->value.ptr.pointee->value.array.element
        : ptr_type->value.ptr.pointee;

    expression_result_t index = ir_visit_expression(context, expr->value.array_subscript.index);
    if (index.kind == EXPR_RESULT_ERR) return EXPR_ERR;
    if (index.is_lvalue) index = get_rvalue(context, index);
    assert(index.kind == EXPR_RESULT_VALUE);

    // The subscript must have an integer type
    if (!is_integer_type(index.c_type)) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_SUBSCRIPT_TYPE,
            .location = expr->value.array_subscript.index->span.start,
        });
        return EXPR_ERR;
    }

    ir_var_t result = temp_var(context, get_ir_ptr_type(element_type));
    ir_build_get_array_element_ptr(context->builder, base_ptr, index.value, result);

    const type_t *result_type = target.c_type->kind == TYPE_ARRAY
        ? target.c_type->value.array.element_type
        : target.c_type->value.pointer.base;
    return (expression_result_t) {
        .kind = EXPR_RESULT_VALUE,
        .c_type = result_type,
        .is_lvalue = true,
        .is_string_literal = false,
        .addr_of = false,
        .value = ir_value_for_var(result),
    };
}

expression_result_t ir_visit_call_expression(ir_gen_context_t *context, const expression_t *expr) {
    expression_result_t function = ir_visit_expression(context, expr->value.call.callee);
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
            .location = expr->value.call.callee->span.start,
            .value.call_target_not_function = {
                .type = function.c_type,
            }
        });
        return EXPR_ERR;
    }

    // Check that the number of arguments matches function arity
    size_t expected_args_count = function.c_type->value.function.parameter_list->length;
    bool variadic = function.c_type->value.function.parameter_list->variadic;
    size_t actual_args_count = expr->value.call.arguments.size;
    if ((variadic && actual_args_count < expected_args_count) || (!variadic && actual_args_count != expected_args_count)) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_CALL_ARGUMENT_COUNT_MISMATCH,
            .location = expr->value.call.callee->span.start,
            .value.call_argument_count_mismatch = {
                .expected = expected_args_count,
                .actual = actual_args_count,
            }
        });
        return EXPR_ERR;
    }

    // Evaluate the arguments
    ir_value_t *args = malloc(sizeof(ir_value_t) * actual_args_count);
    for (size_t i = 0; i < actual_args_count; i += 1) {
        expression_result_t arg = ir_visit_expression(context, expr->value.call.arguments.buffer[i]);

        // Error occurred while evaluating the argument
        if (arg.kind == EXPR_RESULT_ERR) return EXPR_ERR;

        if (arg.c_type->kind == TYPE_ARRAY) {
            arg = convert_to_type(context, arg.value, get_ptr_type(arg.c_type), get_ptr_type(arg.c_type->value.array.element_type));
        } else if (arg.is_lvalue) {
            arg = get_rvalue(context, arg);
        }

        // Implicit conversion to the parameter type
        // Variadic arguments are _NOT_ converted to a specific type, but chars, shorts, and floats are promoted
        // Array arguments are passed as pointers
        if (i < function.c_type->value.function.parameter_list->length) {
            const type_t *param_type = function.c_type->value.function.parameter_list->parameters[i]->type;
            if (param_type->kind == TYPE_ARRAY) param_type = get_ptr_type(param_type->value.array.element_type);
            arg = convert_to_type(context, arg.value, arg.c_type, param_type);
        } else {
            if (arg.c_type->kind == TYPE_INTEGER) {
                const type_t *new_type = type_after_integer_promotion(arg.c_type);
                arg = convert_to_type(context, arg.value, arg.c_type, new_type);
            } else if (arg.c_type->kind == TYPE_FLOATING && arg.c_type->value.floating == FLOAT_TYPE_FLOAT) {
                arg = convert_to_type(context, arg.value, arg.c_type, &DOUBLE);
            }
        }

        // Conversion was invalid
        if (arg.kind == EXPR_RESULT_ERR) return EXPR_ERR;

        args[i] = arg.value;
    }

    // Emit the call instruction
    ir_var_t *result = NULL;
    if (function.c_type->value.function.return_type->kind != TYPE_VOID) {
        result = (ir_var_t*) malloc(sizeof(ir_var_t));
        *result = temp_var(context, get_ir_type(context,function.c_type->value.function.return_type));
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
        .c_type = function.c_type->value.function.return_type,
        .is_lvalue = false,
        .is_string_literal = false,
        .addr_of = false,
        .value = result_value,
    };
}

expression_result_t ir_visit_cast_expression(ir_gen_context_t *context, const expression_t *expr) {
    assert(expr != NULL && expr->kind == EXPRESSION_CAST);

    expression_result_t value = ir_visit_expression(context, expr->value.cast.expression);
    if (value.kind == EXPR_RESULT_ERR) return EXPR_ERR;
    if (value.is_lvalue) value = get_rvalue(context, value);
    return convert_to_type(context, value.value, value.c_type, expr->value.cast.type);
}

expression_result_t ir_visit_binary_expression(ir_gen_context_t *context, const expression_t *expr) {
    assert(context != NULL && "Context must not be NULL");
    assert(expr != NULL && "Expression must not be NULL");
    assert(expr->kind == EXPRESSION_BINARY);

    switch (expr->value.binary.kind) {
        case BINARY_ARITHMETIC: {
            expression_result_t lhs = ir_visit_expression(context, expr->value.binary.left);
            expression_result_t rhs = ir_visit_expression(context, expr->value.binary.right);
            if (expr->value.binary.operator.arithmetic == BINARY_ARITHMETIC_ADD ||
                expr->value.binary.operator.arithmetic == BINARY_ARITHMETIC_SUBTRACT) {
                return ir_visit_additive_binexpr(context, expr, lhs, rhs);
            } else {
                return ir_visit_multiplicative_binexpr(context, expr, lhs, rhs);
            }
        }
        case BINARY_ASSIGNMENT: {
            return ir_visit_assignment_binexpr(context, expr);
        }
        case BINARY_BITWISE: {
            expression_result_t lhs = ir_visit_expression(context, expr->value.binary.left);
            expression_result_t rhs = ir_visit_expression(context, expr->value.binary.right);
            return ir_visit_bitwise_binexpr(context, expr, lhs, rhs);
        }
        case BINARY_COMMA: {
            // TODO
            source_position_t pos = expr->value.binary.operator_token->position;
            fprintf(stderr, "%s:%d:%d: comma operator not yet implemented\n",
                pos.path, pos.line, pos.column);
            exit(1);
        }
        case BINARY_COMPARISON: {
            return ir_visit_comparison_binexpr(context, expr);
        }
        case BINARY_LOGICAL: {
            return ir_visit_logical_expression(context, expr);
        }
    }
}

expression_result_t ir_visit_additive_binexpr(ir_gen_context_t *context, const expression_t *expr, expression_result_t left, expression_result_t right) {
    const type_t *result_type;
    const ir_type_t *ir_result_type;

    // Bubble up errors if the operands are invalid.
    if (left.kind == EXPR_RESULT_ERR || right.kind == EXPR_RESULT_ERR) return EXPR_ERR;

    bool is_addition = expr->value.binary.operator_token->kind == TK_PLUS
                     || expr->value.binary.operator_token->kind == TK_PLUS_ASSIGN;

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
                    .value.i = 0,
                }
            };

            if (is_floating_type(result_type)) {
                result.constant.value.f = is_addition ? left.value.constant.value.f + right.value.constant.value.f
                                                      : left.value.constant.value.f - right.value.constant.value.f;
            } else {
                result.constant.value.i = is_addition ? left.value.constant.value.i + right.value.constant.value.i
                                                      : left.value.constant.value.i - right.value.constant.value.i;
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
                .location = expr->value.binary.operator_token->position,
                .value.invalid_binary_expression_operands = {
                    .operator = expr->value.binary.operator_token->value,
                    .left_type = left.c_type,
                    .right_type = right.c_type,
                },
            });
            return EXPR_ERR;
        }

        assert(pointer_operand.kind == EXPR_RESULT_VALUE); // todo
        assert(pointer_operand.kind == EXPR_RESULT_VALUE);
        ir_var_t result = temp_var(context, ir_get_type_of_value(pointer_operand.value));
        ir_build_get_array_element_ptr(context->builder, pointer_operand.value, integer_operand.value, result);

        // The result type is the same as the pointer type.
        result_type = pointer_operand.c_type;

        return (expression_result_t) {
            .kind = EXPR_RESULT_VALUE,
            .c_type = result_type,
            .is_lvalue = false,
            .is_string_literal = false,
            .addr_of = false,
            .value = ir_value_for_var(result),
        };
    } else {
        // Invalid operand types.
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_BINARY_EXPRESSION_OPERANDS,
            .location = expr->value.binary.operator_token->position,
            .value.invalid_binary_expression_operands = {
                .operator = expr->value.binary.operator_token->value,
                .left_type = left.c_type,
                .right_type = right.c_type,
            },
        });
        return EXPR_ERR;
    }
}

expression_result_t ir_visit_multiplicative_binexpr(ir_gen_context_t *context, const expression_t *expr, expression_result_t left, expression_result_t right) {
    bool is_modulo = expr->value.binary.operator_token->kind == TK_PERCENT || expr->value.binary.operator_token->kind == TK_MOD_ASSIGN;
    bool is_division = expr->value.binary.operator_token->kind == TK_SLASH || expr->value.binary.operator_token->kind == TK_DIVIDE_ASSIGN;

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
            .location = expr->value.binary.operator_token->position,
            .value.invalid_binary_expression_operands = {
                .operator = expr->value.binary.operator_token->value,
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
            .value.i = 0,
        };

        if (ir_is_integer_type(ir_result_type)) {
            // TODO: emit warning and set undefined value for division by zero
            // For now we will just set the value to 0 and move on
            if (is_division && right.value.constant.value.i == 0) {
                value.value.i = 0;
            } else {
                if (is_modulo) value.value.i = left.value.constant.value.i % right.value.constant.value.i;
                else if (is_division) value.value.i = left.value.constant.value.i / right.value.constant.value.i;
                else value.value.i = left.value.constant.value.i * right.value.constant.value.i;
            }
        } else {
            // no modulo operator for floating point
            if (is_division) value.value.f = left.value.constant.value.f / right.value.constant.value.f;
            else value.value.f = left.value.constant.value.f * right.value.constant.value.f;
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
        } else { // multiplication
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

expression_result_t ir_visit_bitwise_binexpr(ir_gen_context_t *context, const expression_t *expr, expression_result_t left, expression_result_t right) {
    // Bubble up errors if the operands are invalid.
    if (left.kind == EXPR_RESULT_ERR || right.kind == EXPR_RESULT_ERR) return EXPR_ERR;

    if (left.is_lvalue) left = get_rvalue(context, left);
    if (right.is_lvalue) right = get_rvalue(context, right);

    bool is_lshift = expr->value.binary.operator_token->kind == TK_LSHIFT || expr->value.binary.operator_token->kind == TK_LSHIFT_ASSIGN;
    bool is_rshift = expr->value.binary.operator_token->kind == TK_RSHIFT || expr->value.binary.operator_token->kind == TK_RSHIFT_ASSIGN;
    bool is_shift = is_lshift || is_rshift;
    bool is_and = expr->value.binary.operator_token->kind == TK_AMPERSAND || expr->value.binary.operator_token->kind == TK_BITWISE_AND_ASSIGN;
    bool is_or = expr->value.binary.operator_token->kind == TK_BITWISE_OR || expr->value.binary.operator_token->kind == TK_BITWISE_OR_ASSIGN;

    // For bitwise operators, both operands must have integer type
    if (!is_integer_type(left.c_type) || !is_integer_type(right.c_type)) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_BINARY_EXPRESSION_OPERANDS,
            .location = expr->value.binary.operator_token->position,
            .value.invalid_binary_expression_operands = {
                .operator = expr->value.binary.operator_token->value,
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
            .value.i = 0,
        };

        if (is_lshift) value.value.i = left.value.constant.value.i << right.value.constant.value.i;
        else if (is_rshift) value.value.i = left.value.constant.value.i >> right.value.constant.value.i;
        else if (is_and) value.value.i = left.value.constant.value.i & right.value.constant.value.i;
        else if (is_or) value.value.i = left.value.constant.value.i | right.value.constant.value.i;
        else value.value.i = left.value.constant.value.i ^ right.value.constant.value.i;

        result = (ir_value_t) {
            .kind = IR_VALUE_CONST,
            .constant = value,
        };
    } else {
        ir_var_t temp = temp_var(context, result_type);
        if (is_shift) {
            if (is_lshift) ir_build_shl(context->builder, left.value, right.value, temp);
            else ir_build_shr(context->builder, left.value, right.value, temp);
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
    expression_result_t left = ir_visit_expression(context, expr->value.binary.left);
    expression_result_t right = ir_visit_expression(context, expr->value.binary.right);

    // Bubble up errors if the operands are invalid.
    if (left.kind == EXPR_RESULT_ERR || right.kind == EXPR_RESULT_ERR) return EXPR_ERR;

    // The left operand must be a lvalue.
    if (!left.is_lvalue || left.c_type->is_const) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_ASSIGNMENT_TARGET,
            .location = expr->value.binary.operator_token->position,
        });
        return EXPR_ERR;
    }

    if (expr->value.binary.operator_token->kind != TK_ASSIGN) {
        switch (expr->value.binary.operator.assignment) {
            case BINARY_ADD_ASSIGN:
            case BINARY_SUBTRACT_ASSIGN:
                right = ir_visit_additive_binexpr(context, expr, left, right);
                break;
            case BINARY_DIVIDE_ASSIGN:
            case BINARY_MODULO_ASSIGN:
            case BINARY_MULTIPLY_ASSIGN:
                right = ir_visit_multiplicative_binexpr(context, expr, left, right);
                break;
            case BINARY_BITWISE_AND_ASSIGN:
            case BINARY_BITWISE_OR_ASSIGN:
            case BINARY_BITWISE_XOR_ASSIGN:
            case BINARY_SHIFT_LEFT_ASSIGN:
            case BINARY_SHIFT_RIGHT_ASSIGN:
                right = ir_visit_bitwise_binexpr(context, expr, left, right);
                break;
            default:
                // This should be unreachable
                fprintf(stderr, "%s:%d IR generation error, unrecognized assignment operator\n", __FILE__, __LINE__);
                exit(1);
        }
    }

    bool is_struct_assignment = right.c_type->kind == TYPE_STRUCT_OR_UNION;
    if (!is_struct_assignment && right.is_lvalue) right = get_rvalue(context, right);

    // Generate an assignment instruction.
    if (!types_equal(left.c_type, right.c_type)) {
        // Convert the right operand to the type of the left operand.
        right = convert_to_type(context, right.value, right.c_type, left.c_type);
        if (right.kind == EXPR_RESULT_ERR) return EXPR_ERR;
    }

    ir_value_t ptr;
    if (left.kind == EXPR_RESULT_VALUE) {
        ptr = left.value;
    } else if (left.kind == EXPR_RESULT_INDIRECTION) {
        ptr = get_indirect_ptr(context, left);
    } else {
        return EXPR_ERR;
    }

    if (is_struct_assignment) {
        // the struct types should be the same, so it doesn't matter if we use the length of the dest or src
        ssize_t size = ir_size_of_type_bytes(context->arch, ir_get_type_of_value(ptr)->value.ptr.pointee);
        ir_value_t length_val = ir_make_const_int(context->arch->ptr_int_type, size);
        ir_build_memcpy(context->builder, ptr, right.value, length_val);
    } else {
        ir_build_store(context->builder, ptr, right.value);
    }

    // assignments can be chained, e.g. `a = b = c;`
    return left;
}

expression_result_t ir_visit_comparison_binexpr(ir_gen_context_t *context, const expression_t *expr) {
    assert(context != NULL && "Context must not be NULL");
    assert(expr != NULL && "Expression must not be NULL");
    assert(expr->kind == EXPRESSION_BINARY && expr->value.binary.kind == BINARY_COMPARISON);

    // Evaluate the left and right operands.
    expression_result_t left = ir_visit_expression(context, expr->value.binary.left);
    expression_result_t right = ir_visit_expression(context, expr->value.binary.right);

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
        const binary_comparison_operator_t op = expr->value.binary.operator.comparison;

        if (left.value.kind == IR_VALUE_CONST && right.value.kind == IR_VALUE_CONST) {
            // constant folding
            ir_const_t value = {
                .kind = IR_CONST_INT,
                .type = &IR_BOOL,
                .value.i = 0,
            };
            bool floating = is_floating_type(common_type);
            long double leftf;
            long double rightf;
            long long lefti;
            long long righti;
            if (floating) {
                leftf = left.value.constant.kind == IR_CONST_INT ? left.value.constant.value.i : left.value.constant.value.f;
                rightf = right.value.constant.kind == IR_CONST_INT ? right.value.constant.value.i : right.value.constant.value.f;
            } else {
                lefti = left.value.constant.kind == IR_CONST_INT ? left.value.constant.value.i : left.value.constant.value.f;
                righti = right.value.constant.kind == IR_CONST_INT ? right.value.constant.value.i : right.value.constant.value.f;
            }
            switch (op) {
                case BINARY_COMPARISON_EQUAL:
                    value.value.i = floating ? leftf == rightf : lefti == righti;
                    break;
                case BINARY_COMPARISON_NOT_EQUAL:
                    value.value.i = floating ? leftf != rightf : lefti != righti;
                    break;
                case BINARY_COMPARISON_LESS_THAN:
                    value.value.i = floating ? leftf < rightf : lefti < righti;
                    break;
                case BINARY_COMPARISON_LESS_THAN_OR_EQUAL:
                    value.value.i = floating ? leftf <= rightf : lefti <= righti;
                    break;
                case BINARY_COMPARISON_GREATER_THAN:
                    value.value.i = floating ? leftf > rightf : lefti > righti;
                    break;
                case BINARY_COMPARISON_GREATER_THAN_OR_EQUAL:
                    value.value.i = floating ? leftf >= rightf : lefti >= righti;
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
            .location = expr->value.binary.operator_token->position,
            .value.invalid_binary_expression_operands = {
                .operator = expr->value.binary.operator_token->value,
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
    assert(expr->kind == EXPRESSION_BINARY && expr->value.binary.kind == BINARY_LOGICAL);

    // Whether the operator is logical AND ('&&') or logical OR ('||')
    bool is_logical_and = expr->value.binary.operator.logical == BINARY_LOGICAL_AND;
    bool is_logical_or = !is_logical_and;

    // Evaluate the left operand
    // The logical && and || operators are short-circuiting, so if the left operand is false (for &&) or true (for ||),
    // then the right operand is not evaluated.
    expression_result_t left = ir_visit_expression(context, expr->value.binary.left);
    if (left.kind == EXPR_RESULT_ERR) return EXPR_ERR;
    if (left.is_lvalue) left = get_rvalue(context, left);

    // Both operands must have scalar type
    if (!is_scalar_type(left.c_type)) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_LOGICAL_BINARY_EXPRESSION_OPERAND_TYPE,
            .location = expr->value.binary.left->span.start,
            .value.invalid_logical_binary_expression_operand_type = {
                .type = left.c_type,
            },
        });
        return EXPR_ERR;
    }

    // Convert the left operand to a boolean value (if it is not already)
    // We already know that the left operand is a scalar type, so we don't need to check for errors since its a
    // valid conversion.
    ir_value_t left_bool = get_boolean_value(context, left.value, left.c_type, expr->value.binary.left).value;
    if (left_bool.kind == IR_VALUE_CONST) {
        // constant folding
        if ((is_logical_and && left_bool.constant.value.i == 0) || (is_logical_or && left_bool.constant.value.i != 0)) {
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
            expression_result_t right = ir_visit_expression(context, expr->value.binary.right);
            if (right.kind == EXPR_RESULT_ERR) return EXPR_ERR;
            if (right.is_lvalue) right = get_rvalue(context, right);
            if (!is_scalar_type(right.c_type)) {
                append_compilation_error(&context->errors, (compilation_error_t) {
                    .kind = ERR_INVALID_LOGICAL_BINARY_EXPRESSION_OPERAND_TYPE,
                    .location = expr->value.binary.right->span.start,
                    .value.invalid_logical_binary_expression_operand_type = {
                        .type = right.c_type,
                    },
                });
                return EXPR_ERR;
            }
            ir_value_t right_bool = get_boolean_value(context, right.value, right.c_type, expr->value.binary.right).value;
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
    expression_result_t right = ir_visit_expression(context, expr->value.binary.right);
    if (right.kind == EXPR_RESULT_ERR) return EXPR_ERR;
    if (right.is_lvalue) right = get_rvalue(context, right);

    // Both operands must have scalar type
    if (!is_scalar_type(right.c_type)) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_LOGICAL_BINARY_EXPRESSION_OPERAND_TYPE,
            .location = expr->value.binary.left->span.start,
            .value.invalid_logical_binary_expression_operand_type = {
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

expression_result_t ir_visit_sizeof_expression(ir_gen_context_t *context, const expression_t *expr) {
    assert(expr != NULL && expr->kind == EXPRESSION_SIZEOF);
    const ir_type_t *type = get_ir_type(context, expr->value.sizeof_type);
    ssize_t size = ir_size_of_type_bytes(context->arch, type);
    const ir_value_t size_val = ir_make_const_int(context->arch->ptr_int_type, (long long) size);
    return (expression_result_t) {
        .addr_of = false,
        .c_type = c_ptr_uint_type(),
        .is_lvalue = false,
        .is_string_literal = false,
        .kind = EXPR_RESULT_VALUE,
        .value = size_val,
    };
}

expression_result_t ir_visit_ternary_expression(ir_gen_context_t *context, const expression_t *expr) {
    assert(context != NULL && "Context must not be NULL");
    assert(expr != NULL && "Expression must not be NULL");
    assert(expr->kind == EXPRESSION_TERNARY);

    expression_result_t condition = ir_visit_expression(context, expr->value.ternary.condition);
    if (condition.kind == EXPR_RESULT_ERR) return EXPR_ERR;
    if (condition.is_lvalue) condition = get_rvalue(context, condition);

    // The condition must have scalar type
    if (!is_scalar_type(condition.c_type)) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_TERNARY_CONDITION_TYPE,
            .location = expr->value.ternary.condition->span.start,
            .value.invalid_ternary_condition_type = {
                .type = condition.c_type,
            },
        });
        return EXPR_ERR;
    }

    const char* true_label = gen_label(context);
    const char* merge_label = gen_label(context);

    // Get the boolean value of the condition
    ir_value_t ir_condition = get_boolean_value(context, condition.value, condition.c_type, expr->value.ternary.condition).value;

    expression_result_t true_result;
    expression_result_t false_result;

    ir_instruction_node_t *true_branch_end = NULL;
    ir_instruction_node_t *false_branch_end = NULL;

    if (ir_condition.kind == IR_VALUE_CONST) {
        // Constant folding
        // Even though one of the branches will not be evaluated, we still need to visit it to perform semantic analysis
        // and to decide the type of the result. We will just throw away the generated code afterwards.

        if (ir_condition.constant.value.i != 0) {
            // Evaluate the true branch
            true_result = ir_visit_expression(context, expr->value.ternary.true_expression);
            if (true_result.kind == EXPR_RESULT_ERR) return EXPR_ERR;
            // Throw away the code for the false branch
            ir_instruction_node_t *position = ir_builder_get_position(context->builder);
            false_result = ir_visit_expression(context, expr->value.ternary.false_expression);
            ir_builder_clear_after(context->builder, position);
        } else {
            // Evaluate the false branch
            false_result = ir_visit_expression(context, expr->value.ternary.false_expression);
            if (false_result.kind == EXPR_RESULT_ERR) return EXPR_ERR;
            // Throw away the code for the true branch
            ir_instruction_node_t *position = ir_builder_get_position(context->builder);
            true_result = ir_visit_expression(context, expr->value.ternary.true_expression);
            ir_builder_clear_after(context->builder, position);
        }
    } else {
        // Branch based on the condition, falls through to the false branch
        ir_build_br_cond(context->builder, ir_condition, true_label);

        // False branch
        false_result = ir_visit_expression(context, expr->value.ternary.false_expression);
        if (false_result.kind == EXPR_RESULT_ERR) return EXPR_ERR;
        if (false_result.is_lvalue) false_result = get_rvalue(context, false_result);
        false_branch_end = ir_builder_get_position(context->builder);

        // True branch
        ir_build_nop(context->builder, true_label);
        true_result = ir_visit_expression(context, expr->value.ternary.true_expression);
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
                    .value.i = 0,
                },
            }
        };
    } else if (is_pointer_type(true_result.c_type) && is_pointer_type(false_result.c_type)) {
        // TODO: pointer compatibility checks
        // For now, we will just use the type of the first non void* pointer branch
        result_type = true_result.c_type->value.pointer.base->kind == TYPE_VOID ? false_result.c_type : true_result.c_type;
        ir_result_type = get_ir_type(context,result_type);
    } else {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_TERNARY_EXPRESSION_OPERANDS,
            .location = expr->value.ternary.condition->span.start, // TODO: use the '?' token position
            .value.invalid_ternary_expression_operands = {
                .true_type = true_result.c_type,
                .false_type = false_result.c_type,
            },
        });
        return EXPR_ERR;
    }

    if (ir_condition.kind == IR_VALUE_CONST) {
        // Constant folding
        expression_result_t result_expr;
        return ir_condition.constant.value.i != 0
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
    assert(expr->kind == EXPRESSION_UNARY);

    switch (expr->value.unary.operator) {
        case UNARY_BITWISE_NOT:
            return ir_visit_bitwise_not_unexpr(context, expr);
        case UNARY_LOGICAL_NOT:
            return ir_visit_logical_not_unexpr(context, expr);
        case UNARY_ADDRESS_OF:
            return ir_visit_address_of_unexpr(context, expr);
        case UNARY_DEREFERENCE:
            return ir_visit_indirection_unexpr(context, expr);
        case UNARY_SIZEOF:
            return ir_visit_sizeof_unexpr(context, expr);
        case UNARY_PRE_DECREMENT:
            return ir_visit_increment_decrement(context, expr, true, false);
        case UNARY_POST_DECREMENT:
            return ir_visit_increment_decrement(context, expr, false, false);
        case UNARY_PRE_INCREMENT:
            return ir_visit_increment_decrement(context, expr, true, true);
        case UNARY_POST_INCREMENT:
            return ir_visit_increment_decrement(context, expr, false, true);
        default:
            fprintf(stderr, "%s:%d:%d: Unary operator not implemented\n",
                expr->span.start.path, expr->span.start.line, expr->span.start.column);
            exit(1);
    }
}

expression_result_t ir_visit_bitwise_not_unexpr(ir_gen_context_t *context, const expression_t *expr) {
    assert(context != NULL && "Context must not be NULL");
    assert(expr != NULL && "Expression must not be NULL");
    assert(expr->kind == EXPRESSION_UNARY);

    expression_result_t operand = ir_visit_expression(context, expr->value.unary.operand);
    if (operand.kind == EXPR_RESULT_ERR) return EXPR_ERR;
    if (operand.is_lvalue) operand = get_rvalue(context, operand);

    if (!is_integer_type(operand.c_type)) {
        // The operand must have integer type
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_UNARY_ARITHMETIC_OPERATOR_TYPE,
            .location = expr->value.unary.operand->span.start,
            .value.invalid_unary_arithmetic_operator_type = {
                .type = operand.c_type,
                .operator = *expr->value.unary.token,
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
                .value.i = ~operand.value.constant.value.i,
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

expression_result_t ir_visit_logical_not_unexpr(ir_gen_context_t *context, const expression_t *expr) {
    assert(expr != NULL && expr->kind == EXPRESSION_UNARY);

    expression_result_t operand = ir_visit_expression(context, expr->value.unary.operand);
    if (operand.kind == EXPR_RESULT_ERR) return EXPR_ERR;
    if (operand.is_lvalue) operand = get_rvalue(context, operand);

    if (!is_scalar_type(operand.c_type)) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_UNARY_ARITHMETIC_OPERATOR_TYPE,
            .location = expr->value.unary.token->position,
            .value.invalid_unary_arithmetic_operator_type = {
                .operator = *expr->value.unary.token,
                .type = operand.c_type,
            }
        });
        return EXPR_ERR;
    }

    // The result has type int. It is 0 if the value of the operand compares unequal to 0, otherwise the result is 1.
    // The expression !expr is equivalent to (0 == expr).

    ir_value_t result;
    if (operand.value.kind == IR_VALUE_CONST) {
        // constant volding
        assert(operand.value.constant.kind == IR_CONST_INT);
        result = ir_make_const_int(context->arch->sint, operand.value.constant.value.i == 0 ? 1 : 0);
    } else {
        // Get a constant zero of the same type as the operand
        ir_value_t zero = ir_get_zero_value(context, ir_get_type_of_value(operand.value));

        // Compare to 0
        ir_var_t comparisson_result = temp_var(context, &IR_BOOL);
        ir_build_eq(context->builder, operand.value, zero, comparisson_result);

        // Extend the result to an int, as a boolean is just a 1-bit integer
        ir_var_t int_result = temp_var(context, context->arch->sint);
        ir_build_ext(context->builder, ir_value_for_var(comparisson_result), int_result);
        result = ir_value_for_var(int_result);
    }

    return (expression_result_t) {
        .addr_of = false,
        .c_type = &INT,
        .is_lvalue = false,
        .is_string_literal = false,
        .kind = EXPR_RESULT_VALUE,
        .value = result,
    };
}

expression_result_t ir_visit_address_of_unexpr(ir_gen_context_t *context, const expression_t *expr) {
    // The operand of the unary address of ('&') operator must be one of:
    // 1. A function designator
    // 2. The result of a [] or * operator
    // 3. A lvalue that designates an object that is not a bit-field and does not have the 'register' storage-class specifier

    expression_result_t operand = ir_visit_expression(context, expr->value.unary.operand);
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
        exit(1);
    }
}

expression_result_t ir_visit_indirection_unexpr(ir_gen_context_t *context, const expression_t *expr) {
    expression_result_t operand = ir_visit_expression(context, expr->value.unary.operand);
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
    if (operand.c_type->value.pointer.base->kind == TYPE_FUNCTION) {
        // TODO: dereference function pointers
        assert(false && "De-referencing function pointers not implemented");
    } else {
        expression_result_t *inner = malloc(sizeof(expression_result_t));
        *inner = operand;

        return (expression_result_t) {
            .kind = EXPR_RESULT_INDIRECTION,
            .c_type = operand.c_type->value.pointer.base,
            .is_lvalue = true,
            .is_string_literal = false,
            .addr_of = false,
            .indirection_inner = inner,
        };
    }
}

expression_result_t ir_visit_sizeof_unexpr(ir_gen_context_t *context, const expression_t *expr) {
    assert(expr != NULL && expr->kind == EXPRESSION_UNARY);
    // TODO: error if sizeof is applied to an expression that designates a bit-field member
    expression_result_t operand = ir_visit_expression(context, expr->value.unary.operand);
    if (operand.kind == EXPR_RESULT_ERR) return EXPR_ERR;
    const ir_type_t *ir_type = ir_get_type_of_value(operand.value);
    if (operand.is_lvalue) ir_type = ir_type->value.ptr.pointee;
    ssize_t size = ir_size_of_type_bytes(context->arch, ir_type);
    ir_value_t size_val = ir_make_const_int(ir_ptr_int_type(context), (long long) size);
    return (expression_result_t) {
        .kind = EXPR_RESULT_VALUE,
        .addr_of = false,
        .c_type = c_ptr_uint_type(),
        .is_lvalue = false,
        .is_string_literal = false,
        .value = size_val,
    };
}

expression_result_t ir_visit_increment_decrement(ir_gen_context_t *context, const expression_t *expr, bool pre, bool increment) {
    assert(expr != NULL && expr->kind == EXPRESSION_UNARY);

    expression_result_t lvalue = ir_visit_expression(context, expr->value.unary.operand);
    if (lvalue.kind == EXPR_RESULT_ERR) return EXPR_ERR;
    if (!lvalue.is_lvalue || lvalue.kind != EXPR_RESULT_VALUE) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_ASSIGNMENT_TARGET,
            .location = expr->value.unary.token->position,
        });
        return EXPR_ERR;
    }

    expression_result_t rvalue = get_rvalue(context, lvalue);
    if (rvalue.kind == EXPR_RESULT_ERR) return EXPR_ERR;

    // The semantics of the increment/decrement operators are similar to the additive operators.
    // The operand must have an arithmetic or pointer type.
    // TODO: this should also work for enums (?) when those get implemented
    if (!is_arithmetic_type(rvalue.c_type) && !is_pointer_type(rvalue.c_type)) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_CANNOT_INCREMENT_DECREMENT_TYPE,
            .location = expr->value.unary.token->position,
            .value.cannot_increment_decrement_type = {
                .type = rvalue.c_type,
            },
        });
        return EXPR_ERR;
    }

    const ir_type_t *ir_type = rvalue.value.var.type;
    ir_var_t post_value = temp_var(context, ir_type);
    if (is_integer_type(rvalue.c_type)) {
        ir_value_t rhs = ir_make_const_int(ir_type, 1);
        if (increment) ir_build_add(context->builder, rvalue.value, rhs, post_value);
        else ir_build_sub(context->builder, rvalue.value, rhs, post_value);
    } else if (is_floating_type(rvalue.c_type)) {
        ir_value_t rhs = ir_make_const_float(ir_type, 1.0);
        if (increment) ir_build_add(context->builder, rvalue.value, rhs, post_value);
        else ir_build_sub(context->builder, rvalue.value, rhs, post_value);
    } else /* has pointer type */ {
        ir_build_get_array_element_ptr(context->builder, rvalue.value, ir_make_const_int(&IR_I32, increment ? 1 : -1), post_value);
    }

    ir_build_store(context->builder, lvalue.value, ir_value_for_var(post_value));

    return (expression_result_t) {
        .addr_of = false,
        .c_type = lvalue.c_type,
        .is_lvalue = false,
        .is_string_literal = false,
        .kind = EXPR_RESULT_VALUE,
        .value = pre ? ir_value_for_var(post_value) : rvalue.value,
    };
}

expression_result_t ir_visit_member_access_expression(ir_gen_context_t *context, const expression_t *expr) {
    assert(expr != NULL && expr->kind == EXPRESSION_MEMBER_ACCESS);
    assert(expr->value.member_access.operator.kind == TK_ARROW || expr->value.member_access.operator.kind == TK_DOT);

    expression_result_t target = ir_visit_expression(context, expr->value.member_access.struct_or_union);
    if (target.kind == EXPR_RESULT_ERR) return EXPR_ERR;

    // The target must be a struct or a pointer to a struct
    if (expr->value.member_access.operator.kind == TK_ARROW &&
        (target.c_type->kind != TYPE_POINTER || target.c_type->value.pointer.base->kind != TYPE_STRUCT_OR_UNION)) {
        // If the operator is '->', then the type of the target must be a pointer to a struct.
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_MEMBER_ACCESS_TARGET,
            .location = expr->value.member_access.operator.position,
            .value.invalid_member_access_target = {
                .type = target.c_type,
                .operator = expr->value.member_access.operator
            }
        });
        return EXPR_ERR;
    } else if (expr->value.member_access.operator.kind == TK_DOT && target.c_type->kind != TYPE_STRUCT_OR_UNION) {
        // If the operator is '.', then the type of the target must be a struct.
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_MEMBER_ACCESS_TARGET,
            .location = expr->value.member_access.operator.position,
            .value.invalid_member_access_target = {
                .type = target.c_type,
                .operator = expr->value.member_access.operator
            }
        });
        return EXPR_ERR;
    }

    // If the target is a pointer, we need to dereference it to get the base pointer
    if (target.c_type->kind == TYPE_POINTER) {
        target = get_rvalue(context, target);
    }

    ir_value_t base_ptr;
    if (target.kind == EXPR_RESULT_VALUE) {
        assert(ir_get_type_of_value(target.value)->kind == IR_TYPE_PTR);
        base_ptr = target.value;
    } else {
        base_ptr = get_indirect_ptr(context, target);
    }

    const ir_type_t *struct_type = ir_get_type_of_value(base_ptr)->value.ptr.pointee;
    const tag_t *tag = lookup_tag_by_uid(context, struct_type->value.struct_or_union.id);
    assert(tag != NULL);

    // Look up the field in the struct definition to find its index
    const ir_struct_field_t *ir_field = NULL;
    hash_table_lookup(&struct_type->value.struct_or_union.field_map, expr->value.member_access.member.value, (void**) &ir_field);
    if (ir_field == NULL) {
        append_compilation_error(&context->errors, (compilation_error_t) {
            .kind = ERR_INVALID_STRUCT_FIELD_REFERENCE,
            .location = expr->value.member_access.operator.position,
            .value.invalid_struct_field_reference = {
                .type = target.c_type,
                .field = expr->value.member_access.member
            }
        });
        return EXPR_ERR;
    }

    // Lookup the field in the c type (guaranteed to exist if its in the corresponding ir struct type).
    // Note that if padding was added between fields, the field indexes will not be equal, and we will have to find the
    // field with a matching identifier.
    const type_t *c_struct_type = tag->c_type;
    const struct_field_t *c_field = NULL;
    for (int i = 0; i < c_struct_type->value.struct_or_union.fields.size; i += 1) {
        if (strcmp(ir_field->name, c_struct_type->value.struct_or_union.fields.buffer[i]->identifier->value) == 0) {
            c_field = c_struct_type->value.struct_or_union.fields.buffer[i];
            break;
        }
    }
    assert(c_field != NULL);

    const ir_var_t result = temp_var(context, get_ir_ptr_type(ir_field->type));
    ir_build_get_struct_member_ptr(context->builder, target.value, ir_field->index, result);

    return (expression_result_t) {
        .kind = EXPR_RESULT_VALUE,
        .c_type = c_field->type,
        .is_lvalue = true,
        .is_string_literal = false,
        .addr_of = false,
        .value = ir_value_for_var(result),
    };
}

expression_result_t ir_visit_compound_literal(ir_gen_context_t *context, const expression_t *expr) {
    assert(expr->kind == EXPRESSION_COMPOUND_LITERAL);

    const type_t *type = expr->value.compound_literal.type;

    // TODO: type check

    const tag_t *tag = lookup_tag(context, type->value.struct_or_union.identifier->value);
    type = tag->c_type;

    // Create a stack slot to store the result temporarily because we don't know where this is being stored
    const ir_type_t *ir_type = tag->ir_type;
    ir_var_t res = temp_var(context, get_ir_ptr_type(ir_type));
    insert_alloca(context, ir_type, res);

    ir_visit_initializer_list(context, ir_value_for_var(res), type, &expr->value.compound_literal.initializer_list);

    return (expression_result_t) {
        .kind = EXPR_RESULT_VALUE,
        .value = ir_value_for_var(res),
        .c_type = type,
        .is_lvalue = true,
        .symbol = NULL,
        .is_string_literal = false,
    };
}

expression_result_t ir_visit_primary_expression(ir_gen_context_t *context, const expression_t *expr) {
    assert(context != NULL && "Context must not be NULL");
    assert(expr != NULL && "Primary expression must not be NULL");
    assert(expr->kind == EXPRESSION_PRIMARY);

    switch (expr->value.primary.kind) {
        case PE_IDENTIFIER: {
            symbol_t *symbol = lookup_symbol(context, expr->value.primary.value.token.value);
            if (symbol == NULL) {
                source_position_t pos = expr->value.primary.value.token.position;
                append_compilation_error(&context->errors, (compilation_error_t) {
                    .kind = ERR_USE_OF_UNDECLARED_IDENTIFIER,
                    .location = pos,
                    .value.use_of_undeclared_identifier = {
                        .identifier = expr->value.primary.value.token.value,
                    },
                });
                return EXPR_ERR;
            }

            if (symbol->kind == SYMBOL_ENUMERATION_CONSTANT) {
                // some symbols don't actually represent a variable and have no address
                return (expression_result_t) {
                    .kind = EXPR_RESULT_VALUE,
                    .symbol = symbol,
                    .value = (ir_value_t) {
                        .kind = IR_VALUE_CONST,
                        .constant = symbol->const_value,
                    },
                    .c_type = symbol->c_type,
                    .is_lvalue = false,
                    .is_string_literal = false,
                    .addr_of = false,
                };
            } else {
                // others just represent an address in the data segment or on the stack
                return (expression_result_t) {
                    .kind = EXPR_RESULT_VALUE,
                    .c_type = symbol->c_type,
                    .is_lvalue = true,
                    .is_string_literal = false,
                    .addr_of = false,
                    .symbol = symbol,
                    .value = (ir_value_t) {
                        .kind = IR_VALUE_VAR,
                        .var = symbol->ir_ptr,
                    },
                };
            }
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
            char *literal = replace_escape_sequences(expr->value.primary.value.token.value);

            // Maybe there should be a special expression node type for static lengths?
            expression_t *array_length_expr = malloc(sizeof(expression_t));
            *array_length_expr = (expression_t) {
                .kind = EXPRESSION_PRIMARY,
                .value.primary = {
                    .kind = PE_CONSTANT,
                    .value.token = {
                        .kind = TK_INTEGER_CONSTANT,
                        .value = malloc(32),
                        .position = expr->value.primary.value.token.position,
                    },
                },
            };
            char *val = malloc(32);
            snprintf(val, 32, "%zu", strlen(literal) + 1);
            array_length_expr->value.primary.value.token.value = val;

            // The C type is an array of characters
            type_t *c_type = malloc(sizeof(type_t));
            *c_type = (type_t) {
                .kind = TYPE_ARRAY,
                .value.array = {
                    .element_type = &CHAR,
                    .size = array_length_expr,
                },
            };

            ir_type_t *ir_type = malloc(sizeof(ir_type_t));
            *ir_type = (ir_type_t) {
                .kind = IR_TYPE_ARRAY,
                .value.array = {
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
                    .value.s = literal,
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
            return ir_visit_expression(context, expr->value.primary.value.expression);
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
    assert(expr->kind == EXPRESSION_PRIMARY && expr->value.primary.kind == PE_CONSTANT);
    assert(expr->value.primary.value.token.value != NULL && "Token value must not be NULL");

    switch (expr->value.primary.value.token.kind) {
        case TK_CHAR_LITERAL: {
            // TODO: Handle escape sequences, wide character literals.
            char c = expr->value.primary.value.token.value[0];
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
                        .value.i = (int)c,
                    }
                }
            };
        }
        case TK_INTEGER_CONSTANT: {
            unsigned long long value;
            const type_t *c_type;
            decode_integer_constant(&expr->value.primary.value.token, &value, &c_type);
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
                            .value.i = value,
                    }
                }
            };
        }
        case TK_FLOATING_CONSTANT: {
            long double value;
            const type_t *c_type;
            decode_float_constant(&expr->value.primary.value.token, &value, &c_type);
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
                        .value.f = value,
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

const type_t *c_ptr_uint_type(void) {
    // TODO: arch dependent
    return &UNSIGNED_LONG;
}

const ir_type_t* get_ir_type(ir_gen_context_t *context, const type_t *c_type) {
    assert(c_type != NULL && "C type must not be NULL");

    switch (c_type->kind) {
        case TYPE_INTEGER: {
            if (c_type->value.integer.is_signed) {
                switch (c_type->value.integer.size) {
                    case INTEGER_TYPE_BOOL:
                        return &IR_BOOL;
                    case INTEGER_TYPE_CHAR:
                        return context->arch->schar;
                    case INTEGER_TYPE_SHORT:
                        return context->arch->sshort;
                    case INTEGER_TYPE_INT:
                        return context->arch->sint;
                    case INTEGER_TYPE_LONG:
                        return context->arch->slong;
                    case INTEGER_TYPE_LONG_LONG:
                        return context->arch->slonglong;
                    default:
                        return context->arch->sint;
                }
            } else {
                switch (c_type->value.integer.size) {
                    case INTEGER_TYPE_BOOL:
                        return &IR_BOOL;
                    case INTEGER_TYPE_CHAR:
                        return context->arch->uchar;
                    case INTEGER_TYPE_SHORT:
                        return context->arch->ushort;
                    case INTEGER_TYPE_INT:
                        return context->arch->uint;
                    case INTEGER_TYPE_LONG:
                        return context->arch->ulong;
                    case INTEGER_TYPE_LONG_LONG:
                        return context->arch->ulonglong;
                    default:
                        return context->arch->uint;
                }
            }
        }
        case TYPE_FLOATING: {
            switch (c_type->value.floating) {
                case FLOAT_TYPE_FLOAT:
                    return context->arch->_float;
                case FLOAT_TYPE_DOUBLE:
                    return context->arch->_double;
                case FLOAT_TYPE_LONG_DOUBLE:
                    return context->arch->_long_double;
                default:
                    return context->arch->_double;
            }
        }
        case TYPE_POINTER: {
            const ir_type_t *pointee = get_ir_type(context, c_type->value.pointer.base);
            ir_type_t *ir_type = malloc(sizeof(ir_type_t));
            *ir_type = (ir_type_t) {
                .kind = IR_TYPE_PTR,
                .value.ptr = {
                    .pointee = pointee,
                },
            };
            return ir_type;
        }
        case TYPE_FUNCTION: {
            const ir_type_t *ir_return_type = get_ir_type(context, c_type->value.function.return_type);
            const ir_type_t **ir_param_types = malloc(c_type->value.function.parameter_list->length * sizeof(ir_type_t*));
            for (size_t i = 0; i < c_type->value.function.parameter_list->length; i++) {
                const parameter_declaration_t *param = c_type->value.function.parameter_list->parameters[i];
                const ir_type_t *ir_type = get_ir_type(context,param->type);
                ir_param_types[i] = ir_type->kind == IR_TYPE_ARRAY
                    ? get_ir_ptr_type(ir_type->value.array.element) // array to pointer decay
                    : ir_type;
            }
            ir_type_t *ir_type = malloc(sizeof(ir_type_t));
            *ir_type = (ir_type_t) {
                .kind = IR_TYPE_FUNCTION,
                .value.function = {
                    .return_type = ir_return_type,
                    .params = ir_param_types,
                    .num_params = c_type->value.function.parameter_list->length,
                    .is_variadic = c_type->value.function.parameter_list->variadic
                },
            };
            return ir_type;
        }
        case TYPE_ARRAY: {
            const ir_type_t *element_type = get_ir_type(context,c_type->value.array.element_type);
            size_t length = 0;
            if (c_type->value.array.size != NULL) {
                expression_result_t array_len = ir_visit_expression(context, c_type->value.array.size);
                if (array_len.kind == EXPR_RESULT_ERR) assert(false && "Invalid array size"); // TODO: handle error
                if (array_len.is_lvalue) array_len = get_rvalue(context, array_len);
                ir_value_t length_val = array_len.value;
                if (length_val.kind != IR_VALUE_CONST) {
                    // TODO: handle non-constant array sizes
                    assert(false && "Non-constant array sizes not implemented");
                }
                length = length_val.constant.value.i;
            }

            ir_type_t *ir_type = malloc(sizeof(ir_type_t));
            *ir_type = (ir_type_t) {
                .kind = IR_TYPE_ARRAY,
                .value.array = {
                    .element = element_type,
                    .length = length,
                }
            };
            return ir_type;
        }
        case TYPE_STRUCT_OR_UNION: {
            // This is only for looking up existing struct types, creating a new one should be done through
            // the function get_ir_struct_type
            const tag_t *tag = lookup_tag(context, c_type->value.struct_or_union.identifier->value);
            if (tag == NULL) {
                // Any valid declaration that declares a struct also creates the tag (for example: `struct Foo *foo`)
                // If the tag isn't valid here, then there was some other error earlier in the program
                // We will just return a default type
                return &IR_I32;
            }
            return tag->ir_type;
        }
        default:
            return &IR_VOID;
    }
}

// This should only be called when creating the declaration/tag
const ir_type_t *get_ir_struct_type(ir_gen_context_t *context, const type_t *c_type, const char *id) {
    assert(c_type != NULL && c_type->kind == TYPE_STRUCT_OR_UNION);

    // map of field name -> field ptr
    hash_table_t field_map = hash_table_create_string_keys(32);

    // get field list
    ir_struct_field_ptr_vector_t fields = VEC_INIT;
    for (size_t i = 0; i < c_type->value.struct_or_union.fields.size; i++) {
        const struct_field_t *c_field = c_type->value.struct_or_union.fields.buffer[i];
        assert(c_field->index == i); // assuming they're in order
        ir_struct_field_t *ir_field = malloc(sizeof(ir_struct_field_t));
        const ir_type_t *ir_field_type = NULL;
        ir_field_type = get_ir_type(context, c_field->type);

        *ir_field = (ir_struct_field_t) {
            .name = c_field->identifier->value,
            .type = ir_field_type,
            .index = c_field->index,
        };
        hash_table_insert(&field_map, ir_field->name, ir_field);
        VEC_APPEND(&fields, ir_field);
    }

    ir_type_struct_t definition = {
        .id = id,
        .fields = fields,
        .field_map = field_map,
        .is_union = c_type->value.struct_or_union.is_union,
    };
    if (!c_type->value.struct_or_union.packed && !c_type->value.struct_or_union.is_union) definition = ir_pad_struct(context->arch, &definition);

    ir_type_t *ir_type = malloc(sizeof(ir_type_t));
    *ir_type = (ir_type_t) {
        .kind = IR_TYPE_STRUCT_OR_UNION,
        .value.struct_or_union = definition,
    };
    return ir_type;
}

const ir_type_t *get_ir_ptr_type(const ir_type_t *pointee) {
    // TODO: cache these?
    ir_type_t *ir_type = malloc(sizeof(ir_type_t));
    *ir_type = (ir_type_t) {
        .kind = IR_TYPE_PTR,
        .value.ptr = {
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
                .value.i = 0,
            }
        };
    } else if (ir_is_float_type(type)) {
        return (ir_value_t) {
            .kind = IR_VALUE_CONST,
            .constant = (ir_const_t) {
                .kind = IR_CONST_FLOAT,
                .type = type,
                .value.f = 0.0,
            }
        };
    } else if (type->kind == IR_TYPE_PTR) {
        ir_value_t zero = ir_get_zero_value(context, get_ir_type(context,c_ptr_uint_type()));
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
            .value.invalid_conversion_to_boolean = {
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
            .value.i =  ir_is_float_type(ir_type) ? value.constant.value.f != 0.0 : value.constant.value.i != 0,
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
                        .value.i = value.constant.value.i,
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
            if (ir_size_of_type_bits(context->arch, source_type) > ir_size_of_type_bits(context->arch, result_type)) {
                // Truncate
                ir_build_trunc(context->builder, value, result);
            } else if (ir_size_of_type_bits(context->arch, source_type) < ir_size_of_type_bits(context->arch, result_type)) {
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
                        .value.i = (long long)value.constant.value.f,
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
                        .value.i = value.constant.value.i,
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
                        .value.f = value.constant.value.f,
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
            if (ir_size_of_type_bits(context->arch, source_type) > ir_size_of_type_bits(context->arch, result_type)) {
                // Truncate
                ir_build_trunc(context->builder, value, result);
            } else if (ir_size_of_type_bits(context->arch, source_type) < ir_size_of_type_bits(context->arch, result_type)) {
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
                        .value.f = (double)value.constant.value.i,
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
                        .value.i = value.constant.value.i,
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
                        .value.i = value.constant.value.i,
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
            if (ir_size_of_type_bits(context->arch, source_type) < ir_size_of_type_bits(context->arch, get_ir_type(context, c_ptr_uint_type()))) {
                ir_var_t temp = temp_var(context, get_ir_type(context,c_ptr_uint_type()));
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

    // We may need to load the value from a pointer.
    // However, there may be multiple levels of indirection, eachrequiring a load.
    expression_result_t *e = &res;
    int indirection_level = 0;
    do {
        assert(e->indirection_inner != NULL);
        e = e->indirection_inner;
        indirection_level += 1;
    } while (e->kind == EXPR_RESULT_INDIRECTION);

    if (!e->is_lvalue) {
        // value has already been loaded
        indirection_level -= 1;
    }

    // Starting at the base pointer, repeatedly load the new pointer
    ir_value_t ptr = e->value;
    for (int i = 0; i < indirection_level; i += 1) {
        ir_var_t temp = temp_var(context, ir_get_type_of_value(ptr)->value.ptr.pointee);
        ir_build_load(context->builder, ptr, temp);
        ptr = ir_value_for_var(temp);
    }

    return ptr;
}

expression_result_t get_rvalue(ir_gen_context_t *context, expression_result_t res) {
    assert(res.is_lvalue && "Expected lvalue");
    if (res.kind == EXPR_RESULT_VALUE) {
        assert(ir_get_type_of_value(res.value)->kind == IR_TYPE_PTR && "Expected pointer type");
        if (res.symbol != NULL && res.symbol->c_type->is_const && res.symbol->has_const_value) {
            // TODO: not quite sure this is correct for const pointers (e.g. `const int *foo = bar`)
            // This value is a compile time constant. Use the constant value instead of loading from memory.
            return (expression_result_t) {
                .kind = EXPR_RESULT_VALUE,
                .c_type = res.c_type,
                .is_lvalue = false,
                .is_string_literal = false,
                .addr_of = false,
                .value = (ir_value_t) {
                    .kind = IR_VALUE_CONST,
                    .constant = res.symbol->const_value,
                },
            };
        }

        ir_var_t temp = temp_var(context, ir_get_type_of_value(res.value)->value.ptr.pointee);
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
        ir_var_t result = temp_var(context, ir_get_type_of_value(ptr)->value.ptr.pointee);
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

ir_instruction_node_t *insert_alloca(ir_gen_context_t *context, const ir_type_t *ir_type, ir_var_t result) {
    // save the current position of the builder
    ir_instruction_node_t *position = ir_builder_get_position(context->builder);
    bool should_restore = position != NULL && position != context->alloca_tail;

    ir_builder_position_after(context->builder, context->alloca_tail);
    ir_instruction_node_t *alloca_node = ir_build_alloca(context->builder, ir_type, result);
    context->alloca_tail = alloca_node;

    // restore the builder position
    if (should_restore) {
        ir_builder_position_after(context->builder, position);
    }

    return alloca_node;
}

const ir_type_t *ir_ptr_int_type(const ir_gen_context_t *context) {
    return context->arch->ptr_int_type;
}

ir_value_t ir_make_const_int(const ir_type_t *type, long long value) {
    return (ir_value_t) {
        .kind = IR_VALUE_CONST,
        .constant = {
            .kind = IR_CONST_INT,
            .type = type,
            .value.i = value,
        }
    };
}

ir_value_t ir_make_const_float(const ir_type_t *type, double value) {
    return (ir_value_t) {
        .kind = IR_VALUE_CONST,
        .constant = {
            .kind = IR_CONST_FLOAT,
            .type = type,
            .value.f = value,
        },
    };
}
