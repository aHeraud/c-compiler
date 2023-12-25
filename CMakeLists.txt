cmake_minimum_required(VERSION 3.25)
project(c_compiler C)

function(add_git_submodule path)
    find_package(Git REQUIRED)

    execute_process(COMMAND ${GIT_EXECUTABLE} submodule init ${path}
            WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
            COMMAND_ERROR_IS_FATAL ANY)

    execute_process(COMMAND ${GIT_EXECUTABLE} submodule update ${path}
            WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
            COMMAND_ERROR_IS_FATAL ANY)

    add_subdirectory(${path})
endfunction(add_git_submodule)

# LLVM setup
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM v${LLVM_PACKAGE_VERSION} in ${LLVM_DIR}")
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

set(CMAKE_C_STANDARD 99)
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

include_directories(src)

add_library(cclib STATIC
        src/lexer.h src/lexer.c
        src/preprocessor.h src/preprocessor.c
        src/util/read-lines.h src/util/read-lines.c
        src/util/vectors.h src/util/vectors.c
        src/parser.c src/parser.h
        src/util/ast-printer.h src/util/ast-printer.c src/ast.h
        src/util/hashtable.c src/util/hashtable.h
        src/codegen.c src/codegen.h
        src/types.c src/types.h
)

add_executable(cc src/main.c)

target_link_libraries(cc PUBLIC cclib)
target_link_libraries(cc PUBLIC LLVM) # TODO: only link the required libraries, not all of LLVM

set(CUNIT_DISABLE_TESTS TRUE)
set(CUNIT_DISABLE_EXAMPLES TRUE)
add_git_submodule("dependencies/cunit")
add_executable(unit-tests
        src/util/read-lines.h src/util/vectors.h src/lexer.h src/parser.h src/util/ast-printer.h
        tst/test-main.c
        tst/tests.h
        tst/util/read-lines-test.c
        tst/lexer-tests.c
        tst/parser-tests.c src/ast.h
        tst/preprocessor-tests.c
        tst/util/hashtable-tests.c
        tst/test-common.h
        tst/test-common.c
        tst/types-test.c
)
target_link_libraries(unit-tests PUBLIC cunit)
target_link_libraries(unit-tests PUBLIC cclib)

enable_testing()
add_test(NAME tests COMMAND unit-tests WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
