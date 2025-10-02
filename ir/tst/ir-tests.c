#include <CUnit/Basic.h>
#include <stdlib.h>
#include <string.h>

#include "ir/ir.h"

ir_global_t* make_global(const char* name, bool initialized, ir_const_t value) {
    ir_global_t *global = (ir_global_t*) malloc(sizeof(ir_global_t));
    *global = (ir_global_t) {
        .name = name,
        .type = NULL,
        .initialized = initialized,
        .value = value,
    };
    return global;
}

void test_ir_sort_globals_linear_chain(void) {
    // c; b -> c; a -> b
    ir_module_t module = {
        .name = "m",
        .arch = NULL,
        .globals = VEC_INIT,
        .type_map = hash_table_create_string_keys(16),
        .functions = { .buffer = NULL, .size = 0, .capacity = 0 },
    };

    ir_const_t a_val = { .kind = IR_CONST_GLOBAL_POINTER, .type = NULL, .value.global_name = "b" };
    ir_const_t b_val = { .kind = IR_CONST_GLOBAL_POINTER, .type = NULL, .value.global_name = "c" };
    ir_const_t c_val = { .kind = IR_CONST_INT, .type = NULL, .value.i = 0 };

    VEC_APPEND(&module.globals, make_global("a", true, a_val));
    VEC_APPEND(&module.globals, make_global("b", true, b_val));
    VEC_APPEND(&module.globals, make_global("c", true, c_val));

    ir_sort_global_definitions(&module);

    CU_ASSERT_EQUAL(module.globals.size, 3);
    CU_ASSERT_STRING_EQUAL(module.globals.buffer[0]->name, "c");
    CU_ASSERT_STRING_EQUAL(module.globals.buffer[1]->name, "b");
    CU_ASSERT_STRING_EQUAL(module.globals.buffer[2]->name, "a");

    // cleanup
    for (size_t i = 0; i < module.globals.size; i++) free(module.globals.buffer[i]);
    if (module.globals.buffer) free(module.globals.buffer);
    hash_table_destroy(&module.type_map);
}

void test_ir_sort_globals_aggregate_refs(void) {
    // x -> { y, z } (array of two pointers). y and z independent
    ir_module_t module = {
        .name = "m2",
        .arch = NULL,
        .globals = VEC_INIT,
        .type_map = hash_table_create_string_keys(16),
        .functions = { .buffer = NULL, .size = 0, .capacity = 0 },
    };

    ir_const_t y_val = { .kind = IR_CONST_INT, .type = NULL, .value.i = 1 };
    ir_const_t z_val = { .kind = IR_CONST_INT, .type = NULL, .value.i = 2 };

    ir_const_t *arr_vals = (ir_const_t*) malloc(2 * sizeof(ir_const_t));
    arr_vals[0] = (ir_const_t){ .kind = IR_CONST_GLOBAL_POINTER, .type = NULL, .value.global_name = "y" };
    arr_vals[1] = (ir_const_t){ .kind = IR_CONST_GLOBAL_POINTER, .type = NULL, .value.global_name = "z" };

    ir_const_t x_val = {
        .kind = IR_CONST_ARRAY,
        .type = NULL,
        .value.array = { .values = arr_vals, .length = 2 },
    };

    VEC_APPEND(&module.globals, make_global("x", true, x_val));
    VEC_APPEND(&module.globals, make_global("y", true, y_val));
    VEC_APPEND(&module.globals, make_global("z", true, z_val));

    ir_sort_global_definitions(&module);

    CU_ASSERT_EQUAL(module.globals.size, 3);
    // x must be after both y and z
    size_t idx_x = 0, idx_y = 0, idx_z = 0;
    for (size_t i = 0; i < module.globals.size; i++) {
        if (strcmp(module.globals.buffer[i]->name, "x") == 0) idx_x = i;
        if (strcmp(module.globals.buffer[i]->name, "y") == 0) idx_y = i;
        if (strcmp(module.globals.buffer[i]->name, "z") == 0) idx_z = i;
    }
    CU_ASSERT_TRUE(idx_x > idx_y);
    CU_ASSERT_TRUE(idx_x > idx_z);

    // cleanup
    free(arr_vals);
    for (size_t i = 0; i < module.globals.size; i++) free(module.globals.buffer[i]);
    if (module.globals.buffer) free(module.globals.buffer);
    hash_table_destroy(&module.type_map);
}

void test_ir_sort_globals_cycle(void) {
    // a <-> b cycle; expect original order preserved and no crash
    ir_module_t module = {
        .name = "m3",
        .arch = NULL,
        .globals = VEC_INIT,
        .type_map = hash_table_create_string_keys(16),
        .functions = { .buffer = NULL, .size = 0, .capacity = 0 },
    };

    ir_const_t a_val = { .kind = IR_CONST_GLOBAL_POINTER, .type = NULL, .value.global_name = "b" };
    ir_const_t b_val = { .kind = IR_CONST_GLOBAL_POINTER, .type = NULL, .value.global_name = "a" };

    VEC_APPEND(&module.globals, make_global("a", true, a_val));
    VEC_APPEND(&module.globals, make_global("b", true, b_val));

    ir_sort_global_definitions(&module);

    CU_ASSERT_EQUAL(module.globals.size, 2);
    // Expect stable original order due to cycle fallback
    CU_ASSERT_STRING_EQUAL(module.globals.buffer[0]->name, "a");
    CU_ASSERT_STRING_EQUAL(module.globals.buffer[1]->name, "b");

    for (size_t i = 0; i < module.globals.size; i++) free(module.globals.buffer[i]);
    if (module.globals.buffer) free(module.globals.buffer);
    hash_table_destroy(&module.type_map);
}

static void test_ir_sort_globals_independent_stable(void) {
    // Two independent globals should preserve original order
    ir_module_t module = {
        .name = "module",
        .arch = NULL,
        .globals = VEC_INIT,
        .type_map = hash_table_create_string_keys(16),
        .functions = { .buffer = NULL, .size = 0, .capacity = 0 },
    };

    ir_const_t a_val = { .kind = IR_CONST_INT, .type = NULL, .value.i = 10 };
    ir_const_t b_val = { .kind = IR_CONST_INT, .type = NULL, .value.i = 20 };

    VEC_APPEND(&module.globals, make_global("first", true, a_val));
    VEC_APPEND(&module.globals, make_global("second", true, b_val));

    ir_sort_global_definitions(&module);

    CU_ASSERT_EQUAL(module.globals.size, 2);
    CU_ASSERT_STRING_EQUAL(module.globals.buffer[0]->name, "first");
    CU_ASSERT_STRING_EQUAL(module.globals.buffer[1]->name, "second");

    for (size_t i = 0; i < module.globals.size; i++) free(module.globals.buffer[i]);
    if (module.globals.buffer) free(module.globals.buffer);
    hash_table_destroy(&module.type_map);
}

int ir_tests_init_suite(void) {
    CU_pSuite suite = CU_add_suite("IR tests", NULL, NULL);
    if (suite == NULL) return CU_get_error();

    CU_add_test(suite, "sort globals - linear chain", test_ir_sort_globals_linear_chain);
    CU_add_test(suite, "sort globals - aggregate references", test_ir_sort_globals_aggregate_refs);
    CU_add_test(suite, "sort globals - cycle", test_ir_sort_globals_cycle);
    return CUE_SUCCESS;
}
