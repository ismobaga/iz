cmake_minimum_required(VERSION 3.16)
project(izi CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find LLVM (prefer 18)
list(APPEND CMAKE_PREFIX_PATH
    /usr/lib/llvm-18/lib/cmake/llvm
    /usr/lib/llvm-18/lib/cmake/clang)
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM install prefix: ${LLVM_INSTALL_PREFIX}")

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

# Collect all source files
file(GLOB_RECURSE IZI_SOURCES
    src/lexer/*.cpp
    src/parser/*.cpp
    src/typechecker/*.cpp
    src/codegen/*.cpp
    src/main.cpp)

add_executable(izi ${IZI_SOURCES})

# Link against LLVM shared library
find_library(LLVM_SHARED_LIB LLVM-18
    HINTS /usr/lib/llvm-18/lib /usr/lib/x86_64-linux-gnu)
if(NOT LLVM_SHARED_LIB)
    # Fallback: use component mapping
    llvm_map_components_to_libnames(LLVM_LIBS core support passes native nativecodegen mc object target)
    set(LLVM_SHARED_LIB ${LLVM_LIBS})
endif()

target_link_libraries(izi PRIVATE ${LLVM_SHARED_LIB})
target_include_directories(izi PRIVATE src)

# Tests executable
file(GLOB_RECURSE TEST_SOURCES tests/*.cpp)
if(TEST_SOURCES)
    add_executable(izi_tests ${TEST_SOURCES}
        src/lexer/lexer.cpp
        src/parser/parser.cpp
        src/typechecker/typechecker.cpp
        src/codegen/codegen.cpp)
    target_link_libraries(izi_tests PRIVATE ${LLVM_SHARED_LIB})
    target_include_directories(izi_tests PRIVATE src)

    enable_testing()
    add_test(NAME izi_tests COMMAND izi_tests)
endif()
