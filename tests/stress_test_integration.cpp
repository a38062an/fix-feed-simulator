#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <cstring>
#include <atomic>

// --- Include YOUR Real Components ---
#include <core/blocking_ring_buffer.h>
#include <fix/message.h>
#include <network/udp_sender.h>

// Real Payload
struct MarketTick
{
    char symbol[8];
    double bid;
    double ask;
};

const int NUM_OPS = 100'000;
const int BUFFER_SIZE = 1024;

int main()
{
    try
    {
        BlockingRingBuffer<MarketTick, BUFFER_SIZE> queue;

        // Capture latencies
        std::vector<long> producer_latencies;
        producer_latencies.reserve(NUM_OPS);

        std::atomic<bool> start_gun{false};

        // --- PRODUCER (Generates data) ---
        std::jthread producer([&]()
                              {
            // Busy wait for start
            while (!start_gun.load(std::memory_order_acquire));
            
            MarketTick tick;
            std::strncpy(tick.symbol, "ESZ5", 4);
            tick.bid = 100.0; 
            tick.ask = 100.25;

            for (int i = 0; i < NUM_OPS; ++i) {
                auto t0 = std::chrono::high_resolution_clock::now();
                
                // Pushes to queue. BLOCKS if queue is full.
                queue.push(tick);

                auto t1 = std::chrono::high_resolution_clock::now();
                producer_latencies.push_back(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()
                );
            } });

        // --- CONSUMER (Encodes FIX + Sends UDP) ---
        std::jthread consumer([&]()
                              {
            UDPMulticastSender sender("239.255.1.1", 9999);
            FIXMessage fixMessage("FIX.4.2");
            MarketTick tick;

            while (!start_gun.load(std::memory_order_acquire));

            for (int i = 0; i < NUM_OPS; ++i) {
                // 1. Pop from Queue
                queue.pop(tick);

                // 2. Encode FIX
                fixMessage.clearBody();
                fixMessage.addField(35, "W")
                          .addField(55, tick.symbol)
                          .addField(269, "0").addField(270, std::to_string(tick.bid))
                          .addField(269, "1").addField(270, std::to_string(tick.ask));
                
                auto data = fixMessage.finalize();

                // 3. Send UDP with Backpressure (Retry on Buffer Full)
                bool sent = false;
                while (!sent) {
                    try {
                        // This might THROW "ENOBUFS" if the kernel is full
                        sender.send(data);
                        sent = true; // If we get here, it succeeded
                    } 
                    catch (const std::exception& e) {
                        // Now we just pause for a microsecond to let the buffer drain
                        std::this_thread::yield(); 
                    }
                }
            } });

        std::cout << "Starting REAL Integration Test (FIX + UDP)..." << std::endl;

        auto start_time = std::chrono::high_resolution_clock::now();
        start_gun.store(true, std::memory_order_release); // GO!

        producer.join();
        consumer.join();

        auto end_time = std::chrono::high_resolution_clock::now();

        // --- Analysis ---
        double total_seconds = std::chrono::duration<double>(end_time - start_time).count();

        std::sort(producer_latencies.begin(), producer_latencies.end());
        long p50 = producer_latencies[NUM_OPS * 0.50];
        long p99 = producer_latencies[NUM_OPS * 0.99];
        long max_lat = producer_latencies.back();

        std::cout << "\n=== INTEGRATION TEST RESULTS ===" << std::endl;
        std::cout << "Throughput:    " << (long)(NUM_OPS / total_seconds) << " ops/sec" << std::endl;
        std::cout << "Median (p50):  " << p50 << " ns" << std::endl;
        std::cout << "99%   (p99):   " << p99 << " ns" << std::endl;
        std::cout << "Max Latency:   " << max_lat << " ns" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
    }
}