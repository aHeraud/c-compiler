cmake_minimum_required(VERSION 3.22.1)
project(utils C)

include_directories("${CMAKE_CURRENT_LIST_DIR}/include")

add_library(utils STATIC
        include/utils/hashtable.h src/hashtable.c
        ../src/util/strings.h ../src/util/strings.c
        include/utils/vectors.h src/vectors.c
)

include_directories("${CMAKE_SOURCE_DIR}/dependencies/cunit/CUnit")
add_executable(utils-unit-tests
        tst/test-main.c
        tst/hashtable-tests.c
)
target_link_libraries(utils-unit-tests PUBLIC utils cunit)
add_test(NAME utils-unit-tests COMMAND utils-unit-tests WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
