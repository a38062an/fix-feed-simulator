#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <iomanip>

// Include your specific header
#include <core/blocking_ring_buffer.h>

// 10 Million operations gives a stable average
const int NUM_OPERATIONS = 10000000;
const int BUFFER_CAPACITY = 4096;

void run_benchmark()
{
    // Setup
    BlockingRingBuffer<int, BUFFER_CAPACITY> queue;

    std::atomic<bool> start{false};

    // We use std::vector to verify data integrity (optional, but good practice)
    std::vector<int> received_data;
    received_data.reserve(NUM_OPERATIONS);

    // --- PRODUCER THREAD ---
    std::jthread producer([&]()
                          {
        // Spin until start signal (synchronizes the threads)
        while (!start.load(std::memory_order_acquire)); 

        for (int i = 0; i < NUM_OPERATIONS; ++i) {
            // This line BLOCKS if buffer is full
            // It incurs the cost of std::unique_lock + std::condition_variable
            queue.push(i);
        } });

    // --- CONSUMER THREAD ---
    std::jthread consumer([&]()
                          {
        while (!start.load(std::memory_order_acquire));

        int val;
        for (int i = 0; i < NUM_OPERATIONS; ++i) {
            // This line BLOCKS if buffer is empty
            queue.pop(val);
        } });

    std::cout << "Threads ready. Starting race..." << std::endl;

    // START TIMING
    auto start_time = std::chrono::high_resolution_clock::now();
    start.store(true, std::memory_order_release); // Fire the starting gun

    // Join threads (jthread joins automatically on destruction, but we do it explicitly to stop the clock)
    producer.join();
    consumer.join();

    auto end_time = std::chrono::high_resolution_clock::now();
    // END TIMING

    // Calculations
    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
    double seconds = duration_ns / 1e9;
    double ops_per_sec = NUM_OPERATIONS / seconds;

    std::cout << "\n--- RESULTS: BlockingRingBuffer ---" << std::endl;
    std::cout << "Operations:  " << NUM_OPERATIONS << std::endl;
    std::cout << "Time Taken:  " << std::fixed << std::setprecision(4) << seconds << "s" << std::endl;
    std::cout << "Throughput:  " << (long)ops_per_sec << " ops/sec" << std::endl;
    std::cout << "Avg Latency: " << (duration_ns / NUM_OPERATIONS) << " ns/op" << std::endl;
}

int main()
{
    try
    {
        run_benchmark();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Test Failed: " << e.what() << std::endl;
    }
    return 0;
}