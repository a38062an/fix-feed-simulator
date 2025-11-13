cmake_minimum_required(VERSION 4.0)
project(market_data_system C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(market_data_system
        include/market/price_generator.h
        include/market/random_walk_generator.h
        include/core/blocking_ring_buffer.h
        include/core/ring_buffer.h
        include/market/random_walk_generator.h
        include/core/ring_buffer.h
        include/core/ring_buffer.h
        include/core/ring_buffer.h)
