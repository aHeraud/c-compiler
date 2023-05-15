#include "CUnit/Basic.h"
#include "tests.h"

int main() {
    if (CUE_SUCCESS != CU_initialize_registry()) {
        return CU_get_error();
    }

    read_lines_test_init_suite();
    lexer_tests_init_suite();

    CU_basic_set_mode(CU_BRM_VERBOSE);

    CU_ErrorCode error;
    if (CUE_SUCCESS != (error = CU_basic_run_tests())) {
        return error;
    }

    unsigned int tests_failed = CU_get_number_of_failures();

    CU_cleanup_registry();
    if (CU_get_error() != CUE_SUCCESS) {
        return CU_get_error();
    }

    return (int) tests_failed;
}
