#ifndef MARKET_DATA_SYSTEM_MARKET_DATA_SYSTEM_RW_H
#define MARKET_DATA_SYSTEM_MARKET_DATA_SYSTEM_RW_H

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
#include <core/blocking_ring_buffer.h>
#include <network/udp_sender.h>
#include <fix/message.h>

using price = double;

struct MarketTick
{
    std::string symbol;
    price bid;
    price ask;
    int bid_size;
    int ask_size;
};

/**
 *
 * Stress test possible as price generation is independent of generation speed
 * No sleep here
 *
 */
class MarketDataSystemRW
{
public:
    MarketDataSystemRW()
    {
        // Random walk: (startPrice, stepSize)
        generators_.push_back(std::make_unique<RandomWalkGenerator<price>>(100.0, 0.01));

        try
        {
            sender_ = std::make_unique<UDPMulticastSender>("239.255.1.1", 9999);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Could not initialise network sender: " << e.what() << std::endl;
        }

        std::cout << "MarketDataSystemRW initialised." << std::endl;
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
        CVMonitor_.notify_all();
        // Wake consumer
        MarketTick dummyTick;
        SPSCTickQueue_.push(dummyTick);
    }

    ~MarketDataSystemRW()
    {
        if (running_.load())
            stop();
        std::cout << "MarketDataSystemRW shutdown." << std::endl;
    }

    auto &getQueue() { return SPSCTickQueue_; }
    uint64_t getGeneratedCount() const { return ticksGenerated_.load(std::memory_order_relaxed); }
    uint64_t getSentCount() const { return ticksSent_.load(std::memory_order_relaxed); }

private:
    std::vector<std::unique_ptr<IPriceGenerator<price>>> generators_;
    BlockingRingBuffer<MarketTick, 4096> SPSCTickQueue_;
    std::unique_ptr<UDPMulticastSender> sender_;

    alignas(64) std::atomic<uint64_t> ticksGenerated_{0};
    alignas(64) std::atomic<uint64_t> ticksSent_{0};
    std::vector<std::jthread> threads_;
    std::atomic<bool> running_{true};
    std::condition_variable CVMonitor_;
    std::mutex CVMutex_;

    void producerThread()
    {
        std::cout << "Producer thread started (Random Walk model active)." << std::endl;
        auto &generator = generators_[0];
        while (running_.load(std::memory_order_relaxed))
        {
            price midPrice = generator->getNextPrice();
            double spread = 0.05 + 0.01 * ((double)std::rand() / RAND_MAX);
            spread = std::round(spread * 100.0) / 100.0;
            price bidPrice = midPrice - spread / 2.0;
            price askPrice = midPrice + spread / 2.0;
            int volume = (std::rand() % 100) + 50;
            MarketTick tick = {"ESZ5", bidPrice, askPrice, volume, volume};
            SPSCTickQueue_.push(tick);
            ticksGenerated_.fetch_add(1, std::memory_order_relaxed);
        }
        std::cout << "Producer thread stopped (no more data generated)." << std::endl;
    }

    void consumerThread()
    {
        std::cout << "Consumer thread started" << std::endl;
        FIXMessage fixMessage("FIX.4.2");
        MarketTick tick;
        while (running_.load(std::memory_order_relaxed))
        {
            if (!SPSCTickQueue_.pop(tick))
            {
                std::this_thread::yield();
                continue;
            }
            if (!running_.load(std::memory_order_relaxed))
                break;
            fixMessage.clearBody();
            fixMessage.addField(35, "W").addField(55, tick.symbol).addField(268, "2");
            fixMessage.addField(269, "0").addField(270, std::format("{:.2f}", tick.bid)).addField(271, std::to_string(tick.bid_size));
            fixMessage.addField(269, "1").addField(270, std::format("{:.2f}", tick.ask)).addField(271, std::to_string(tick.ask_size));
            std::span<const uint8_t> completeMessage = fixMessage.finalize();
            if (sender_)
            {
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
        std::cout << "Monitor thread started." << std::endl;
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
        std::cout << "Monitor Thread has stopped." << std::endl;
    }
};

#endif // MARKET_DATA_SYSTEM_MARKET_DATA_SYSTEM_RW_H
