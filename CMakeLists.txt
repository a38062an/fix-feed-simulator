cmake_minimum_required(VERSION 4.0)

# Project declaration: we build both C and C++ sources. Declaring CXX
# enables CMake's C++ support.
project(market_data_system LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Basic language standards and flags
# ---------------------------------------------------------------------------
# Keep C settings (project uses a small C API in headers)
set(CMAKE_C_STANDARD 11)

# Use C++20 for the C++ sources (required for std::span, concepts, etc.)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands (compile_commands.json) so tools like VS Code and
# clangd can pick up the exact compile flags and include paths. This file is
# generated in the build directory after configuration.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set a sane default build type if the user didn't provide one.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "No build type selected, default to Debug")
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# Sources and target
# ---------------------------------------------------------------------------
# We build a single executable from the main in `src/producer_main.cpp`.
# Headers under `include/` are not compiled directly; instead we add the
# directory to the target's include path so `#include <market/...>` works.
set(SRC_FILES
        src/producer_main.cpp
)

add_executable(market_data_system ${SRC_FILES})

# Make the project's `include/` directory available to the compiler. Using
# target_include_directories keeps the include scope local to this target and
# is preferred over the global include_directories() call.
target_include_directories(market_data_system
        PRIVATE
                ${CMAKE_SOURCE_DIR}/include
)

# You can add target_compile_definitions, link libraries, or other target
# properties below. For example, if you later add a network library you can
# link it with target_link_libraries(market_data_system PRIVATE <lib>).

# Optional: set output directory (keeps the binary in build/)
set_target_properties(market_data_system PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Helpful status message when configuring
message(STATUS "Configured project '${PROJECT_NAME}'")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Include dir: ${CMAKE_SOURCE_DIR}/include")
