#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <memory>
#include <cmath>   // For std::sqrt
#include <numeric> // For std::accumulate

// --- ARCHITECTURE DETECTION ---
#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#include <x86intrin.h> // For __rdtsc()
#elif defined(__aarch64__) || defined(_M_ARM64)
// ARM64 Timer
inline uint64_t __rdtsc()
{
    uint64_t val;
    asm volatile("mrs %0, cntvct_el0" : "=r"(val));
    return val;
}
#endif

#include "../include/core/blocking_ring_buffer.h"
#include "../include/core/nonblocking_ring_buffer.h"

// --- CONSTANTS ---
// Estimated nanoseconds per cycle for a 3.2GHz CPU (M1/M2/M3 Performance Core)
// Update this if running on a different clock speed (e.g. 1/Freq)
const double NS_PER_CYCLE = 0.3125;

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

struct Tick
{
    uint64_t send_time; // We pass the timestamp through the queue
};

// --- STATS PRINTER ---
void print_detailed_stats(const std::string &label, std::vector<uint64_t> &data)
{
    if (data.empty())
        return;

    // 1. Basic Stats
    std::sort(data.begin(), data.end());
    uint64_t min_val = data.front();
    uint64_t max_val = data.back();
    uint64_t median = data[data.size() / 2];
    uint64_t p99 = data[static_cast<size_t>(data.size() * 0.99)];

    // 2. Average (Mean)
    double sum = std::accumulate(data.begin(), data.end(), 0.0);
    double mean = sum / data.size();

    // 3. Standard Deviation (Jitter)
    // We calculate variance manually to avoid complex iterator headers
    double accum = 0.0;
    for (uint64_t d : data)
    {
        accum += (d - mean) * (d - mean);
    }
    double stdev = std::sqrt(accum / data.size());

    std::cout << "--------------------------------------------------\n"
              << "  " << label << "\n"
              << "--------------------------------------------------\n"
              << std::fixed << std::setprecision(2)
              << "Min:        " << std::setw(10) << min_val << " cycles (" << (min_val * NS_PER_CYCLE) << " ns)\n"
              << "Median:     " << std::setw(10) << median << " cycles (" << (median * NS_PER_CYCLE) << " ns)\n"
              << "Mean:       " << std::setw(10) << mean << " cycles (" << (mean * NS_PER_CYCLE) << " ns)\n"
              << "99%ile:     " << std::setw(10) << p99 << " cycles (" << (p99 * NS_PER_CYCLE) << " ns) <- Tail Latency\n"
              << "Max:        " << std::setw(10) << max_val << " cycles\n"
              << "StdDev:     " << std::setw(10) << stdev << " cycles (" << (stdev * NS_PER_CYCLE) << " ns) <- Jitter\n"
              << "\n";
}

// --- NON-BLOCKING LATENCY TEST ---
void test_nonblocking(size_t iterations)
{
    auto q = std::make_unique<LockFreeRingBuffer<Tick, 65536>>();
    std::vector<uint64_t> latencies;
    latencies.reserve(iterations);

    std::atomic<bool> running{true};

    // Consumer
    std::thread consumer([&]()
                         {
        Tick t;
        while (running.load(std::memory_order_relaxed)) {
            if (q->pop(t)) {
                uint64_t now = __rdtsc();
                if (t.send_time > 0) { // Ignore warmup dummy data
                    latencies.push_back(now - t.send_time);
                }
            } else {
                cpu_relax();
            }
        } });

    // Producer
    for (size_t i = 0; i < iterations; ++i)
    {
        // Simulate market gaps to expose wake-up latency
        if (i % 100 == 0)
            std::this_thread::sleep_for(std::chrono::microseconds(10));

        Tick t{__rdtsc()};
        while (!q->push(t))
            cpu_relax();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;
    consumer.join();

    print_detailed_stats("Non-Blocking (Lock-Free) Stats", latencies);
}

// --- BLOCKING LATENCY TEST ---
void test_blocking(size_t iterations)
{
    auto q = std::make_unique<BlockingRingBuffer<Tick, 65536>>();
    std::vector<uint64_t> latencies;
    latencies.reserve(iterations);

    std::atomic<bool> running{true};

    // Consumer
    std::thread consumer([&]()
                         {
        Tick t;
        while (running.load(std::memory_order_relaxed)) {
            if (q->pop(t)) {
                uint64_t now = __rdtsc();
                if (t.send_time > 0) {
                    latencies.push_back(now - t.send_time);
                }
            }
        } });

    // Producer
    for (size_t i = 0; i < iterations; ++i)
    {
        if (i % 100 == 0)
            std::this_thread::sleep_for(std::chrono::microseconds(10));

        Tick t{__rdtsc()};
        q->push(t);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;
    q->stop();
    consumer.join();

    print_detailed_stats("Blocking (Mutex/CondVar) Stats", latencies);
}

int main()
{
    std::cout << "==================================================\n";
    std::cout << "   LATENCY & JITTER BENCHMARK (Lower is Better)   \n";
    std::cout << "==================================================\n";
    std::cout << "Architecture: "
#if defined(__aarch64__) || defined(_M_ARM64)
              << "ARM64 (Apple Silicon)"
#elif defined(__x86_64__) || defined(_M_X64)
              << "x86_64"
#else
              << "Unknown"
#endif
              << "\n";
    std::cout << "Clock est:    " << (1.0 / NS_PER_CYCLE) << " GHz\n\n";

    test_blocking(100000);
    test_nonblocking(100000);

    return 0;
}