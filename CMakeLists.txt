cmake_minimum_required(VERSION 3.22.1)
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

# Python setup (for validation tests)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(CMAKE_C_STANDARD 99)
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

# Ultimately, the compiler needs to be able to compile itself.
# As we're targeting the c99 standard, we don't want to use any language features from gnu extensions or later
# versions of C.
if (CMAKE_C_COMPILER_ID MATCHES "Clang")
    # Clang
    SET(CMAKE_C_FLAGS "-std=c99 -pedantic-errors -Wall -Wextra -Wno-incompatible-pointer-types-discards-qualifiers -Wno-strict-prototypes")
elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    # GCC
    SET(CMAKE_C_FLAGS "-std=c99 -pedantic-errors -Wpedantic -Werror -Wno-discarded-qualifiers")
else()
    SET(CMAKE_C_FLAGS "-std=c99")
endif()

enable_testing()

add_subdirectory(common)
include_directories(${CMAKE_SOURCE_DIR}/common/include)

add_subdirectory(ir)
include_directories(${CMAKE_SOURCE_DIR}/ir/include)

add_subdirectory(backend)
include_directories(${CMAKE_SOURCE_DIR}/backend/include)

include_directories(src)

add_library(cclib STATIC
        src/parser/lexer.h src/parser/lexer.c
        src/parser/parser.c src/parser/parser.h
        src/ast.h src/ast.c
        src/types.c src/types.h
        src/parser/numeric-constants.h src/parser/numeric-constants.c
        src/errors.h src/errors.c
        src/ir/codegen/codegen.h src/ir/codegen/codegen.c
        src/ir/codegen/internal.h src/ir/codegen/internal.c
        src/ir/codegen/declarations.c
        src/ir/codegen/expressions.c
        src/ir/codegen/initializers.c
        src/ir/codegen/statements.c
        src/util/strings.h src/util/strings.c
        src/builtins.h src/builtins.c
)
target_link_libraries(cclib PUBLIC common ir backend)

add_executable(cc src/main.c)
target_link_libraries(cc PUBLIC cclib)

# For enabling asan
# target_compile_options(cc PRIVATE -fsanitize=address)
# target_link_options(cc PRIVATE -fsanitize=address)

set(CMAKE_POLICY_VERSION_MINIMUM "3.10")
set(CUNIT_DISABLE_TESTS TRUE)
set(CUNIT_DISABLE_EXAMPLES TRUE)
add_git_submodule("dependencies/cunit")
add_executable(unit-tests
        src/parser/lexer.h src/parser/parser.h
        tst/test-main.c
        tst/tests.h
        tst/lexer-tests.c
        tst/parser-tests.c src/ast.h
        tst/test-common.h
        tst/test-common.c
        tst/types-test.c
        tst/numeric-constants-tests.c
        tst/ir/ir-gen-tests.c
)
target_compile_definitions(unit-tests PRIVATE C_COMPILER_UNIT_TESTS)
target_link_libraries(unit-tests PUBLIC cunit)
target_link_libraries(unit-tests PUBLIC cclib ir common)

# CTest

add_test(NAME unit-tests COMMAND unit-tests WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
find_package(LLVM REQUIRED CONFIG)
add_test(NAME validation-tests COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/validation/harness.py WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/validation)
set_property(TEST validation-tests PROPERTY ENVIRONMENT_MODIFICATION "PATH=path_list_append:${LLVM_TOOLS_BINARY_DIR}")
