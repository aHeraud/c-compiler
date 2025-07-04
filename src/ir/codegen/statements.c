#include "errors.h"
#include "ir/codegen/internal.h"

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
        if (return_type->kind != IR_TYPE_VOID) {
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