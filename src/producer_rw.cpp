#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>

// Include your Engine (Random Walk variant)
#include <market/market_data_system_rw.h>

// Global flag for Ctrl+C handling
std::atomic<bool> keepRunning{true};

// Ctrl+C handler for graceful shutdown
void signalHandler(int signum)
{
    std::cout << "\nInterrupt signal (" << signum << ") received.\n";
    keepRunning = false;
}

int main()
{
    // Register Ctrl+C handler
    std::signal(SIGINT, signalHandler);

    try
    {
        std::cout << "Initializing Market Data System (Random Walk)..." << std::endl;

        MarketDataSystemRW system;
        system.start();

        std::cout << "System running. Press Ctrl+C to stop." << std::endl;

        while (keepRunning)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        std::cout << "Shutting down..." << std::endl;
        system.stop();
    }
    catch (const std::exception &e)
    {
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
