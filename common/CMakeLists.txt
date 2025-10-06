cmake_minimum_required(VERSION 3.22.1)
project(common C)

include_directories("${CMAKE_CURRENT_LIST_DIR}/include")

add_library(common STATIC
        include/utils/hashtable.h src/hashtable.c
        include/utils/vectors.h src/vectors.c
        include/target.h src/target.c
)

include_directories("${CMAKE_SOURCE_DIR}/dependencies/cunit/CUnit")
add_executable(common-unit-tests
        tst/test-main.c
        tst/hashtable-tests.c
)
target_link_libraries(common-unit-tests PUBLIC common cunit)
add_test(NAME common-unit-tests COMMAND common-unit-tests WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
