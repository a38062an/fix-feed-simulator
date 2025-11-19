#ifndef MARKET_DATA_SYSTEM_MARKET_DATA_SYSTEM_H
#define MARKET_DATA_SYSTEM_MARKET_DATA_SYSTEM_H

#include <vector>
#include <memory>
#include <thread> // For std::jthread (C++20)
#include <atomic>
#include <functional>
#include <string>
#include <iostream>
#include <chrono>
#include <format>
#include <mutex>

// --- Project Components ---
#include <market/price_generator.h>
#include <market/random_walk_generator.h>
#include <core/blocking_ring_buffer.h>
#include <network/udp_sender.h>
#include <fix/message.h>

// May have to refactor later to std::int32_t
using price = double;

struct MarketTick
{
    std::string symbol;
    price bid;
    price ask;
};

/**
 *
 *@brief Orchestrates the entire market data system
 *
 * This class owns all components (generators, queues, network senders)
 * and manages application threads.
 *
 */
class MarketDataSystem
{
public:
    MarketDataSystem()
    {
        generators_.push_back(std::make_unique<RandomWalkGenerator<price>>(100.0, 0.01));

        try
        {
            sender_ = std::make_unique<UDPMulticastSender>("239.255.1.1", 9999);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Could not initialise network sender: " << e.what() << std::endl;
        }

        std::cout << "MarketDataSystem initialised." << std::endl;
    }

    /**
     * @brief starts server
     * Local `std::jthread` objects would destruct and block (join) immediately
     * if we don't use private threads_ vector at the end of `start()`.
     * Storing them here allows them to run asynchronously until the system is destroyed.
     */
    void start()
    {
        std::cout << "Starting threads..." << std::endl;

        // --- Start Producer Thread ---
        // This thread generates ticks and pushes them to the queue
        threads_.emplace_back([this]
                              { producerThread(); });

        // --- Start Consumer Thread ---
        // This thread pops from the queue, encodes FIX, and sends
        threads_.emplace_back([this]
                              { consumerThread(); });

        // --- Start Monitor Thread ---
        // This thread prints metrics every second
        threads_.emplace_back([this]
                              { monitorThread(); });

        std::cout << "All threads running." << std::endl;
    }

    void stop()
    {
        std::cout << "Stopping system threads..." << std::endl;
        running_.store(false, std::memory_order_relaxed);

        // Call to instant shut down monitor thread
        CVMonitor_.notify_all();

        // This wakes up the consumer thread to kill itself
        MarketTick dummyTick;
        SPSCTickQueue_.push(dummyTick);
    }

    ~MarketDataSystem()
    {
        if (running_.load())
        {
            stop();
        }
        std::cout << "MarketDataSystem shutdown." << std::endl;
    }

private:
    // --- Components ---

    std::vector<std::unique_ptr<IPriceGenerator<price>>> generators_;
    BlockingRingBuffer<MarketTick, 4096> SPSCTickQueue_;
    std::unique_ptr<UDPMulticastSender> sender_;

    // --- Metrics ---

    // alignas prevents "false sharing" by putting each atomic on its own CPU cache line.
    // Avoids the threads having to ask for ownership of the cache line (CPU reads whole 64-byte blocks)
    alignas(64) std::atomic<uint64_t> ticksGenerated_{0};
    alignas(64) std::atomic<uint64_t> ticksSent_{0};

    // --- Thread Management ---
    std::vector<std::jthread> threads_;

    // Flag to signal thread to stop
    std::atomic<bool> running_{true};

    // Helper variables to get instant shutdown of monitor thread
    std::condition_variable CVMonitor_;
    std::mutex CVMutex_;

    // --- Thread Implementations ---

    /**
     *
     *@brief Producer
     *
     *  Generates price ticks and pushes them onto the queue
     *  Rates limit to not make cpu spin at 100% with sleep()
     */
    void producerThread()
    {
        std::cout << "Producer thread started" << std::endl;
        auto &generator = generators_[0]; // Get our only generator

        // Main work
        while (running_.load(std::memory_order_relaxed))
        {
            // No internal error check needed as it produces true (correct) data only

            // 1. Generate data
            price bidPrice = generator->getNextPrice();
            price askPrice = bidPrice + 0.25; // Spread
            MarketTick tick = {"ESZ5", bidPrice, askPrice};

            // 2. Push to queue
            SPSCTickQueue_.push(tick);

            // 3. Update metric
            ticksGenerated_.fetch_add(1, std::memory_order_relaxed);

            // 4. Rate limit
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << "Producer thread stopped (no more data generated)." << std::endl;
    }

    /**
     *
     * @brief Consumer
     *
     * Thread to pop ticks from queue, encode them and send them
     *
     */
    void consumerThread()
    {
        std::cout << "Consumer thread started" << std::endl;
        FIXMessage fixMessage("FIX.4.2");
        MarketTick tick;

        while (running_.load(std::memory_order_relaxed))
        {
            // 1. Pop from queue

            // Check if pop failed
            if (!SPSCTickQueue_.pop(tick))
            {
                continue;
            }

            // double check we aren't stopping (could execute on fail)
            if (!running_.load(std::memory_order_relaxed))
            {
                break;
            }

            // 2. Encode tick into fix message
            fixMessage.clearBody();

            // Header: MsgType=W (Snapshot), Symbol
            fixMessage.addField(35, "W")
                .addField(55, tick.symbol)
                .addField(268, "2"); // NoMDEntries = 2

            fixMessage.addField(269, "0") // Bid
                .addField(270, std::format("{:.2f}", tick.bid))
                .addField(271, "100"); // Size

            fixMessage.addField(269, "1") // Ask
                .addField(270, std::format("{:.2f}", tick.ask))
                .addField(271, "100"); // Size

            std::span<const uint8_t> completeMessage = fixMessage.finalize();

            // 3. Send over Network
            if (sender_)
            {
                sender_->send(completeMessage);
            }

            // Update Metric
            ticksSent_.fetch_add(1, std::memory_order_relaxed);
        }
        std::cout << "Consumer Thread has stopped." << std::endl;
    }

    /**
     *
     * @brief The Monitor
     *
     * Wakes up to print performance metrics;
     *
     */
    void monitorThread()
    {
        std::cout << "Monitor thread started." << std::endl;

        while (running_.load(std::memory_order_relaxed))
        {
            // 1. sleeping for some time or when stop() is called

            // Efficiently wait for 1s timeout OR immediate stop signal (0% CPU while waiting)
            // On stop() when thread is notified if lambda is returns true then we exit sleep and continue
            std::unique_lock<std::mutex> lock(CVMutex_);
            bool stopping = CVMonitor_.wait_for(lock, std::chrono::seconds(1),
                                                [this]
                                                { return !running_.load(std::memory_order_relaxed); });

            if (stopping)
            {
                break;
            }

            // 2. Load and Reset Metrics
            uint64_t genCount = ticksGenerated_.exchange(0, std::memory_order_relaxed);
            uint64_t sentCount = ticksSent_.exchange(0, std::memory_order_relaxed);

            // 3. Print Report
            std::cout << "[Metrics] Ticks / secs: "
                      << "Generated = " << genCount << ", "
                      << "Sent = " << sentCount << std::endl;
        }
        std::cout << "Monitor Thread has stopped." << std::endl;
    }
};

#endif // MARKET_DATA_SYSTEM_MARKET_DATA_SYSTEM_H
