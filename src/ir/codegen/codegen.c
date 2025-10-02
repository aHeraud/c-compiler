/// A module for performing semantic analysis and IR generation from an input AST. Semantic analysis and IR generation
/// are combined into a single traversal of the AST.

#include "errors.h"
#include "ir/cfg.h"
#include "ir/codegen/codegen.h"
#include "ir/codegen/internal.h"
#include "ir/fmt.h"

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

    // Topological sort of global definitions
    ir_sort_global_definitions(context.module);

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
            .ir_ptr = ir_value_for_var((ir_var_t) {
                .name = function->identifier->value,
                .type = function_type,
            }),
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
            .ir_ptr = ir_value_for_var(param_ptr),
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
        char temp[1024];
        ir_fmt_type(temp, 1024, context->function->type);
        fprintf(stderr, "IR validation error in function %s %s\n", function->identifier->value, temp);
        ir_fmt_instr(temp, 1024, errors.buffer[0].instruction);
        fprintf(stderr, "At instruction: %s\n", temp);
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