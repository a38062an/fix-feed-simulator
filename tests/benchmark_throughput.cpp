#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <iomanip>
#include <memory> // For std::unique_ptr

// --- ARCHITECTURE SPECIFIC INTRINSICS ---
#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif

#include "../include/core/blocking_ring_buffer.h"
#include "../include/core/nonblocking_ring_buffer.h"

// --- CONSTANTS ---
// 65536 is a standard HFT buffer size (2^16).
// Large enough to absorb bursts, small enough to stay in L2/L3 cache.
const size_t BUFFER_CAPACITY = 65536;
const int ITERATIONS = 10'000'000;

// A realistic 16-byte payload (Sequence ID + Timestamp)
struct Order
{
    uint64_t id;
    uint64_t ts;
};

// --- CPU RELAX HELPER ---
inline void cpu_relax()
{
#if defined(__x86_64__) || defined(_M_X64)
    _mm_pause();
#elif defined(__aarch64__) || defined(_M_ARM64)
    asm volatile("yield");
#else
    std::this_thread::yield();
#endif
}

// --- BLOCKING TEST ---
double run_blocking(bool is_warmup)
{
    auto q = std::make_unique<BlockingRingBuffer<Order, BUFFER_CAPACITY>>();
    std::atomic<bool> start{false};

    // Adjust iterations for warmup
    int count = is_warmup ? (ITERATIONS / 10) : ITERATIONS;

    std::thread consumer([&]()
                         {
        while (!start.load(std::memory_order_acquire));
        Order t;
        for (int i = 0; i < count; ++i) {
            q->pop(t); // Blocking wait internally
        } });

    std::thread producer([&]()
                         {
        while (!start.load(std::memory_order_acquire));
        for (int i = 0; i < count; ++i) {
            q->push({(uint64_t)i, 0}); // Blocking wait internally
        } });

    auto t1 = std::chrono::high_resolution_clock::now();
    start.store(true, std::memory_order_release);

    producer.join();
    consumer.join();

    auto t2 = std::chrono::high_resolution_clock::now();

    double duration_sec = std::chrono::duration<double>(t2 - t1).count();
    return count / duration_sec; // Ops per second
}

// --- NON-BLOCKING TEST ---
double run_nonblocking(bool is_warmup)
{
    auto q = std::make_unique<LockFreeRingBuffer<Order, BUFFER_CAPACITY>>();
    std::atomic<bool> start{false};

    int count = is_warmup ? (ITERATIONS / 10) : ITERATIONS;

    std::thread consumer([&]()
                         {
        while (!start.load(std::memory_order_acquire));
        Order t;
        for (int i = 0; i < count; ++i) {
            // Spin until we get data
            while (!q->pop(t)) {
                cpu_relax();
            }
        } });

    std::thread producer([&]()
                         {
        while (!start.load(std::memory_order_acquire));
        for (int i = 0; i < count; ++i) {
            // Spin until we have space
            while (!q->push({(uint64_t)i, 0})) {
                cpu_relax();
            }
        } });

    auto t1 = std::chrono::high_resolution_clock::now();
    start.store(true, std::memory_order_release);

    producer.join();
    consumer.join();

    auto t2 = std::chrono::high_resolution_clock::now();

    double duration_sec = std::chrono::duration<double>(t2 - t1).count();
    return count / duration_sec;
}

void print_result(const std::string &name, double ops_per_sec)
{
    std::cout << std::left << std::setw(20) << name
              << " : " << std::fixed << std::setprecision(0)
              << ops_per_sec << " ops/sec ("
              << std::setprecision(2) << (ops_per_sec / 1'000'000.0) << " M/s)" << std::endl;
}

int main()
{
    std::cout << "--- THROUGHPUT BENCHMARK ---\n";
    std::cout << "Payload: 16 Bytes | Capacity: " << BUFFER_CAPACITY << " | Iterations: " << ITERATIONS << "\n";
    std::cout << "Architecture: "
#if defined(__aarch64__) || defined(_M_ARM64)
                 "ARM64 (Apple Silicon)"
#else
                 "x86_64"
#endif
              << "\n\n";

    std::cout << "Warming up caches...\n";
    run_blocking(true);
    run_nonblocking(true);
    std::cout << "Warmup complete. Starting Race.\n\n";

    // Run Blocking
    double block_res = run_blocking(false);
    print_result("Blocking (Mutex)", block_res);

    // Run Non-Blocking
    double nonblock_res = run_nonblocking(false);
    print_result("Lock-Free (Atomic)", nonblock_res);

    // Calculate Improvement
    double improvement = ((nonblock_res - block_res) / block_res) * 100.0;
    std::cout << "\nImprovement: " << std::fixed << std::setprecision(2) << improvement << "%" << std::endl;

    return 0;
}