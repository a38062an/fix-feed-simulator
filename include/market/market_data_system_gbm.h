#ifndef MARKET_DATA_SYSTEM_MARKET_DATA_SYSTEM_GBM_H
#define MARKET_DATA_SYSTEM_MARKET_DATA_SYSTEM_GBM_H

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
#include <market/geometric_brownian_motion_generator.h>
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
 * Cannot stress test this system as GBM messes up
 * Limited production rate of prices
 *
 */
class MarketDataSystemGBM
{
public:
    MarketDataSystemGBM()
    {
        // GBM: (startPrice, mu, sigma, dt)
        generators_.push_back(std::make_unique<GBMGenerator<price>>(100.0, 0.1, 0.3, 0.001));

        try
        {
            sender_ = std::make_unique<UDPMulticastSender>("239.255.1.1", 9999);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Could not initialise network sender: " << e.what() << std::endl;
        }

        std::cout << "MarketDataSystemGBM initialised." << std::endl;
    }

    void start()
    {
        threads_.emplace_back([this]
                              { producerThread(); });
        threads_.emplace_back([this]
                              { consumerThread(); });
        threads_.emplace_back([this]
                              { monitorThread(); });
    }

    void stop()
    {
        running_.store(false, std::memory_order_relaxed);
        CVMonitor_.notify_all();
        MarketTick dummyTick;
        SPSCTickQueue_.push(dummyTick);
    }

    ~MarketDataSystemGBM()
    {
        if (running_.load())
            stop();
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
        std::cout << "Producer thread started (GBM model active with Mean-Reversion)." << std::endl;
        auto &generator = generators_[0];

        // --- Mean-Reversion Constants (Numerical Stability) ---
        // This is necessary because the price collapses due to floating point error accumulation.
        const price TARGET_PRICE = 100.0;
        const double REVERSION_STRENGTH = 0.00005;

        while (running_.load(std::memory_order_relaxed))
        {
            // 1. Generate new Price from GBM
            price midPrice = generator->getNextPrice();

            // --- MEAN-REVERSION FORCE ---
            // Calculate deviation from the target (100.0)
            double deviation = TARGET_PRICE - midPrice;
            // Apply a small push back to center the price.
            midPrice += deviation * REVERSION_STRENGTH;
            // --------------------------

            // 2. Dynamic Spread Calculation
            double spread = 0.05 + 0.01 * ((double)std::rand() / RAND_MAX);
            spread = std::round(spread * 100.0) / 100.0;

            price bidPrice = midPrice - spread / 2.0;
            price askPrice = midPrice + spread / 2.0;

            // Random volume between 50 and 150
            int volume = (std::rand() % 100) + 50;

            // Use the UPDATED MarketTick struct
            MarketTick tick = {"ESZ5", bidPrice, askPrice, volume, volume};

            // 2. Push to queue (Blocking if buffer is full)
            SPSCTickQueue_.push(tick);
            ticksGenerated_.fetch_add(1, std::memory_order_relaxed);

            std::this_thread::sleep_for(std::chrono::milliseconds(7));
        }
        std::cout << "Producer thread stopped (no more data generated)." << std::endl;
    }

    void consumerThread()
    {
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

#endif // MARKET_DATA_SYSTEM_MARKET_DATA_SYSTEM_GBM_H
