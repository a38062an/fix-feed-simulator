cmake_minimum_required(VERSION 3.16)
project(market_data_system LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Compiler settings
# ---------------------------------------------------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate compile_commands.json for clangd/IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to Debug build if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type (Debug/Release)" FORCE)
endif()

# ---------------------------------------------------------------------------
# Common settings
# ---------------------------------------------------------------------------
set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)
set(OUTPUT_DIR ${CMAKE_BINARY_DIR})

# ---------------------------------------------------------------------------
# Producer executable
# ---------------------------------------------------------------------------
add_executable(udp_sender
    src/producer_main.cpp
)

target_include_directories(udp_sender PRIVATE ${INCLUDE_DIR})
set_target_properties(udp_sender PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR}
)

# ---------------------------------------------------------------------------
# Packet analyzer executable (requires libpcap)
# ---------------------------------------------------------------------------
find_library(PCAP_LIBRARY NAMES pcap)

if(NOT PCAP_LIBRARY)
    message(WARNING "libpcap not found - packet_analyzer will not link")
endif()

add_executable(packet_analyzer 
    src/analyzer_main.cpp
)

target_include_directories(packet_analyzer PRIVATE ${INCLUDE_DIR})
set_target_properties(packet_analyzer PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR}
)

if(PCAP_LIBRARY)
    target_link_libraries(packet_analyzer PRIVATE ${PCAP_LIBRARY})
endif()

# ---------------------------------------------------------------------------
# Status output
# ---------------------------------------------------------------------------
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "Include directory: ${INCLUDE_DIR}")
if(PCAP_LIBRARY)
    message(STATUS "libpcap: ${PCAP_LIBRARY}")
endif()