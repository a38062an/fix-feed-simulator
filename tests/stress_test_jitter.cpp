#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <algorithm>
#include <random>
#include <iomanip>
#include <cmath>

#include <core/blocking_ring_buffer.h>

// --- 1. Realistic Payload (64 Bytes) ---
// Fits exactly in one CPU Cache Line. This stresses memory bandwidth.
struct MarketTick
{
    long sequenceId;  // 8 bytes
    double bid;       // 8 bytes
    double ask;       // 8 bytes
    char symbol[8];   // 8 bytes
    long timestamp;   // 8 bytes
    char padding[24]; // Fill to 64 bytes
};

const int NUM_OPS = 1'000'000;
const int BUFFER_SIZE = 1024; // Smaller buffer forces collisions sooner

// --- 2. Simulated CPU Work (Busy Wait) ---
// We don't use sleep() because it yields the CPU. We want to burn cycles
// to simulate FIX encoding or JSON parsing.
void burn_cpu(int nanoseconds)
{
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::nanoseconds(nanoseconds))
        ;
}

int main()
{
    BlockingRingBuffer<MarketTick, BUFFER_SIZE> queue;

    // Vectors to capture latency per-operation (for histogram)
    std::vector<long> producer_latencies;
    producer_latencies.reserve(NUM_OPS);

    std::atomic<bool> start_gun{false};

    // --- PRODUCER (The Exchange) ---
    std::jthread producer([&]()
                          {
        while (!start_gun.load(std::memory_order_acquire));

        MarketTick tick;
        for (int i = 0; i < NUM_OPS; ++i) {
            auto t0 = std::chrono::high_resolution_clock::now();
            
            // This WILL block if Consumer is too slow
            queue.push(tick);

            auto t1 = std::chrono::high_resolution_clock::now();
            auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
            producer_latencies.push_back(latency);
        } });

    // --- CONSUMER (The FIX Engine) ---
    std::jthread consumer([&]()
                          {
        while (!start_gun.load(std::memory_order_acquire));

        MarketTick tick;
        for (int i = 0; i < NUM_OPS; ++i) {
            queue.pop(tick);

            // --- SIMULATED REALITY ---
            // Simulate 500ns of work (FIX encoding + Network Stack)
            // This makes the Consumer slower than Producer!
            burn_cpu(500); 
        } });

    std::cout << "Starting Stress Test (Simulated Load)..." << std::endl;
    auto start_time = std::chrono::high_resolution_clock::now();
    start_gun.store(true, std::memory_order_release);

    producer.join();
    consumer.join();
    auto end_time = std::chrono::high_resolution_clock::now();

    // --- 3. Analysis ---
    double total_seconds = std::chrono::duration<double>(end_time - start_time).count();

    // Sort latencies to find percentiles
    std::sort(producer_latencies.begin(), producer_latencies.end());
    long p50 = producer_latencies[NUM_OPS * 0.50];
    long p99 = producer_latencies[NUM_OPS * 0.99];
    long p999 = producer_latencies[NUM_OPS * 0.999];
    long max_lat = producer_latencies.back();

    std::cout << "\n=== STRESS TEST RESULTS ===" << std::endl;
    std::cout << "Throughput:    " << (long)(NUM_OPS / total_seconds) << " ops/sec" << std::endl;
    std::cout << "\nLatency Distribution (How long push() took):" << std::endl;
    std::cout << "Median (p50):  " << p50 << " ns" << std::endl;
    std::cout << "99%   (p99):   " << p99 << " ns   <-- The Danger Zone" << std::endl;
    std::cout << "99.9% (p999):  " << p999 << " ns" << std::endl;
    std::cout << "Max Latency:   " << max_lat << " ns" << std::endl;

    if (p99 > 1000)
    {
        std::cout << "\n[ANALYSIS]: High p99 latency detected!" << std::endl;
        std::cout << "The Producer is hitting a full buffer and getting put to sleep by the OS." << std::endl;
        std::cout << "Solution: Lock-Free Queue + Busy-Wait Strategy." << std::endl;
    }
}