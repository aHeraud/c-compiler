cmake_minimum_required(VERSION 3.22.1)
project(ir C)

include_directories("${CMAKE_CURRENT_LIST_DIR}/include")
add_library(ir STATIC
        include/ir/arch.h src/arch.c
        include/ir/cfg.h src/cfg.c
        include/ir/fmt.h src/fmt.c
        include/ir/ir.h src/ir.c
        include/ir/ir-builder.h src/ir-builder.c
        include/ir/ssa.h src/ssa.c
)
target_link_libraries(ir PUBLIC utils)

include_directories("${CMAKE_SOURCE_DIR}/dependencies/cunit/CUnit")
add_executable(ir-unit-tests
        tst/test-main.c
        tst/cfg-tests.c
)
target_link_libraries(ir-unit-tests PUBLIC ir cunit)
add_test(NAME ir-unit-tests COMMAND ir-unit-tests WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
