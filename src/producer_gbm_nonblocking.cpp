#include <iostream>
#include <thread>
#include <chrono>

#include <market/market_data_system_gbm_nonblocking.h>
#include <csignal>
#include <atomic>

static std::atomic<bool> running{true};

extern "C" void signal_handler(int)
{
    running.store(false);
}

int main(int argc, char **argv)
{
    std::cout << "Starting MarketDataSystemNonBlocking (GBM)..." << std::endl;

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Use default IP/port for simplicity
    MarketDataSystemNonBlocking system;
    system.start();

    // Run until signalled to stop (Ctrl+C)
    while (running.load())
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "Stopping MarketDataSystemNonBlocking..." << std::endl;
    system.stop();
    // give threads a moment to shutdown cleanly
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << "Shutdown complete." << std::endl;
    return 0;
}
