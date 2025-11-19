#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>

#include <market/market_data_system_gbm.h>

std::atomic<bool> keepRunning{true};

void signalHandler(int signum)
{
    std::cout << "\nInterrupt signal (" << signum << ") received.\n";
    keepRunning = false;
}

int main()
{
    std::signal(SIGINT, signalHandler);
    try
    {
        MarketDataSystemGBM system;
        system.start();
        while (keepRunning)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        system.stop();
    }
    catch (const std::exception &e)
    {
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
