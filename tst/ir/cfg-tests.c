#include <CUnit/Basic.h>

#include "ir/ir.h"
#include "ir/ir-builder.h"
#include "ir/cfg.h"

void test_cfg_create_basic() {
    // Create a basic cfg that has a single block
    ir_function_builder_t *builder = ir_builder_create();
    ir_build_ret_void(builder);
    ir_instruction_vector_t body = ir_builder_finalize(builder);

    ir_function_definition_t function = {
        .name = "main",
        .type = & (ir_type_t) {
            .kind = IR_TYPE_FUNCTION,
            .function = {
                .return_type = & (ir_type_t) {
                    .kind = IR_TYPE_VOID,
                },
                .params = NULL,
                .num_params = 0,
                .is_variadic = false,
            },
        },
        .params = NULL,
        .num_params = 0,
        .is_variadic = false,
        .body = body,
    };

    ir_control_flow_graph_t cfg = ir_create_control_flow_graph(&function);
    CU_ASSERT_PTR_NOT_NULL_FATAL(cfg.entry);
    CU_ASSERT_EQUAL_FATAL(cfg.basic_blocks.size, 1);
    CU_ASSERT_PTR_EQUAL_FATAL(cfg.basic_blocks.buffer[0], cfg.entry);
    CU_ASSERT_EQUAL_FATAL(cfg.entry->id, 0);
    CU_ASSERT_TRUE_FATAL(cfg.entry->instructions.size == 1);
    CU_ASSERT_EQUAL_FATAL(cfg.entry->instructions.buffer[0]->opcode, IR_RET);
}

void test_cfg_create_if_else() {
    // Create a basic cfg that has a single block
    ir_var_t cond = {
        .name = "a",
        .type = & (ir_type_t) {
            .kind = IR_TYPE_BOOL,
        },
    };

    ir_function_builder_t *builder = ir_builder_create();
    ir_build_br_cond(builder, (ir_value_t) { .kind = IR_VALUE_VAR, .var = cond }, "l0");
    ir_build_ret(builder, (ir_value_t) { .kind = IR_VALUE_CONST, .constant = { .kind = IR_CONST_INT, .i = 1 } });
    ir_build_nop(builder, "l0");
    ir_build_ret(builder, (ir_value_t) { .kind = IR_VALUE_CONST, .constant = { .kind = IR_CONST_INT, .i = 0 } });
    ir_instruction_vector_t body = ir_builder_finalize(builder);

    ir_function_definition_t function = {
        .name = "main",
        .type = & (ir_type_t) {
            .kind = IR_TYPE_FUNCTION,
            .function = {
                .return_type = & (ir_type_t) {
                    .kind = IR_TYPE_VOID,
                },
                .params = (const ir_type_t*[]) { &IR_BOOL },
                .num_params = 1,
                .is_variadic = false,
            },
        },
        .params = (ir_var_t[]) { cond },
        .num_params = 1,
        .is_variadic = false,
        .body = body,
    };

    ir_control_flow_graph_t cfg = ir_create_control_flow_graph(&function);
    CU_ASSERT_PTR_NOT_NULL_FATAL(cfg.entry);
    CU_ASSERT_EQUAL_FATAL(cfg.basic_blocks.size, 3);

    CU_ASSERT_EQUAL_FATAL(cfg.entry->instructions.size, 1);
    CU_ASSERT_EQUAL_FATAL(cfg.entry->instructions.buffer[0]->opcode, IR_BR_COND);
}

void test_cfg_prune() {
    // Create a basic cfg that has a single block
    ir_var_t cond = {
        .name = "a",
        .type = & (ir_type_t) {
            .kind = IR_TYPE_BOOL,
        },
    };

    ir_function_builder_t *builder = ir_builder_create();
    ir_build_br_cond(builder, (ir_value_t) { .kind = IR_VALUE_VAR, .var = cond }, "l0");
    ir_build_ret(builder, (ir_value_t) { .kind = IR_VALUE_CONST, .constant = { .kind = IR_CONST_INT, .type = &IR_I32, .i = 1 } });
    ir_build_br(builder, "l1"); // never reached, since the previous instruction doesn't fall through
    ir_build_nop(builder, "l0");
    ir_build_ret(builder, (ir_value_t) { .kind = IR_VALUE_CONST, .constant = { .kind = IR_CONST_INT, .type = &IR_I32, .i = 0 } });

    // never reached
    ir_build_nop(builder, "l1");
    ir_build_ret(builder, (ir_value_t) { .kind = IR_VALUE_CONST, .constant = { .kind = IR_CONST_INT, .type = &IR_I32, .i = 1 } });

    ir_instruction_vector_t body = ir_builder_finalize(builder);

    ir_function_definition_t function = {
        .name = "main",
        .type = & (ir_type_t) {
            .kind = IR_TYPE_FUNCTION,
            .function = {
                .return_type = & (ir_type_t) {
                    .kind = IR_TYPE_VOID,
                },
                .params = (const ir_type_t*[]) { &IR_BOOL },
                .num_params = 1,
                .is_variadic = false,
            },
        },
        .params = (ir_var_t[]) { cond },
        .num_params = 1,
        .is_variadic = false,
        .body = body,
    };

    ir_control_flow_graph_t cfg = ir_create_control_flow_graph(&function);
    //ir_print_control_flow_graph(stdout, &cfg, 1);
    CU_ASSERT_EQUAL_FATAL(cfg.basic_blocks.size, 5);

    ir_prune_control_flow_graph(&cfg);
    //ir_print_control_flow_graph(stdout, &cfg, 1);

    CU_ASSERT_EQUAL_FATAL(cfg.basic_blocks.size, 3);
}

void test_cfg_linearize() {
    ir_var_t cond = {
        .name = "a",
        .type = & (ir_type_t) {
            .kind = IR_TYPE_BOOL,
        },
    };

    ir_function_builder_t *builder = ir_builder_create();
    ir_build_br_cond(builder, (ir_value_t) { .kind = IR_VALUE_VAR, .var = cond }, "l0");
    ir_build_ret(builder, (ir_value_t) { .kind = IR_VALUE_CONST, .constant = { .kind = IR_CONST_INT, .i = 1 } });
    ir_build_nop(builder, "l0");
    ir_build_ret(builder, (ir_value_t) { .kind = IR_VALUE_CONST, .constant = { .kind = IR_CONST_INT, .i = 0 } });
    ir_instruction_vector_t body = ir_builder_finalize(builder);

    ir_function_definition_t function = {
        .name = "main",
        .type = & (ir_type_t) {
            .kind = IR_TYPE_FUNCTION,
            .function = {
                .return_type = & (ir_type_t) {
                    .kind = IR_TYPE_VOID,
                },
                .params = (const ir_type_t*[]) { &IR_BOOL },
                .num_params = 1,
                .is_variadic = false,
            },
        },
        .params = (ir_var_t[]) { cond },
        .num_params = 1,
        .is_variadic = false,
        .body = body,
    };

    ir_control_flow_graph_t cfg = ir_create_control_flow_graph(&function);
    CU_ASSERT_PTR_NOT_NULL_FATAL(cfg.entry);
    CU_ASSERT_EQUAL_FATAL(cfg.basic_blocks.size, 3);

    CU_ASSERT_EQUAL_FATAL(cfg.entry->instructions.size, 1);
    CU_ASSERT_EQUAL_FATAL(cfg.entry->instructions.buffer[0]->opcode, IR_BR_COND);

    ir_instruction_vector_t instrs =ir_linearize_cfg(&cfg);
    CU_ASSERT_EQUAL_FATAL(instrs.size, 4);
    CU_ASSERT_EQUAL_FATAL(instrs.buffer[0].opcode, IR_BR_COND);
    CU_ASSERT_EQUAL_FATAL(instrs.buffer[1].opcode, IR_RET);
    CU_ASSERT_EQUAL_FATAL(instrs.buffer[2].opcode, IR_NOP);
    CU_ASSERT_EQUAL_FATAL(instrs.buffer[3].opcode, IR_RET);
}

int cfg_tests_init_suite() {
    CU_pSuite suite = CU_add_suite("IR Generation Tests", NULL, NULL);
    if (suite == NULL) {
        return CU_get_error();
    }

    CU_add_test(suite, "create - basic", test_cfg_create_basic);
    CU_add_test(suite, "create - if else", test_cfg_create_if_else);
    CU_add_test(suite, "prune - if else", test_cfg_prune);
    CU_add_test(suite, "linearize - if else", test_cfg_linearize);
    return CUE_SUCCESS;
}