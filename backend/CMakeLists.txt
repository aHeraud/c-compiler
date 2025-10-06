cmake_minimum_required(VERSION 3.22.1)
project(backend C)

include_directories("${CMAKE_SOURCE_DIR}/ir/include")

# LLVM setup
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM v${LLVM_PACKAGE_VERSION} in ${LLVM_DIR}")
message(STATUS "LLVM binary dir: ${LLVM_TOOLS_BINARY_DIR}")
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_library(backend STATIC
        include/backend/backend.h
        src/llvm/llvm-gen.h src/llvm/llvm-gen.c
)

target_link_libraries(backend PUBLIC common ir LLVM) # TODO: only link the required libraries, not all of LLVM
