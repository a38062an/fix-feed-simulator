#ifndef MARKET_DATA_SYSTEM_RW_NONBLOCKING_H
#define MARKET_DATA_SYSTEM_RW_NONBLOCKING_H

#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <functional>
#include <string>
#include <iostream>
#include <chrono>
#include <format>
#include <mutex>
#include <span>
#include <cmath>
#include <cstdlib> // For std::rand

// --- Project Components ---
#include <market/price_generator.h>
#include <market/random_walk_generator.h>
// CHANGE 1: Include the Lock-Free Queue
#include <core/nonblocking_ring_buffer.h>
#include <network/udp_sender.h>
#include <fix/message.h>

using price = double;

struct MarketTickRW
{
    std::string symbol;
    price bid;
    price ask;
    int bid_size;
    int ask_size;
};

/**
 *
 * Non-Blocking Random Walk System
 * - Uses LockFreeRingBuffer (SPSC)
 * - Uses Atomic/Spin logic instead of Mutex/Sleep
 *
 */
class MarketDataSystemRWNonBlocking
{
public:
    // CHANGE 2: Constructor accepts Interface IP to fix the Multicast Routing issue
    MarketDataSystemRWNonBlocking(const std::string &dest_ip = "239.255.1.1", uint16_t port = 9999, const std::string &interface_ip = "127.0.0.1")
    {
        // Random walk: (startPrice, stepSize)
        generators_.push_back(std::make_unique<RandomWalkGenerator<price>>(100.0, 0.01));

        try
        {
            // CHANGE 3: Pass the interface IP to the sender
            sender_ = std::make_unique<UDPMulticastSender>(dest_ip, port, interface_ip);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Could not initialise network sender: " << e.what() << std::endl;
        }

        std::cout << "MarketDataSystemRWNonBlocking initialised." << std::endl;
    }

    void start()
    {
        std::cout << "Starting threads..." << std::endl;
        threads_.emplace_back([this]
                              { producerThread(); });
        threads_.emplace_back([this]
                              { consumerThread(); });
        threads_.emplace_back([this]
                              { monitorThread(); });
        std::cout << "All threads running." << std::endl;
    }

    void stop()
    {
        std::cout << "Stopping system threads..." << std::endl;
        running_.store(false, std::memory_order_relaxed);

        // CHANGE 4: No need to push dummy tick.
        // The consumer is polling atomic flags, it will see running_ is false immediately.
        CVMonitor_.notify_all();
    }

    ~MarketDataSystemRWNonBlocking()
    {
        if (running_.load())
            stop();
        std::cout << "MarketDataSystemRWNonBlocking shutdown." << std::endl;
    }

    auto &getQueue() { return SPSCTickQueue_; }
    uint64_t getGeneratedCount() const { return ticksGenerated_.load(std::memory_order_relaxed); }
    uint64_t getSentCount() const { return ticksSent_.load(std::memory_order_relaxed); }

private:
    std::vector<std::unique_ptr<IPriceGenerator<price>>> generators_;

    // CHANGE 5: Using LockFreeRingBuffer
    LockFreeRingBuffer<MarketTickRW, 4096> SPSCTickQueue_;

    std::unique_ptr<UDPMulticastSender> sender_;

    alignas(64) std::atomic<uint64_t> ticksGenerated_{0};
    alignas(64) std::atomic<uint64_t> ticksSent_{0};
    std::vector<std::jthread> threads_;
    std::atomic<bool> running_{true};
    std::condition_variable CVMonitor_;
    std::mutex CVMutex_;

    void producerThread()
    {
        std::cout << "Producer thread started (Random Walk - NonBlocking)." << std::endl;
        auto &generator = generators_[0];

        // Pre-allocate tick to reuse memory
        MarketTickRW tick;
        tick.symbol = "ESZ5";

        while (running_.load(std::memory_order_relaxed))
        {
            price midPrice = generator->getNextPrice();
            double spread = 0.05 + 0.01 * ((double)std::rand() / RAND_MAX);
            spread = std::round(spread * 100.0) / 100.0;

            tick.bid = midPrice - spread / 2.0;
            tick.ask = midPrice + spread / 2.0;
            tick.bid_size = (std::rand() % 100) + 50;
            tick.ask_size = tick.bid_size;

            // CHANGE 6: Busy-Wait / Retry logic for Lock-Free Queue
            // If queue is full, we keep trying until space is available.
            while (!SPSCTickQueue_.push(tick))
            {
                if (!running_.load(std::memory_order_relaxed))
                    return;
                std::this_thread::yield(); // Be polite to the CPU
            }

            ticksGenerated_.fetch_add(1, std::memory_order_relaxed);
        }
        std::cout << "Producer thread stopped." << std::endl;
    }

    void consumerThread()
    {
        std::cout << "Consumer thread started" << std::endl;
        FIXMessage fixMessage("FIX.4.2");
        MarketTickRW tick;

        while (running_.load(std::memory_order_relaxed))
        {
            // CHANGE 7: Non-blocking pop logic
            if (!SPSCTickQueue_.pop(tick))
            {
                // If empty, yield so Producer can run
                std::this_thread::yield();
                continue;
            }

            // Processing Logic
            fixMessage.clearBody();
            fixMessage.addField(35, "W").addField(55, tick.symbol).addField(268, "2");
            fixMessage.addField(269, "0").addField(270, std::format("{:.2f}", tick.bid)).addField(271, std::to_string(tick.bid_size));
            fixMessage.addField(269, "1").addField(270, std::format("{:.2f}", tick.ask)).addField(271, std::to_string(tick.ask_size));

            std::span<const uint8_t> completeMessage = fixMessage.finalize();

            if (sender_)
            {
                // UDP Sending logic remains the same
                bool sent = false;
                while (!sent && running_.load(std::memory_order_relaxed))
                {
                    try
                    {
                        sender_->send(completeMessage);
                        sent = true;
                    }
                    catch (const std::exception &)
                    {
                        // Buffer full? Spin briefly.
                        std::this_thread::sleep_for(std::chrono::microseconds(1));
                    }
                }
                ticksSent_.fetch_add(1, std::memory_order_relaxed);
            }
        }
        std::cout << "Consumer Thread has stopped." << std::endl;
    }

    void monitorThread()
    {
        while (running_.load(std::memory_order_relaxed))
        {
            std::unique_lock<std::mutex> lock(CVMutex_);
            bool stopping = CVMonitor_.wait_for(lock, std::chrono::seconds(1), [this]
                                                { return !running_.load(std::memory_order_relaxed); });
            if (stopping)
                break;
            uint64_t genCount = ticksGenerated_.exchange(0, std::memory_order_relaxed);
            uint64_t sentCount = ticksSent_.exchange(0, std::memory_order_relaxed);
            std::cout << "[Metrics] Ticks / secs: Generated = " << genCount << ", Sent = " << sentCount << std::endl;
        }
    }
};

#endif // MARKET_DATA_SYSTEM_RW_NONBLOCKING_H