cmake_minimum_required(VERSION 3.20)
project(ComplexityLab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Emit compile_commands.json for IDE tooling.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTS "Build tests" ON)

# No default: an unset or misspelled build type is no config at all, silently
# dropping -g and -O. Demand an explicit, exact choice rather than guessing.
set(CL_BUILD_TYPES Debug Release RelWithDebInfo)
if(NOT CMAKE_BUILD_TYPE IN_LIST CL_BUILD_TYPES)
    list(JOIN CL_BUILD_TYPES ", " CL_BUILD_TYPES_HELP)
    message(FATAL_ERROR
        "CMAKE_BUILD_TYPE is '${CMAKE_BUILD_TYPE}', must be one of: ${CL_BUILD_TYPES_HELP}\n"
        "  e.g. cmake -B build/debug -DCMAKE_BUILD_TYPE=Debug")
endif()

# External libraries and the imgui target. Each layer's own targets live in its
# directory's CMakeLists.txt.
include(cmake/Dependencies.cmake)

# Settings for the repo's own code. add_compile_options() is captured per target
# as it is created, so listing it after the include above keeps our warnings off
# the third-party targets; add_compile_definitions() is a directory property and
# so reaches imgui as well, which is harmless. Optimization and -g come from
# CMake's per-config defaults.
add_compile_options(-Wall -Wextra -Wpedantic -Wno-missing-field-initializers)
add_compile_definitions(
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
    GLFW_INCLUDE_VULKAN
)

# GCC <= 13 miswarns (-Warray-bounds) on libstdc++'s std::copy memmove path,
# which Vulkan-Hpp's uint32 array copies instantiate everywhere. Force-include a
# header that parses <algorithm> inside a diagnostic-ignored region as the first
# thing in every TU, silencing the false positive at its library source while
# keeping -Warray-bounds active for our own code. Fixed in GCC 14, so the
# workaround self-removes once the toolchain is upgraded. See the header.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14)
    add_compile_options("SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gcc-array-bounds-fp.h")
endif()

add_subdirectory(lib)      # cl_lib    — pure primitives
add_subdirectory(engine)   # cl_engine — framework
add_subdirectory(apps)     # executables

if(BUILD_TESTS)
    find_package(GTest REQUIRED)
    enable_testing()
    include(GoogleTest)
    add_subdirectory(tests)
endif()
