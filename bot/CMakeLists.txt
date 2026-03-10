cmake_minimum_required(VERSION 3.10)
project(Knilb)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libstdc++ -static-libgcc")



# OPTIMIZATION FLAGS
if(MSVC)
    add_compile_options(/O2)
else()
    add_compile_options(-O3 -march=native -w )
endif()
# Include the 'include' directory
include_directories(include)

# ---------------------------------------------------------
# AUTO-DISCOVERY
# This finds all .cpp files in src/ and saves them to SOURCES
# ---------------------------------------------------------
file(GLOB_RECURSE SOURCES "src/*.cpp")
# Exclude test files from main sources
list(FILTER SOURCES EXCLUDE REGEX "src/tests/.*")

# Create the executable using the discovered list
add_executable(Knilb ${SOURCES})