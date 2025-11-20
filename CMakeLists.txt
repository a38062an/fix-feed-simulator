cmake_minimum_required(VERSION 3.20)
project(FixFeedSimulator)

set(CMAKE_CXX_STANDARD 20)

# --- 1. Dependencies ---
find_package(Threads REQUIRED)

# Find PCAP (Networking)
find_library(PCAP_LIBRARY NAMES pcap)
if(NOT PCAP_LIBRARY)
    message(WARNING "libpcap not found - networking features may fail to link")
endif()

# --- 2. Include Directories ---
include_directories(include)

# --- 3. EXECUTABLES ---

# UDP senders: GBM and RandomWalk variants only
add_executable(udp_sender_gbm src/producer_gbm.cpp)
if(PCAP_LIBRARY)
    target_link_libraries(udp_sender_gbm PRIVATE ${PCAP_LIBRARY})
endif()

add_executable(udp_sender_rw src/producer_rw.cpp)
if(PCAP_LIBRARY)
    target_link_libraries(udp_sender_rw PRIVATE ${PCAP_LIBRARY})
endif()

# Non-blocking Market Data System (GBM) - new executable using the lock-free ring buffer
add_executable(udp_sender_gbm_nonblocking src/producer_gbm_nonblocking.cpp)
if(PCAP_LIBRARY)
    target_link_libraries(udp_sender_gbm_nonblocking PRIVATE ${PCAP_LIBRARY})
endif()

# Non-blocking Market Data System (RandomWalk) - new executable using the lock-free ring buffer
add_executable(udp_sender_rw_nonblocking src/producer_rw_nonblocking.cpp)
if(PCAP_LIBRARY)
    target_link_libraries(udp_sender_rw_nonblocking PRIVATE ${PCAP_LIBRARY})
endif()
# B. The Packet Analyzer (Console)
add_executable(packet_analyzer src/analyzer_main.cpp)
if(PCAP_LIBRARY)
    target_link_libraries(packet_analyzer PRIVATE ${PCAP_LIBRARY})
endif()

# C. Tests & Benchmarks
add_executable(stress_test_integration tests/stress_test_integration.cpp)
target_link_libraries(stress_test_integration pthread)

add_executable(stress_test_jitter tests/stress_test_jitter.cpp)
target_link_libraries(stress_test_jitter pthread)

add_executable(benchmark_throughput tests/benchmark_throughput.cpp)
target_link_libraries(benchmark_throughput pthread)

# Ring buffer stress test
# Latency benchmark
add_executable(latency_benchmark tests/benchmark_latency.cpp)
target_link_libraries(latency_benchmark pthread)